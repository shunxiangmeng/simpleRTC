#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include <list>
#include <memory>

namespace infra {

class TaskQueue {
public:

    TaskQueue();  //默认构造函数

    TaskQueue(const TaskQueue&) = delete;  //拷贝构造函数

    TaskQueue(TaskQueue&&) = delete;   //移动构造函数

    ~TaskQueue();

	virtual void postTask(std::function<void()> task);


	void postDelayedTask(std::function<void() &&> task, int64_t delayTime);

protected:

    std::queue<std::function<void()>> task_queue_;
    std::mutex task_queue_mutex_;

    struct DelayedTask {
        bool operator< (const DelayedTask& delayedTask) const {
            return (delayedTask.run_time_ms < run_time_ms);
        }

        int64_t delay_ms;
        int64_t run_time_ms;
        uint32_t task_number;
        std::function<void()> task;
    };

    std::priority_queue<DelayedTask> delayed_task_queue_;

};

}
