#pragma once
#include "util/Timer.hpp"
#include <Arduino.h>
#include <functional>
#include <vector>
#include "util/Logger.hpp"

namespace app {

// Task scheduler for non-blocking periodic tasks
class TaskScheduler {
public:
    using TaskFunc = std::function<void()>;

    struct Task {
        const char* name;
        TaskFunc func;
        uint32_t intervalMs;
        util::Timer timer;
        bool enabled;

        Task(const char* n, TaskFunc f, uint32_t interval, bool en = true)
            : name(n), func(f), intervalMs(interval), timer(interval), enabled(en) {}
    };

    TaskScheduler() = default;

    // Add a new task
    void addTask(const char* name, TaskFunc func, uint32_t intervalMs, bool enabled = true) {
        tasks_.emplace_back(name, func, intervalMs, enabled);
    }

    // Run all due tasks
    void tick() {
        for (auto& task : tasks_) {
            if (task.enabled && task.timer.check()) {
                task.func();
            }
        }
    }

    // Enable/disable tasks by name
    void enable(const char* name) {
        for (auto& task : tasks_) {
            if (strcmp(task.name, name) == 0) {
                task.enabled = true;
                task.timer.reset();
                return;
            }
        }
    }

    void disable(const char* name) {
        for (auto& task : tasks_) {
            if (strcmp(task.name, name) == 0) {
                task.enabled = false;
                return;
            }
        }
    }

    // Set interval for a task
    void setInterval(const char* name, uint32_t intervalMs) {
        for (auto& task : tasks_) {
            if (strcmp(task.name, name) == 0) {
                task.intervalMs = intervalMs;
                task.timer.setInterval(intervalMs);
                return;
            }
        }
    }

    // Get task count
    size_t taskCount() const { return tasks_.size(); }

    // Print task status
    void printStatus() const {
        Logger::info("=== Task Status ===");
        for (const auto& task : tasks_) {
            Logger::info("  %s: %s (interval: %ums, next in: %ums)",
                task.name,
                task.enabled ? "enabled" : "disabled",
                task.intervalMs,
                task.timer.remaining());
        }
    }

private:
    std::vector<Task> tasks_;
};

} // namespace app
