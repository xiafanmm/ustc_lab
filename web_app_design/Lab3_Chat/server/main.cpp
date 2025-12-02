/*
 * Description: 聊天室服务端主程序，处理客户端连接与消息转发
 * Author: 夏凡
 * Create: 2025-12-02
 */

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <algorithm>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <cstdlib>
#include "../common/Protocol.h"

// 常量定义 
const int MAX_BUFFER_SIZE = 1024 * 10;
const int LISTEN_BACKLOG = 10;

struct ClientContext {
    int socketFd;
    std::string name;
};

// 全局状态 
std::vector<ClientContext> g_clients;
std::mutex g_clientsMutex;

// 文件传输路由表: SenderFD -> ReceiverFD (-1代表群发)
std::map<int, int> g_fileTransferRoutes;
std::mutex g_fileMutex;

// 辅助：读取固定长度 
bool RecvFixedLen(int sockfd, char *buf, int len)
{
    int total = 0;
    while (total < len) {
        int received = recv(sockfd, buf + total, len - total, 0);
        if (received <= 0) {
            return false;
        }
        total += received;
    }
    return true;
}

// 通用发送函数
void SendPacket(int fd, int type, const std::string &data)
{
    MsgHeader header;
    header.type = type;
    header.bodyLen = data.size();
    header.senderId = -1;
    send(fd, (char *)&header, sizeof(header), 0);
    if (!data.empty()) {
        send(fd, data.c_str(), data.size(), 0);
    }
}

// 广播消息
void BroadcastPacket(int type, const std::string &data, int excludeFd)
{
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    for (auto &cli : g_clients) {
        if (excludeFd == -1 || cli.socketFd != excludeFd) {
            SendPacket(cli.socketFd, type, data);
        }
    }
}

// 广播用户列表
void BroadcastUserList()
{
    std::string nameListStr;
    {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        for (size_t i = 0; i < g_clients.size(); ++i) {
            nameListStr += g_clients[i].name;
            if (i != g_clients.size() - 1) {
                nameListStr += ",";
            }
        }
    }
    
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    for (auto &cli : g_clients) {
        SendPacket(cli.socketFd, MSG_USER_LIST, nameListStr);
    }
}

// 处理登录 [cite: 389]
void HandleLogin(int clientFd, const std::string &data, std::string &clientName)
{
    clientName = data;
    std::cout << "登录: " << clientName << std::endl;
    {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        for (auto &c : g_clients) {
            if (c.socketFd == clientFd) {
                c.name = clientName;
            }
        }
    }
    std::string notify = "[系统]: " + clientName + " 加入了群聊";
    BroadcastPacket(MSG_CHAT_TEXT, notify, -1);
    BroadcastUserList();
}

// 处理私聊
void HandlePrivateChat(int clientFd, const std::string &clientName, const std::string &body)
{
    size_t splitPos = body.find('|');
    if (splitPos == std::string::npos) {
        return;
    }

    std::string targetName = body.substr(0, splitPos);
    std::string msgContent = body.substr(splitPos + 1);
    
    int targetFd = -1;
    {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        for (auto &cli : g_clients) {
            if (cli.name == targetName) {
                targetFd = cli.socketFd;
                break;
            }
        }
    }

    if (targetFd != -1) {
        SendPacket(targetFd, MSG_CHAT_PRIVATE, "(私聊) " + clientName + ": " + msgContent);
        SendPacket(clientFd, MSG_CHAT_PRIVATE, "(私聊) 我 -> " + targetName + ": " + msgContent);
    } else {
        SendPacket(clientFd, MSG_CHAT_TEXT, "[系统]: 用户不存在");
    }
}

