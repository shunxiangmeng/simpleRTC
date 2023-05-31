#include "infra/task_queue.h"
#include "infra/thread_pool.h"
#include <chrono>
#include "infra/logger.h"

int main () {

    infof("start...\n");

    {
        std::shared_ptr<infra::ThreadPool> pool = infra::ThreadPool::create("test");


        int count = 0;

        while (1) {
            //std::this_thread::sleep_for(std::chrono::milliseconds(10));

            pool->postTask([&]() {
                errorf("task exec\n");
                });

            count++;
            if (count > 100) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    }
    return 0;
}
