#ifndef LOG_H
#define LOG_H

#include "http_parser.h"
#include <string>
#include <iostream>
#include <fstream>

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

    void LogAcess(LogLevel Level, const HttpRequest Request, const char *Response);

    void LogError(LogLevel Level, std::string ErrorInfo);

private:
    int fileDescriptor_;
    std::string LogFile_;
    LogLevel LogLevel_ = INFO; // 默认日志级别为 INFO

    // 辅助函数，用于获取文件锁
    bool LockFile();

    // 辅助函数，用于释放文件锁
    bool UnlockFile();
};

#endif