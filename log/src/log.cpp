#include "log.h"
#include "http_parser.h"
#include <string>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <ctime>
#include <cstdio>

Logger::Logger(const std::string &LogFile) : LogFile_(LogFile), fileDescriptor_(-1) {}

Logger::~Logger() {}

void Logger::LogAcess(LogLevel Level, const HttpRequest Request, const char *Response)
{
    fileDescriptor_ = open(LogFile_.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fileDescriptor_ == -1)
    {
        std::cerr << "Failed to open log file." << std::endl;
    }

    // 在写入日志前加锁
    while (!LockFile());

    std::string levelStr;
    switch (Level)
    {
    case DEBUG:
        levelStr = "DEBUG";
        break;
    case INFO:
        levelStr = "INFO";
        break;
    case WARNING:
        levelStr = "WARNING";
        break;
    case ERROR:
        levelStr = "ERROR";
        break;
    case CRITICAL:
        levelStr = "CRITICAL";
        break;
    }

    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);

    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

    std::string logMessage = "[" + std::string(timestamp) + "] [" + levelStr + "] " + Request.method + " " + Request.url + "\n\t\t\t";

    // 使用 write 函数向文件描述符 fileDescriptor_ 写入数据
    if (write(fileDescriptor_, logMessage.c_str(), logMessage.size()) == -1)
    {
        // 处理写入失败的情况
        std::cerr << "Failed to write to file descriptor." << std::endl;
    }

    // 写入请求头
    for (const auto it : Request.headers)
    {
        logMessage = it.first + " : " + it.second + " | ";
        if (write(fileDescriptor_, logMessage.c_str(), logMessage.size()) == -1)
        {
            // 处理写入失败的情况
            std::cerr << "Failed to write to file descriptor." << std::endl;
        }
    }

    // 写入响应状态码
    int StatusCode = GetHttpResponseStatusCode(Response);
    logMessage = "\n\t\t\tHttp response status code: " + std::to_string(StatusCode) + "\n";

    if (write(fileDescriptor_, logMessage.c_str(), logMessage.size()) == -1)
    {
        // 处理写入失败的情况
        std::cerr << "Failed to write to file descriptor." << std::endl;
    }

    // 释放锁
    UnlockFile();

    if (fileDescriptor_ != -1)
    {
        close(fileDescriptor_);
        fileDescriptor_ = -1;
    }
}

void Logger::LogError(LogLevel Level, std::string ErrorInfo)
{
    fileDescriptor_ = open(LogFile_.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fileDescriptor_ == -1)
    {
        std::cerr << "Failed to open log file." << std::endl;
    }

    // 在写入日志前加锁
    while (!LockFile());

    std::string levelStr;
    switch (Level)
    {
    case DEBUG:
        levelStr = "DEBUG";
        break;
    case INFO:
        levelStr = "INFO";
        break;
    case WARNING:
        levelStr = "WARNING";
        break;
    case ERROR:
        levelStr = "ERROR";
        break;
    case CRITICAL:
        levelStr = "CRITICAL";
        break;
    }

    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);

    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

    // 使用文件描述符写入数据
    std::string logMessage = "[" + std::string(timestamp) + "] [" + levelStr + "] " + ErrorInfo + "\n";
    if (write(fileDescriptor_, logMessage.c_str(), logMessage.size()) == -1)
    {
        // 处理写入失败的情况
        std::cerr << "Failed to write to file descriptor." << std::endl;
    }
    std::cerr << logMessage;

    // 释放锁
    UnlockFile();

    if (fileDescriptor_ != -1)
    {
        close(fileDescriptor_);
        fileDescriptor_ = -1;
    }
}

bool Logger::LockFile()
{
    struct flock lock;
    lock.l_type = F_WRLCK; // 写入锁
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // 锁住整个文件

    return fcntl(fileDescriptor_, F_SETLK, &lock) != -1;
}

bool Logger::UnlockFile()
{
    struct flock lock;
    lock.l_type = F_UNLCK; // 解锁
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // 解锁整个文件

    return fcntl(fileDescriptor_, F_SETLK, &lock) != -1;
}