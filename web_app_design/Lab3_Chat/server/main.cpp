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
#include <cstdlib> // [新增] 用于 atoi
#include "../common/Protocol.h"

struct ClientContext {
    int socketFd;
    std::string name;
};

// 全局状态
std::vector<ClientContext> clients;
std::mutex clientsMutex;

// 文件传输路由表: SenderFD -> ReceiverFD (-1代表群发)
std::map<int, int> fileTransferRoutes;
std::mutex fileMutex;

// 辅助：读取固定长度
bool recvFixedLen(int sockfd, char* buf, int len) {
    int total = 0;
    while (total < len) {
        int received = recv(sockfd, buf + total, len - total, 0);
        if (received <= 0) return false;
        total += received;
    }
    return true;
}

// 通用发送函数
void sendPacket(int fd, int type, const std::string& data) {
    MsgHeader header;
    header.type = type;
    header.bodyLen = data.size();
    header.senderId = -1;
    send(fd, (char*)&header, sizeof(header), 0);
    if (!data.empty()) {
        send(fd, data.c_str(), data.size(), 0);
    }
}

// 广播消息
void broadcastPacket(int type, const std::string& data, int excludeFd) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& cli : clients) {
        if (excludeFd == -1 || cli.socketFd != excludeFd) {
            sendPacket(cli.socketFd, type, data);
        }
    }
}

// 广播用户列表
void broadcastUserList() {
    std::string nameListStr;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (size_t i = 0; i < clients.size(); ++i) {
            nameListStr += clients[i].name;
            if (i != clients.size() - 1) nameListStr += ",";
        }
    }
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& cli : clients) {
        sendPacket(cli.socketFd, MSG_USER_LIST, nameListStr);
    }
}

void handleClient(int clientFd) {
    char buffer[1024 * 10]; // 10KB Buffer
    std::string clientName = "Unknown";

    while (true) {
        MsgHeader header;
        // 1. 读头
        if (!recvFixedLen(clientFd, (char*)&header, sizeof(MsgHeader))) break;
        // 2. 读体
        if (header.bodyLen > sizeof(buffer)) break;
        if (header.bodyLen > 0) {
            if (!recvFixedLen(clientFd, buffer, header.bodyLen)) break;
        }

        // --- 业务逻辑 ---
        if (header.type == MSG_LOGIN) {
            clientName = std::string(buffer, header.bodyLen);
            std::cout << "登录: " << clientName << std::endl;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for(auto &c : clients) { if(c.socketFd == clientFd) c.name = clientName; }
            }
            std::string notify = "[系统]: " + clientName + " 加入了群聊";
            broadcastPacket(MSG_CHAT_TEXT, notify, -1);
            broadcastUserList();
        }
        else if (header.type == MSG_CHAT_TEXT) {
            broadcastPacket(MSG_CHAT_TEXT, std::string(buffer, header.bodyLen), clientFd);
        }
        else if (header.type == MSG_CHAT_PRIVATE) {
            std::string body(buffer, header.bodyLen);
            size_t splitPos = body.find('|');
            if (splitPos != std::string::npos) {
                std::string targetName = body.substr(0, splitPos);
                std::string msgContent = body.substr(splitPos + 1);
                
                int targetFd = -1;
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    for (auto& cli : clients) {
                        if (cli.name == targetName) { targetFd = cli.socketFd; break; }
                    }
                }
                if (targetFd != -1) {
                    sendPacket(targetFd, MSG_CHAT_PRIVATE, "(私聊) " + clientName + ": " + msgContent);
                    sendPacket(clientFd, MSG_CHAT_PRIVATE, "(私聊) 我 -> " + targetName + ": " + msgContent);
                } else {
                    sendPacket(clientFd, MSG_CHAT_TEXT, "[系统]: 用户不存在");
                }
            }
        }
        else if (header.type == MSG_FILE_INFO) {
            // 格式: TargetName|FileName|Size
            std::string body(buffer, header.bodyLen);
            size_t firstPipe = body.find('|');
            if (firstPipe != std::string::npos) {
                std::string targetName = body.substr(0, firstPipe);
                std::string restInfo = body.substr(firstPipe + 1);

                int targetFd = -1; // -1=群发
                if (!targetName.empty()) {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    for (auto& cli : clients) {
                        if (cli.name == targetName) { targetFd = cli.socketFd; break; }
                    }
                    if (targetFd == -1) {
                        sendPacket(clientFd, MSG_CHAT_TEXT, "[系统]: 目标不在线，文件取消");
                        continue;
                    }
                }

                // 记录路由
                {
                    std::lock_guard<std::mutex> lock(fileMutex);
                    fileTransferRoutes[clientFd] = targetFd;
                }

                MsgHeader fwdHeader = header;
                fwdHeader.bodyLen = restInfo.size();
                
                if (targetFd == -1) {
                    broadcastPacket(MSG_FILE_INFO, restInfo, clientFd);
                } else {
                    send(targetFd, (char*)&fwdHeader, sizeof(fwdHeader), 0);
                    send(targetFd, restInfo.c_str(), restInfo.size(), 0);
                }
            }
        }
        else if (header.type == MSG_FILE_DATA) {
            int targetFd = -2;
            {
                std::lock_guard<std::mutex> lock(fileMutex);
                if (fileTransferRoutes.count(clientFd)) {
                    targetFd = fileTransferRoutes[clientFd];
                }
            }

            if (targetFd == -1) {
                broadcastPacket(MSG_FILE_DATA, std::string(buffer, header.bodyLen), clientFd);
            } else if (targetFd >= 0) {
                send(targetFd, (char*)&header, sizeof(header), 0);
                send(targetFd, buffer, header.bodyLen, 0);
            }
        }
        else if (header.type == MSG_LOGOUT) {
            break;
        }
    }

    // 清理
    {
        std::lock_guard<std::mutex> lock(fileMutex);
        fileTransferRoutes.erase(clientFd);
    }
    close(clientFd);
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(), 
            [clientFd](const ClientContext& c){ return c.socketFd == clientFd; }), 
            clients.end());
    }
    
    if (clientName != "Unknown") {
        std::string notify = "[系统]: " + clientName + " 离开了群聊";
        broadcastPacket(MSG_CHAT_TEXT, notify, -1);
        broadcastUserList();
    }
}

void adminConsole() {
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        if (input.empty()) continue;
        std::string msg = "[系统公告]: " + input;
        broadcastPacket(MSG_CHAT_TEXT, msg, -1);
    }
}

// [修改] 支持命令行参数指定端口
int main(int argc, char* argv[]) {
    int port = 8888; // 默认端口
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) { perror("Socket failed"); return -1; }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if(bind(serverFd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Bind failed");
        return -1;
    }
    
    if (listen(serverFd, 10) == -1) {
        perror("Listen failed");
        return -1;
    }
    
    std::cout << "----------------------------------------" << std::endl;
    std::cout << " Server started on port " << port << std::endl;
    std::cout << " Usage: ./chat_server [port]" << std::endl;
    std::cout << " Type message here to broadcast system notice." << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::thread(adminConsole).detach();

    while (true) {
        sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &len);
        if (clientFd != -1) {
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients.push_back({clientFd, "Unknown"});
            }
            std::thread(handleClient, clientFd).detach();
        }
    }
    close(serverFd);
    return 0;
}