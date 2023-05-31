#include "task_queue.h"

namespace infra {

TaskQueue::TaskQueue() {

}

TaskQueue::~TaskQueue() {
    
}

void TaskQueue::postTask(std::function<void()> task) {
    std::lock_guard<decltype(task_queue_mutex_)> guard(task_queue_mutex_);
    task_queue_.push(std::move(task));
}


}