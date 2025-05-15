#pragma once

// 定义文件传输消息类型
enum class MessageType {
    kFileInfo, // 文件信息
    kFileData, // 文件数据
    kComplete, // 传输完成
    kError,    // 错误

    // 分块传输新增消息类型
    kFileStart,    // 大文件传输开始
    kFileChunk,    // 文件数据块
    kFileChunkAck, // 块确认
    kFileEnd       // 大文件传输结束
};