#pragma once
#include <iostream>
#include <string>
struct i_db {
    virtual ~i_db() {} // Virtual destructor for cleanup

    virtual bool begin_transaction() = 0;
    virtual bool commit_transaction() = 0;
    virtual bool abort_transaction() = 0;
    virtual std::string get(const std::string &key) = 0;
    virtual std::string set(const std::string &key,
                            const std::string &data) = 0;
    // 'delete' is reserved C++ keyword
    virtual std::string remove(const std::string &key) = 0;
};