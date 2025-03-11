#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

const double MinVal = std::numeric_limits<double>::min();
const double MaxVal = std::numeric_limits<double>::max();
constexpr int GB = 1073741824;

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cerr << "Please provie output filename!\n";
        return -1;
    }
    char *filename = argv[1];

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(MinVal, MaxVal);

    std::ostringstream oss;
    oss << std::scientific << std::setprecision(10);
    std::ofstream file(filename);
    for (int i = 0; i < GB;) {
        double random_number = dist(gen);
        oss << random_number << "\n";
        std::string str = oss.str();
        i += str.size();
        file << str;
    }
    file.close();
    return 0;
}