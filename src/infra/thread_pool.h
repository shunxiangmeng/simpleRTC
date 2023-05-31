#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include "task_queue.h"
#include "event_driver.h"


namespace infra {

class ThreadPool : public TaskQueue {
public:

    enum Priority {
        PRIORITY_LOW = 0,
        PRIORITY_NORMAL,
        PRIORIYY_HIGH
    };

    ThreadPool(const ThreadPool&) = delete; 

    ThreadPool(ThreadPool&&) = delete;

    static std::shared_ptr<ThreadPool> create(const std::string& name, int32_t threadCount = 1, Priority priority = PRIORITY_NORMAL);

    virtual void postTask(std::function<void()> task) override;

    ~ThreadPool();

private:

    ThreadPool(const std::string& name, int32_t threadCount = 1, Priority priority = PRIORITY_NORMAL);

    bool start();  //启动

    void stop();

    void run(int32_t index);


private:

    std::string name_;
    int32_t threadCount_;
    Priority priority_;

    bool running;

    std::shared_ptr<EventDriver> event_driver_;

    std::unordered_map<std::thread::id, std::shared_ptr<std::thread>> threads_;
};


}