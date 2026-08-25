#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>

#include "defs.hpp"

class WorkerThread
{
public:
    WorkerThread()
    {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        // Single-threaded fallback for WASM without SharedArrayBuffer
        single_thread_mode = true;
#else
        worker_ = std::thread([this]() {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mtx_);
                    cond_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

                    if (stop_ && tasks_.empty())
                        return;

                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task(); // Execute chunk generation outside the lock
            }
        });
#endif
    }

    DELETE_COPY_MOVE(WorkerThread);

    // Schedules a task and returns an owning handle (std::future)
    template <class F>
    auto Schedule(F&& f) -> std::future<std::invoke_result_t<F>>
    {
        using return_type = std::invoke_result_t<F>;

        // Package the callable into a task
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
        std::future<return_type> res = task->get_future();

        if (single_thread_mode_)
        {
            // If threads aren't supported, execute synchronously right now
            (*task)();
        }
        else
        {
            // Push to worker thread queue
            std::unique_lock<std::mutex> lock(queue_mtx_);
            tasks_.emplace([task]() { (*task)(); });
            cond_.notify_one();
        }

        return res;
    }

    ~WorkerThread()
    {
        if (!single_thread_mode_)
        {
            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                stop_ = true;
            }
            cond_.notify_all();
            if (worker_.joinable())
                worker_.join();
        }
    }

private:
    std::thread worker_;
    std::mutex queue_mtx_;
    std::condition_variable cond_;
    std::queue<std::function<void()>> tasks_;
    bool stop_ = false;
    bool single_thread_mode_ = false;
};
