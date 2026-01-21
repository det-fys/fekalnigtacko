#pragma once

#include <cstdint>
#include <functional>
#include <queue>

class Scheduler
{
    struct Task
    {
        int64_t execute_at;
        std::function<void()> task;

        // order by priority (earliest LAST)
        bool operator<(const Task& other) const
        {
            return execute_at > other.execute_at;
        }
    };

public:
    Scheduler(const int64_t& time) : time_(time) {}

    void Schedule(int64_t delay_ms, std::function<void()> task)
    {
        task_queue_.push(Task{time_ + delay_ms, std::move(task)});
    }

    // int64_t GetTime() const { return time_; }

protected:
    void RunTasks()
    {
        while (!task_queue_.empty() && task_queue_.top().execute_at <= time_)
        {
            auto scheduled_task = task_queue_.top();
            task_queue_.pop();
            scheduled_task.task();
        }
    }

private:
    const int64_t& time_; // Reference to external time source
    std::priority_queue<Task, std::vector<Task>> task_queue_;
};
