/*
 * Description: 定义通信协议、消息类型及包头结构
 * Author: 夏凡
 * Create: 2025-12-02
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

// 默认端口和缓冲区配置
const int DEFAULT_PORT = 8888;
const int FILE_CHUNK_SIZE = 4096;

// 消息类型枚举
enum MsgType {
    MSG_LOGIN = 1,       // 登录
    MSG_CHAT_TEXT,       // 文本消息 (群聊/系统)
    MSG_CHAT_PRIVATE,    // 私聊消息
    MSG_FILE_INFO,       // 文件头 (格式: Target|Name|Size)
    MSG_FILE_DATA,       // 文件内容
    MSG_FILE_END,        // 文件结束
    MSG_LOGOUT,          // 退出
    MSG_USER_LIST        // 用户列表
};

// 固定包头 (12字节)
struct MsgHeader {
    int32_t type;
    int32_t bodyLen;
    int32_t senderId;
};

#endif