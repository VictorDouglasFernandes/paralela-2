#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for(size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this] { 
                        return stop || !tasks.empty(); 
                    });
                    
                    if(stop && tasks.empty()) {
                        return;
                    }
                    
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers) {
        worker.join();
    }
}

// template<class F, class... Args>
// auto ThreadPool::enqueue(F&& f, Args&&... args) 
//     -> std::future<typename std::result_of<F(Args...)>::type> {
//     using return_type = typename std::result_of<F(Args...)>::type;

//     auto task = std::make_shared<std::packaged_task<return_type()>>(
//         std::bind(std::forward<F>(f), std::forward<Args>(args)...)
//     );

//     std::future<return_type> res = task->get_future();
//     {
//         std::unique_lock<std::mutex> lock(queue_mutex);
//         tasks.emplace([task](){ (*task)(); });
//     }
//     condition.notify_one();
//     return res;
// } 