// 处理文件信息头
void HandleFileInfo(int clientFd, const std::string &body)
{
    size_t firstPipe = body.find('|');
    if (firstPipe == std::string::npos) {
        return;
    }

    std::string targetName = body.substr(0, firstPipe);
    std::string restInfo = body.substr(firstPipe + 1);

    int targetFd = -1; // -1代表群发
    if (!targetName.empty()) {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        for (auto &cli : g_clients) {
            if (cli.name == targetName) {
                targetFd = cli.socketFd;
                break;
            }
        }
        if (targetFd == -1) {
            SendPacket(clientFd, MSG_CHAT_TEXT, "[系统]: 目标不在线，文件取消");
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_fileMutex);
        g_fileTransferRoutes[clientFd] = targetFd;
    }

    if (targetFd == -1) {
        BroadcastPacket(MSG_FILE_INFO, restInfo, clientFd);
    } else {
        SendPacket(targetFd, MSG_FILE_INFO, restInfo);
    }
}

// 处理客户端逻辑
void HandleClient(int clientFd)
{
    char buffer[MAX_BUFFER_SIZE];
    std::string clientName = "Unknown";

    while (true) {
        MsgHeader header;
        if (!RecvFixedLen(clientFd, (char *)&header, sizeof(MsgHeader))) {
            break;
        }
        if (header.bodyLen > sizeof(buffer) || header.bodyLen < 0) {
            break;
        }
        if (header.bodyLen > 0) {
            if (!RecvFixedLen(clientFd, buffer, header.bodyLen)) {
                break;
            }
        }

        std::string body(buffer, header.bodyLen);
        if (header.type == MSG_LOGIN) {
            HandleLogin(clientFd, body, clientName);
        } else if (header.type == MSG_CHAT_TEXT) {
            BroadcastPacket(MSG_CHAT_TEXT, body, clientFd);
        } else if (header.type == MSG_CHAT_PRIVATE) {
            HandlePrivateChat(clientFd, clientName, body);
        } else if (header.type == MSG_FILE_INFO) {
            HandleFileInfo(clientFd, body);
        } else if (header.type == MSG_FILE_DATA) {
            // 文件数据块直接转发，不解包字符串
            int targetFd = -2;
            {
                std::lock_guard<std::mutex> lock(g_fileMutex);
                if (g_fileTransferRoutes.count(clientFd)) {
                    targetFd = g_fileTransferRoutes[clientFd];
                }
            }
            if (targetFd == -1) {
                BroadcastPacket(MSG_FILE_DATA, body, clientFd);
            } else if (targetFd >= 0) {
                // 发送头和体
                send(targetFd, (char *)&header, sizeof(header), 0);
                send(targetFd, buffer, header.bodyLen, 0);
            }
        } else if (header.type == MSG_LOGOUT) {
            break;
        }
    }

    // 清理资源
    close(clientFd);
    {
        std::lock_guard<std::mutex> lock(g_fileMutex);
        g_fileTransferRoutes.erase(clientFd);
    }
    {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        g_clients.erase(std::remove_if(g_clients.begin(), g_clients.end(), 
            [clientFd](const ClientContext &c){ return c.socketFd == clientFd; }), 
            g_clients.end());
    }
    
    if (clientName != "Unknown") {
        std::string notify = "[系统]: " + clientName + " 离开了群聊";
        BroadcastPacket(MSG_CHAT_TEXT, notify, -1);
        BroadcastUserList();
    }
}

void AdminConsole()
{
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        if (input.empty()) {
            continue;
        }
        std::string msg = "[系统公告]: " + input;
        BroadcastPacket(MSG_CHAT_TEXT, msg, -1);
    }
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        perror("Socket failed");
        return -1;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(serverFd, (sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Bind failed");
        return -1;
    }
    
    if (listen(serverFd, LISTEN_BACKLOG) == -1) {
        perror("Listen failed");
        return -1;
    }
    
    std::cout << "----------------------------------------" << std::endl;
    std::cout << " Server started on port " << port << std::endl;
    std::thread(AdminConsole).detach();

    while (true) {
        sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientFd = accept(serverFd, (sockaddr *)&clientAddr, &len);
        if (clientFd != -1) {
            {
                std::lock_guard<std::mutex> lock(g_clientsMutex);
                g_clients.push_back({clientFd, "Unknown"});
            }
            std::thread(HandleClient, clientFd).detach();
        }
    }
    close(serverFd);
    return 0;
}