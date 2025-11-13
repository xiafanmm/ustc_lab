#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <cstring>  // memset, strncpy
#include <cstdint>  // intptr_t
#include <cstdlib>  // std::exit

#include "common.h"

struct ClientInfo {
    int         fd;
    std::string name;
};

std::vector<ClientInfo> g_clients;
std::mutex               g_clients_mutex;

// ====================== 工具函数：发送 / 广播 ======================

bool send_to_client(int fd, const ChatMessage& msg) {
    return send_all(fd, &msg, sizeof(msg));
}

void broadcast_message(const ChatMessage& msg) {
    std::lock_guard<std::mutex> lock(g_clients_mutex);
    for (auto& c : g_clients) {
        send_to_client(c.fd, msg);
    }
}

void remove_client_by_fd(int fd) {
    std::lock_guard<std::mutex> lock(g_clients_mutex);
    for (auto it = g_clients.begin(); it != g_clients.end(); ++it) {
        if (it->fd == fd) {
            g_clients.erase(it);
            break;
        }
    }
}

// ====================== 控制台线程：系统公告 / 关闭服务器 ======================

void* console_thread(void* arg) {
    (void)arg;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        if (line == "/quit") {
            // 先广播一条服务器关闭通知
            ChatMessage sys{};
            sys.type = MSG_SYSTEM;
            std::strncpy(sys.from, "SERVER", NAME_LEN - 1);
            std::snprintf(sys.text, MSG_LEN,
                          "Server is shutting down.");
            broadcast_message(sys);

            std::cout << "[SYSTEM] Server is shutting down..." << std::endl;

            // 直接结束整个进程（所有线程一起退出）
            std::exit(0);
        }

        // 普通系统公告
        ChatMessage sys{};
        sys.type = MSG_SYSTEM;
        std::strncpy(sys.from, "SERVER", NAME_LEN - 1);
        std::strncpy(sys.text, line.c_str(), MSG_LEN - 1);

        broadcast_message(sys);
        std::cout << "[SYSTEM] broadcast: " << line << std::endl;
    }

    // 如果 stdin EOF（比如输入被关掉），就直接让线程结束
    return nullptr;
}

// ====================== 客户端处理线程 ======================

void* client_thread(void* arg) {
    int client_fd = (int)(intptr_t)arg;
    ChatMessage msg{};

    // 第一次收到的应为登录消息
    if (!recv_all(client_fd, &msg, sizeof(msg)) || msg.type != MSG_LOGIN) {
        close(client_fd);
        return nullptr;
    }

    std::string username = msg.from;

    // 添加到在线列表
    int online_count = 0;
    {
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        g_clients.push_back(ClientInfo{client_fd, username});
        online_count = (int)g_clients.size();
    }

    // 广播上线消息
    ChatMessage login_msg{};
    login_msg.type = MSG_LOGIN;
    std::strncpy(login_msg.from, username.c_str(), NAME_LEN - 1);
    login_msg.online_count = online_count;
    std::snprintf(login_msg.text, MSG_LEN,
                  "%s joined the chat.", username.c_str());
    broadcast_message(login_msg);

    std::cout << "[INFO] user '" << username
              << "' connected, fd=" << client_fd
              << ", online=" << online_count << std::endl;

    // 循环接收该客户端的后续消息
    while (true) {
        ChatMessage incoming{};
        if (!recv_all(client_fd, &incoming, sizeof(incoming))) {
            // 客户端断开或错误
            break;
        }

        if (incoming.type == MSG_BROADCAST) {
            // 群聊
            broadcast_message(incoming);
            std::cout << "[BROADCAST] from " << incoming.from
                      << ": " << incoming.text << std::endl;
        } else if (incoming.type == MSG_PRIVATE) {
            // 私聊
            std::string target = incoming.to;
            bool found = false;

            {
                std::lock_guard<std::mutex> lock(g_clients_mutex);
                for (auto& c : g_clients) {
                    if (c.name == target) {
                        send_to_client(c.fd, incoming);
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                ChatMessage sys{};
                sys.type = MSG_SYSTEM;
                std::strncpy(sys.from, "SERVER", NAME_LEN - 1);
                std::snprintf(sys.text, MSG_LEN,
                              "User '%s' not found or not online.", target.c_str());
                send_to_client(client_fd, sys);
            } else {
                std::cout << "[PRIVATE] " << incoming.from << " -> "
                          << incoming.to << ": " << incoming.text << std::endl;
            }
        } else if (incoming.type == MSG_LOGOUT) {
            // 客户端主动退出
            break;
        }
    }

    // 处理退出
    remove_client_by_fd(client_fd);

    int left_count = 0;
    {
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        left_count = (int)g_clients.size();
    }

    ChatMessage logout_msg{};
    logout_msg.type = MSG_LOGOUT;
    std::strncpy(logout_msg.from, username.c_str(), NAME_LEN - 1);
    logout_msg.online_count = left_count;
    std::snprintf(logout_msg.text, MSG_LEN,
                  "%s left the chat.", username.c_str());
    broadcast_message(logout_msg);

    std::cout << "[INFO] user '" << username
              << "' disconnected, online=" << left_count << std::endl;

    close(client_fd);
    return nullptr;
}

// ====================== 主函数：监听 + 接收连接 ======================

int main(int argc, char* argv[]) {
    int port = 5555;
    if (argc >= 2) {
        port = std::atoi(argv[1]);
    }

    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;       // 监听所有 IP
    addr.sin_port        = htons(port);

    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 16) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    std::cout << "Server listening on port " << port
              << " (Ctrl+C or /quit to stop)" << std::endl;

    // 启动控制台线程：负责发送系统公告 & /quit 关闭服务器
    pthread_t console_tid;
    pthread_create(&console_tid, nullptr, console_thread, nullptr);
    pthread_detach(console_tid);  // 不 join，进程结束时一起回收

    // 主线程：只负责接收连接
    while (true) {
        sockaddr_in cli_addr{};
        socklen_t   cli_len = sizeof(cli_addr);
        int client_fd = ::accept(listen_fd, (sockaddr*)&cli_addr, &cli_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, nullptr, client_thread, (void*)(intptr_t)client_fd);
        pthread_detach(tid);  // 不用 join，由系统回收线程资源
    }

    close(listen_fd);
    return 0;
}