#include "cache.h"
#include "mock_db.h"
#include "pipeline.h"

void task1() {
    std::cout << "task1\n";
    return;
}

std::string task2() {
    std::cout << "task2\n";
    static int val = 0;
    if (val > 1) {
        return "nice!";
    }
    val++;
    return "";
}

std::string task3() {
    std::cout << "task3\n";
    return "acab";
}
const char *help =
    R"(interactive menu:
    1 - get value
    2 - set value
    3 - delete value
    4 - start transaction
    5 - commit transaction
    6 - abort transaction
    7 - print help
    0 - exit
    )";

int main() {
    bool run = true;
    int action = 0;
    mock_db db;
    cache ch(&db);
    std::cout << help;

    std::string key;
    std::string val;
    while (run) {
        std::cout << "action: ";
        std::cin >> action;
        switch (action) {
        case 0:
            run = false;
            break;
        case 1:
            std::cout << "provide key:";
            std::cin >> key;
            std::cout << ch.get(key) << "\n";
            break;
        case 2:
            std::cout << "provide key:";
            std::cin >> key;

            std::cout << "provide val:";
            std::cin >> val;
            std::cout << ch.set(key, val) << "\n";
            break;
        case 3:
            std::cout << "provide key:";
            std::cin >> key;
            std::cout << ch.remove(key) << "\n";
            break;
        case 4:
            std::cout << ch.begin_transaction() << "\n";
            break;
        case 5:
            std::cout << ch.commit_transaction() << "\n";
            break;
        case 6:
            std::cout << ch.abort_transaction() << "\n";
            break;
        case 7:
            std::cout << help;
            break;
        default:
            break;
        }
    }

    return 0;
}