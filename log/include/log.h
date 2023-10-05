#ifndef LOG_H
#define LOG_H

#include "http_parser.h"
#include <string>
#include <iostream>
#include <fstream>
#include <mutex>

enum LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger
{
public:
    Logger() {}
    Logger(const std::string &LogFile);

    ~Logger();

    // 自定义赋值运算符重载
    Logger &operator=(Logger &other) noexcept
    {
        if (this != &other)
        { // 检查是否尝试自我赋值
            // 在赋值前，首先释放本对象的资源（如果有的话）
            CloseResource(); // 假设这是关闭资源的函数

            // 然后进行资源的移动
            LogFileStream_ = std::move(other.LogFileStream_); // 假设 LogFileStream_ 是需要转移的资源
            LogLevel_ = other.LogLevel_;

            // 清空 other 对象，确保它不再引用资源
            other.LogFileStream_.close(); // 关闭文件（如果已打开）
            other.LogLevel_ = INFO;       // 重置 LogLevel
        }
        return *this;
    }

    void LogAcess(LogLevel Level, const HttpRequest Request, const char *Response);

    void LogError(LogLevel Level, std::string ErrorInfo);

private:
    std::ofstream LogFileStream_;
    LogLevel LogLevel_ = INFO; // 默认日志级别为INFO
    std::mutex LogMutex_;

    // 辅助函数，用于释放资源
    void CloseResource()
    {
        LogFileStream_.close(); // 关闭文件（如果已打开）
    }
};

#endif