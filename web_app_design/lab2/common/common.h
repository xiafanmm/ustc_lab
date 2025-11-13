#pragma once

#include <cstddef>

const int NAME_LEN = 32;     // 用户名最大长度
const int MSG_LEN  = 512;    // 消息最大长度

// 消息类型
enum MsgType {
    MSG_LOGIN    = 1,   // 用户登录广播
    MSG_LOGOUT   = 2,   // 用户退出广播
    MSG_BROADCAST= 3,   // 普通群发消息
    MSG_PRIVATE  = 4,   // 私聊消息
    MSG_SYSTEM   = 5    // 系统公告
};

// 固定长度的消息结构，用于 send/recv
struct ChatMessage {
    int  type;                         // MsgType
    char from[NAME_LEN];               // 发送方用户名
    char to[NAME_LEN];                 // 目标用户名（私聊用）
    char text[MSG_LEN];                // 文本内容
    int  online_count;                 // 当前在线人数（登录/退出时用）
};

// 发送 len 字节，直到发完或出错
bool send_all(int fd, const void* buffer, std::size_t len);

// 接收 len 字节，直到收完或出错
bool recv_all(int fd, void* buffer, std::size_t len);