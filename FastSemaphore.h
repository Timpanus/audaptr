#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>

/// @brief Fast semaphore class that relies on ordered access to atomics where possible
class FastSemaphore {
    class Semaphore {
    public:
        inline void Post() noexcept
        {
            {
                std::unique_lock<std::mutex> Lock(Mutex_);
                ++Count_;
            }
            CondVar_.notify_one();
        }

        inline void Wait() noexcept
        {
            std::unique_lock<std::mutex> Lock(Mutex_);
            CondVar_.wait(Lock, [&]() { return Count_ != 0; });
            --Count_;
        }
    private:
        int Count_{0};
        std::mutex Mutex_;
        std::condition_variable CondVar_;
    };
public:
    inline void Post() noexcept
    {
        const int Count = Count_.fetch_add(1, std::memory_order_release);
        if(Count < 0)
            Semaphore_.Post();
    }

    inline void Wait() noexcept
    {
        const int Count = Count_.fetch_sub(1, std::memory_order_acquire);
        if(Count < 1)
            Semaphore_.Wait();
    }
private:
    std::atomic_int Count_{0};
    Semaphore Semaphore_;
};
