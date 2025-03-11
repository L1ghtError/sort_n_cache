#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <vector>

enum class transaction_state {
    off = 1,
    ready = 1 << 1,
    started = 1 << 2,
};

struct Pipeline {
    enum class fail_policy {
        ignore = 0,
        retry = 1,
        abort = 1 << 1,
        restore = 1 << 2
    };
    using fp = fail_policy;

    template <typename Func, typename... Args>
    void add(Func &&func, Args &&...args) {
        std::unique_lock lock(_mtx);
        pipeline.emplace_back([this, func = std::forward<Func>(func),
                               ... args = std::forward<Args>(args)]() mutable {
            if constexpr (std::is_same_v<std::invoke_result_t<Func, Args...>,
                                         std::string>) {
                std::string res = std::invoke(func, args...);

                if (res == "" && _policy != fp::ignore)
                    failed = true;
            } else {
                std::invoke(func, args...);
            }
        });
    }
    void set_fail_policy(fail_policy new_policy) { _policy = new_policy; }

    bool run() {
        int attempt = 0;
        force_quit = false;

        std::shared_lock lock(_mtx);
        for (const auto &func : pipeline) {
        iteration_begin:
            if (force_quit) {
                force_quit = false;
                clear();
                cancel_cv.notify_all();
                return false;
            }
            func();
            if (failed) {
                failed = false;
                if (_policy == fp::retry && attempt < 2) {
                    // evil ;)
                    goto iteration_begin;
                } else if (_policy == fp::abort) {
                    return false;
                }
            }

            ++processed;
        }
        return true;
    }
    void clear() { pipeline.clear(); }
    void cancel() {
        force_quit = true;
        std::unique_lock lk(cancel_mtx);
        cancel_cv.wait(lk, [&] { return pipeline.size() == 0; });
    }

  private:
    fail_policy _policy;
    std::vector<std::function<void()>> pipeline;
    mutable std::shared_mutex _mtx;
    mutable std::mutex cancel_mtx;
    std::condition_variable cancel_cv;
    std::atomic<bool> failed;
    std::atomic<bool> force_quit;
    int processed;
};