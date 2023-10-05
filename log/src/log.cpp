#include "log.h"
#include "http_parser.h"
#include <string>
#include <iostream>
#include <fstream>
#include <mutex>
#include <ctime>

Logger::Logger(const std::string &LogFile)
{
    LogFileStream_.open(LogFile, std::ios::app);
}

Logger::~Logger()
{
    CloseResource();
}

void Logger::LogAcess(LogLevel Level, const HttpRequest Request, const char *Response)
{
    // 在写入日志前加锁
    std::unique_lock<std::mutex> lock(LogMutex_);

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

    LogFileStream_ << "[" << timestamp << "] [" << levelStr << "] " << Request.method << " " << Request.url << std::endl << "\t\t\t";
    std::cout << "[" << timestamp << "] [" << levelStr << "] " << Request.method << " " << Request.url << std::endl;
    for (const auto it : Request.headers)
    {
        LogFileStream_ << it.first << " : " << it.second << " | ";
    }
    LogFileStream_ << std::endl;

    int StatusCode = GetHttpResponseStatusCode(Response);

    LogFileStream_ << "\t\t\tHttp response status code: " << StatusCode << std::endl;

    LogFileStream_.flush();
}

void Logger::LogError(LogLevel Level, std::string ErrorInfo)
{
    // 在写入日志前加锁
    std::unique_lock<std::mutex> lock(LogMutex_);

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

    LogFileStream_ << "[" << timestamp << "] [" << levelStr << "] " << ErrorInfo << std::endl;
    std::cerr << "[" << timestamp << "] [" << levelStr << "] " << ErrorInfo << std::endl;

    LogFileStream_.flush();
}