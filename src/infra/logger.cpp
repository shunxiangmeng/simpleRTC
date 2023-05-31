#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <time.h>
#include <chrono>
#include "utils/time.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace infra {

ConsoleLogChannel::ConsoleLogChannel(const std::string &name) : LogChannel(name) {
}

void ConsoleLogChannel::write(const std::shared_ptr<LogContent> &content) {
    content_.push_back(content);
}

void ConsoleLogChannel::flush() {
    for (auto i = 0; i < content_.size(); i++) {
        setConsoleColour(content_[i]->level);
        std::cout << content_[i]->content;
    }
    content_.clear();
}

void ConsoleLogChannel::setConsoleColour(LogLevel level) {
#ifdef _WIN32
    #define CLEAR_COLOR 7
    static const WORD LOG_CONST_TABLE[][3] = {
        {0xC7, 0x0C , 'E'},  //红底灰字，黑底红字
        {0xE7, 0x0E , 'W'},  //黄底灰字，黑底黄字
        {0xB7, 0x0B , 'I'},  //天蓝底灰字，黑底天蓝字
        {0xA7, 0x0A , 'D'},  //绿底灰字，黑底绿字
        {0x97, 0x09 , 'T'}   //蓝底灰字，黑底蓝字，window console默认黑底  
    };
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == 0)
        return;
    SetConsoleTextAttribute(handle, LOG_CONST_TABLE[int(level)][1]);
#else
#define CLEAR_COLOR "\033[0m"
    static const char* LOG_CONST_TABLE[][3] = {
            {"\033[44;37m", "\033[34m", "T"},
            {"\033[42;37m", "\033[32m", "D"},
            {"\033[46;37m", "\033[36m", "I"},
            {"\033[43;37m", "\033[33m", "W"},
            {"\033[41;37m", "\033[31m", "E"} };
    std::cout << LOG_CONST_TABLE[int(level)][1];
#endif
}

Logger& Logger::instance() {
    static std::shared_ptr<LogChannel> console = std::make_shared<ConsoleLogChannel>();
    static std::shared_ptr<Logger> slogger = std::make_shared<Logger>(console, LogLevelTrace);
    return *slogger;
}

Logger::Logger(const std::shared_ptr<LogChannel>& channel, LogLevel level) : level_(level), running_(true) {
    addLogChannel(channel);
    thread_ = std::make_shared<std::thread>([this]() {run(); });
}

Logger::~Logger() {
    std::lock_guard<decltype(logChannelsMutex_)> lock(logChannelsMutex_);
    logChannels_.clear();
    running_ = false;
    if (thread_->joinable()) {
        thread_->join();
    }
}

void Logger::addLogChannel(const std::shared_ptr<LogChannel>& channel) {
    std::lock_guard<decltype(logChannelsMutex_)> lock(logChannelsMutex_);
    logChannels_.push_back(channel);
}

void Logger::flush() {
    std::lock_guard<decltype(logChannelsMutex_)> lock(logChannelsMutex_);
    for (auto it : logChannels_) {
        it->flush();
    }
}

void Logger::run() {
    while (running_) {
        semaphore_.wait();
        flush();
    }
}

std::string Logger::printTime() {
    uint64_t timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    uint64_t milli = timestamp + 8 * 60 * 60 * 1000;  //time zone
    auto mTime = std::chrono::milliseconds(milli);
    auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(mTime);
    struct tm now;
    auto tt = std::chrono::system_clock::to_time_t(tp);
    gmtime_s(&now, &tt);

    char buffer[64] = { 0 };
    snprintf(buffer, sizeof(buffer), "[%4d-%02d-%02d %02d:%02d:%02d.%03d]", 
        now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec, int(timestamp % 1000));
    return buffer;
}

void Logger::write(LogLevel level, const std::string &content) {
    std::lock_guard<decltype(logChannelsMutex_)> lock(logChannelsMutex_);
    for (auto it : logChannels_) {
        std::shared_ptr<LogContent> log(new LogContent(level, content));
        it->write(log);
    }
    semaphore_.notify();
}


static const char* sLogLevelString[] = {"[error]", "[warning]", "[info]", "[debug]", "[trace]"};

void Logger::printLog(LogLevel level, const char *file, int line, const char *fmt, ...) {
    if (level > level_) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    char buffer[1024] = { 0 };
    if (vsprintf_s(buffer, sizeof(buffer), fmt, ap) > 0) {
        std::string str;
        str += printTime();
        str += sLogLevelString[int(level)];
        str += std::string("[") + file + std::string(":") + std::to_string(line) + std::string("]");
        str += buffer;
        write(LogLevel(level), str);
    }
    va_end(ap);
}

}