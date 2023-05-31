#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <fstream>
#include "utils/semaphore.h"
#include "utils/utils.h"

namespace infra {

typedef enum {
    LogLevelError = 0,
    LogLevelWarn,
    LogLevelInfo,
    LogLevelDebug,
    LogLevelTrace
} LogLevel;

typedef struct LogContent_tag{
    LogLevel level;
    std::string content;
    LogContent_tag(LogLevel l, const std::string& c) : level(l), content(c) {}
}LogContent;

class LogChannel : public noncopyable {
public:
    LogChannel(const std::string &name) : name_(name) {};
    virtual ~LogChannel() { content_.clear(); };
    virtual void write(const std::shared_ptr<LogContent> &content) = 0;
    virtual void flush() = 0;
protected:    
    std::string name_;
    std::vector<std::shared_ptr<LogContent>> content_;
};

//控制台日志输出
class ConsoleLogChannel : public LogChannel {
public:
    ConsoleLogChannel(const std::string &name = "console");
    virtual ~ConsoleLogChannel() override = default;
    virtual void write(const std::shared_ptr<LogContent> &content) override;
    virtual void flush() override;
    void setConsoleColour(LogLevel level);
private:

};

//异步输出日志，线程安全
class Logger : public noncopyable {
public:
    static Logger &instance();
    Logger(const std::shared_ptr<LogChannel>& channel, LogLevel level = LogLevelInfo);
    ~Logger();
    void addLogChannel(const std::shared_ptr<LogChannel>& channel);
    void setLevel(LogLevel level);
    void printLog(LogLevel level, const char *file, int line, const char *fmt, ...);
private:
    void run();
    void flush();
    void write(LogLevel level, const std::string &content);
    std::string printTime();
private:
    LogLevel level_;
    bool running_;
    Semaphore semaphore_;
    std::shared_ptr<std::thread> thread_;
    std::vector<std::shared_ptr<LogChannel>> logChannels_;
    std::mutex logChannelsMutex_;
};

}


#define printLogf(level, ...) infra::Logger::instance().printLog(level, __FILE__, __LINE__, ##__VA_ARGS__)
#define errorf(...) printLogf(infra::LogLevelError, ##__VA_ARGS__)
#define warnf(...)  printLogf(infra::LogLevelWarn, ##__VA_ARGS__)
#define infof(...)  printLogf(infra::LogLevelInfo, ##__VA_ARGS__)
#define debugf(...) printLogf(infra::LogLevelDebug, ##__VA_ARGS__)
#define tracef(...) printLogf(infra::LogLevelTrace, ##__VA_ARGS__)
