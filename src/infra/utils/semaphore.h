#pragma once
#include <mutex>
#include <condition_variable>
namespace infra {   
class Semaphore {
public:
    Semaphore (int count = 0): count_(count) {}
    
    inline void notify() {
        std::unique_lock<std::mutex> lock(mtx_);
        count_++;
        //notify the waiting thread
        cv_.notify_one();
    }
    inline void wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        while(count_ == 0) {
            //wait on the mutex until notify is called
            cv_.wait(lock);
        }
        count_--;
    }
private:
    std::mutex mtx_;
    std::condition_variable cv_;
    int count_;
};
}