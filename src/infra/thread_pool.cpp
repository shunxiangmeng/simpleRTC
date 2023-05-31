#include "thread_pool.h"
#include <chrono>
#include "event_driver.h"
#include "logger.h"

namespace infra {

ThreadPool::ThreadPool(const std::string& name, int32_t threadCount, Priority priority)
    : name_(name), threadCount_(threadCount), priority_(priority), running(false) {

    event_driver_ = std::make_shared<EventDriver>();
}

ThreadPool::~ThreadPool() {
    stop();
}

std::shared_ptr<ThreadPool> ThreadPool::create(const std::string& name, int32_t threadCount, Priority priority) {
    
    if (threadCount < 1) {
        return nullptr;
    }
    std::shared_ptr<ThreadPool> pool(new ThreadPool(name, threadCount, priority));
    if (pool->start() == false) {
        return nullptr;
    }
    return pool;
}

bool ThreadPool::start() {
    for (int i = 0; i < threadCount_; i++) {
        auto thread = std::make_shared<std::thread>([this, i]() {run(i);});
        auto id = thread->get_id();
        threads_[id] = thread;
    }
    running = true;
    return true;
}

void ThreadPool::stop() {
    running = false;
    task_queue_mutex_.lock();
    for (auto i = 0; i < task_queue_.size(); i++) {
        task_queue_.pop();
    }
    task_queue_mutex_.unlock();

    for (auto it : threads_) {
        if (it.second->joinable()) {
            it.second->join();
        }
    }
    tracef("~ThreadPool\n");
}

void ThreadPool::postTask(std::function<void()> task) {
    TaskQueue::postTask(std::move(task));
    event_driver_->WakeUp();
}

void ThreadPool::run(int32_t index) {
    infof("threadpool:%s %d start\n", name_.c_str(), index);
    while (running) {
        event_driver_->Wait(1000 * 10);

        task_queue_mutex_.lock();
        if (!task_queue_.empty()) {
            size_t task_queue_size = task_queue_.size();
            auto task = task_queue_.front();
            task_queue_.pop();
            task_queue_mutex_.unlock();
            
            //infof("thread {} exec task, size:{}", index, (int)task_queue_size);
            task();
        } else {
            task_queue_mutex_.unlock();
        }
        
    }
    infof("threadpool %s %d exit\n", name_.c_str(), index);

}

}