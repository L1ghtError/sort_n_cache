#pragma once
#include "i_db.h"
#include "pipeline.h"

#include "stdint.h"
#include <atomic>
#include <chrono>
#include <iterator>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

template <typename T> struct cache_el {
    uint64_t expires_at;
    T data;
};

struct cache : public i_db {
    using val_type = cache_el<std::string>;
    cache(i_db *upstream, uint64_t ttl = 12)
        : m_upstream(upstream), m_ttl(ttl),
          _cleanup_thread(&cache::cleanup, this), _stop_cleanup(false) {}

    ~cache() {
        _stop_cleanup = true;
        if (_cleanup_thread.joinable()) {
            _cleanup_thread.join();
        }
    }

    bool begin_transaction() override;
    bool commit_transaction() override;
    bool abort_transaction() override;
    std::string get(const std::string &key) override;
    std::string set(const std::string &key, const std::string &data) override;
    std::string remove(const std::string &key) override;

  private:
    void cleanup();
    std::string _get(const std::string &key);
    std::string _set(const std::string &key, const std::string &data);
    std::string _remove(const std::string &key);

    std::unordered_map<std::string, val_type> m_cache_map;
    mutable std::shared_mutex m_mutex_;
    i_db *m_upstream;
    uint64_t m_ttl;

    std::atomic<bool> _stop_cleanup;
    std::thread _cleanup_thread;

    transaction_state state;
};

bool cache::begin_transaction() {
    if (state == transaction_state::started ||
        state == transaction_state::ready) {
        return false;
    }
    state = transaction_state::ready;
    bool res = m_upstream->begin_transaction();
    if (res == false)
        state = transaction_state::off;
    return res;
}

bool cache::commit_transaction() {
    if (state == transaction_state::off ||
        state == transaction_state::started) {
        return false;
    }
    state = transaction_state::started;

    bool res = m_upstream->commit_transaction();
    state = transaction_state::off;
    return res;
}

bool cache::abort_transaction() {
    if (state == transaction_state::off) {
        return false;
    }
    bool res = m_upstream->abort_transaction();
    state = transaction_state::off;
    return res;
}

void cache::cleanup() {
    while (!_stop_cleanup) {
        std::this_thread::sleep_for(std::chrono::seconds(m_ttl * 2));

        std::time_t now = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());

        std::unique_lock lock(m_mutex_);
        for (auto it = m_cache_map.begin(); it != m_cache_map.end();) {
            if (it->second.expires_at <= now) {
                it = m_cache_map.erase(it);
            } else {
                ++it;
            }
        }
    }
}

std::string cache::get(const std::string &key) {
    if (state == transaction_state::ready) {
        m_upstream->get(key);
        return "ok";
    } else {
        return _get(key);
    }
}

std::string cache::_get(const std::string &key) {
    m_mutex_.lock_shared();
    auto val = m_cache_map.find(key);
    m_mutex_.unlock_shared();

    auto now = std::chrono::system_clock::now();
    std::time_t unix_time =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    // valid cache element!
    if (val != m_cache_map.end()) {
        const val_type &v = val->second;

        if (v.expires_at > unix_time) {
            return val->second.data;
        }
    }
    std::string resp = m_upstream->get(key);
    if (resp == "") {
        std::unique_lock lock(m_mutex_);
        m_cache_map.erase(key);
        return resp;
    }

    unix_time =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    m_cache_map[key] = val_type{unix_time + m_ttl, resp};
    return resp;
}

std::string cache::set(const std::string &key, const std::string &data) {
    if (state == transaction_state::ready) {
        m_upstream->set(key, data);
        return "ok";
    } else {
        return _set(key, data);
    }
}

std::string cache::_set(const std::string &key, const std::string &data) {
    std::string resp = m_upstream->set(key, data);
    if (resp == "") {
        return resp;
    }

    std::time_t unix_time =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::unique_lock lock(m_mutex_);
    m_cache_map[key] = val_type{unix_time + m_ttl, resp};
    return resp;
}

std::string cache::remove(const std::string &key) {
    if (state == transaction_state::ready) {
        m_upstream->remove(key);
        return "ok";
    } else {
        return _remove(key);
    }
}

std::string cache::_remove(const std::string &key) {
    std::string resp = m_upstream->remove(key);
    if (resp == "") {
        return resp;
    }
    std::unique_lock lock(m_mutex_);
    m_cache_map.erase(key);
    return resp;
}