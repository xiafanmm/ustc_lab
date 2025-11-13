#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include <iostream>
#include <string>
#include <cstring>

#include "common.h"

int g_sock = -1;
bool g_running = true;

// 接收线程：不停从服务器读消息并打印
void* recv_thread(void* arg) {
    (void)arg;
    ChatMessage msg{};
    while (g_running) {
        if (!recv_all(g_sock, &msg, sizeof(msg))) {
            std::cout << "[INFO] Disconnected from server." << std::endl;
            g_running = false;
            break;
        }

        switch (msg.type) {
            case MSG_LOGIN:
                std::cout << "[SYSTEM] " << msg.text
                          << " Online: " << msg.online_count << std::endl;
                break;
            case MSG_LOGOUT:
                std::cout << "[SYSTEM] " << msg.text
                          << " Online: " << msg.online_count << std::endl;
                break;
            case MSG_BROADCAST:
                std::cout << "[" << msg.from << "] " << msg.text << std::endl;
                break;
            case MSG_PRIVATE:
                std::cout << "[PRIVATE from " << msg.from << "] "
                          << msg.text << std::endl;
                break;
            case MSG_SYSTEM:
                std::cout << "[SYSTEM] " << msg.text << std::endl;
                break;
            default:
                break;
        }
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int         port      = 5555;

    if (argc >= 2) server_ip = argv[1];
    if (argc >= 3) port = std::atoi(argv[2]);

    g_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &srv.sin_addr) <= 0) {
        std::cerr << "Invalid server IP\n";
        return 1;
    }

    if (connect(g_sock, (sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("connect");
        return 1;
    }

    std::cout << "Connected to " << server_ip << ":" << port << std::endl;

    // 输入用户名
    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);
    if (username.empty()) username = "anonymous";

    // 发送登录消息
    ChatMessage login{};
    login.type = MSG_LOGIN;
    std::strncpy(login.from, username.c_str(), NAME_LEN - 1);
    std::snprintf(login.text, MSG_LEN, "%s joined", username.c_str());
    if (!send_all(g_sock, &login, sizeof(login))) {
        std::cerr << "Failed to send login message\n";
        close(g_sock);
        return 1;
    }

    // 启动接收线程
    pthread_t tid;
    pthread_create(&tid, nullptr, recv_thread, nullptr);

    std::cout << "Usage:\n"
              << "  普通群聊: 直接输入内容回车\n"
              << "  私聊:     @用户名 内容\n"
              << "  退出:     /quit\n";

    // 主线程：负责读取用户输入并发送
    std::string line;
    while (g_running && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        if (line == "/quit") {
            ChatMessage logout{};
            logout.type = MSG_LOGOUT;
            std::strncpy(logout.from, username.c_str(), NAME_LEN - 1);
            send_all(g_sock, &logout, sizeof(logout));
            g_running = false;
            break;
        }

        ChatMessage msg{};
        std::strncpy(msg.from, username.c_str(), NAME_LEN - 1);

        if (line[0] == '@') {
            // 私聊：格式 @用户名 内容
            auto pos = line.find(' ');
            if (pos == std::string::npos || pos <= 1) {
                std::cout << "[INFO] 私聊格式: @用户名 内容\n";
                continue;
            }
            std::string to   = line.substr(1, pos - 1);
            std::string text = line.substr(pos + 1);

            msg.type = MSG_PRIVATE;
            std::strncpy(msg.to, to.c_str(), NAME_LEN - 1);
            std::strncpy(msg.text, text.c_str(), MSG_LEN - 1);
        } else {
            // 普通群发
            msg.type = MSG_BROADCAST;
            std::strncpy(msg.text, line.c_str(), MSG_LEN - 1);
        }

        if (!send_all(g_sock, &msg, sizeof(msg))) {
            std::cout << "[ERROR] Failed to send. Maybe server is down.\n";
            g_running = false;
            break;
        }
    }

    close(g_sock);
    pthread_join(tid, nullptr);

    std::cout << "Client exit.\n";
    return 0;
}