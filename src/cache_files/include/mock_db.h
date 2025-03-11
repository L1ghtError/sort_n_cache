#pragma once
#include "i_db.h"
#include "pipeline.h"

#include <algorithm>
#include <shared_mutex>
#include <utility>
#include <vector>

struct mock_db : public i_db {

    bool begin_transaction() override;
    bool commit_transaction() override;
    bool abort_transaction() override;
    std::string get(const std::string &key) override;
    std::string set(const std::string &key, const std::string &data) override;
    std::string remove(const std::string &key) override;

    void set_policy(Pipeline::fail_policy fp) { pl.set_fail_policy(fp); }

  private:
    std::string _get(const std::string &key);
    std::string _set(const std::string &key, const std::string &data);
    std::string _remove(const std::string &key);
    mutable std::shared_mutex m_mutex_;
    std::vector<std::pair<std::string, std::string>> vals;
    Pipeline pl;
    transaction_state state;
};

bool mock_db::begin_transaction() {
    if (state == transaction_state::started) {
        return false;
    }
    state = transaction_state::ready;
    return true;
}

bool mock_db::commit_transaction() {
    if (state == transaction_state::off ||
        state == transaction_state::started) {
        return false;
    }
    state = transaction_state::started;
    bool res = pl.run();
    state = transaction_state::off;
    return res;
}
bool mock_db::abort_transaction() {
    if (state == transaction_state::off) {
        return false;
    }
    if (state == transaction_state::ready) {
        pl.clear();
    } else {
        pl.cancel();
    }
    state = transaction_state::off;
    return true;
}

std::string mock_db::get(const std::string &key) {
    if (state == transaction_state::ready) {
        pl.add(&mock_db::_get, this, key);
        return "ok";
    } else {
        return _get(key);
    }
}

std::string mock_db::_get(const std::string &key) {
    std::shared_lock lock(m_mutex_);
    for (int i = 0; i < vals.size(); i++) {
        if (vals[i].first == key) {
            return vals[i].second;
        }
    }
    return "";
}

std::string mock_db::set(const std::string &key, const std::string &data) {
    if (state == transaction_state::ready) {
        pl.add(&mock_db::_set, this, key, data);
        return "ok";
    } else {
        return _set(key, data);
    }
}

std::string mock_db::_set(const std::string &key, const std::string &data) {
    std::unique_lock lock(m_mutex_);
    for (int i = 0; i < vals.size(); i++) {
        if (vals[i].first == key) {
            vals[i].second = data;
            return data;
        }
    }
    const std::pair<std::string, std::string> p(key, data);
    vals.emplace_back(p);
    return data;
}

std::string mock_db::remove(const std::string &key) {
    if (state == transaction_state::ready) {
        pl.add(&mock_db::_remove, this, key);
        return "ok";
    } else {
        return _remove(key);
    }
}

std::string mock_db::_remove(const std::string &key) {
    std::unique_lock lock(m_mutex_);
    for (auto it = vals.begin(); it != vals.end(); ++it) {
        if (it->first == key) {
            vals.erase(it);
            return "ok";
        }
    }
    return "";
}