#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

static std::atomic<int> AvailThreads = std::thread::hardware_concurrency();

// Amount of 'Thread local' memory for temporary buffers
const int DefaultChunkSize = 4096; // most popular page size

const int MemLimit = 1 << 25;
// every line is 18 bytes long
const int Linesize = 18;

// TODO, implement multithread optimisation, ExternalMerge is ready for that
template <typename T> struct ExternalContainer {
    std::fstream m_file;
    T *arr;
    size_t m_total_filesize;
    int m_chunk_size;
    // std::mutex mtx;
    int m_loaded_chunk;
    int m_loaded_chunk_size;
    const bool m_dynamic_growth;
    bool m_placement;
    int m_max_elem_count;

    ExternalContainer(int chunk_size = DefaultChunkSize,
                      bool dynamic_growth = true, void *place = nullptr)
        : m_chunk_size(chunk_size - (chunk_size % sizeof(T))),
          m_dynamic_growth(dynamic_growth), m_loaded_chunk(-1),
          m_max_elem_count(m_chunk_size / (sizeof(*arr))), m_total_filesize(0) {

        if (place == nullptr) {
            arr = new T[m_chunk_size / (sizeof(*arr))];
            m_placement = false;
        } else {
            arr = new (place) T[m_chunk_size / (sizeof(*arr))];
            m_placement = true;
        }
    }

    ExternalContainer(ExternalContainer &&other) noexcept
        : m_file(std::move(other.m_file)), arr(other.arr),
          m_total_filesize(other.m_total_filesize),
          m_chunk_size(other.m_chunk_size),
          m_loaded_chunk(other.m_loaded_chunk),
          m_loaded_chunk_size(other.m_loaded_chunk_size),
          m_dynamic_growth(other.m_dynamic_growth),
          m_placement(other.m_placement),
          m_max_elem_count(other.m_max_elem_count) {

        other.arr = nullptr; // Prevent deletion in moved-from object
    }

    bool store_readable(const char *dest_filepath) {
        if (!m_file.is_open()) {
            return false;
        }
        flush_file();

        std::ofstream outfile(dest_filepath);
        outfile << std::scientific << std::setprecision(10);
        m_file.seekg(0, std::ios::beg);
        double val = 0.0;
        while (m_file.read(reinterpret_cast<char *>(&val), sizeof(val))) {
            outfile << val << "\n";
        }

        if (m_file.eof()) {
            std::cerr << "End of file reached." << std::endl;
        } else if (m_file.fail()) {
            std::cerr << "Read failed: Non-fatal I/O error (e.g., type "
                         "mismatch, format error)."
                      << std::endl;
        } else if (m_file.bad()) {
            std::cerr << "Read failed: Fatal I/O error (e.g., hardware "
                         "failure, corruption)."
                      << std::endl;
        }

        outfile.close();
        return true;
    }
    int create_empty_workfile() {
        const char *tmp_name = std::tmpnam(nullptr);
        m_file.open(tmp_name, std::ios::in | std::ios::out | std::ios::binary |
                                  std::ios::trunc);

        if (!m_file.is_open()) {
            printf("Error, unable to open tmp file: %s ", tmp_name);
            return -1;
        }
        m_loaded_chunk_size = 0;
        m_loaded_chunk = 0;
        return 0;
    }

    int prepare_workfile(const char *orig_filepath, const char *dest_filepath) {
        std::ifstream sourceFile(orig_filepath);
        if (!sourceFile) {
            printf("Error %s not found", orig_filepath);
            return -1;
        }
        if (!std::filesystem::exists(dest_filepath)) {
            m_file.open(dest_filepath, std::ios::in | std::ios::out |
                                           std::ios::binary | std::ios::trunc);
            if (!m_file.is_open()) {
                printf("Error, unable to open %s ", dest_filepath);
                sourceFile.close();
                return -1;
            }
            char line[Linesize] = {};
            while (sourceFile.getline(line, Linesize)) {
                double res = std::atof(line);
                m_file.write(reinterpret_cast<char *>(&res), sizeof(res));
            }
        } else {
            m_file.open(dest_filepath,
                        std::ios::in | std::ios::out | std::ios::binary);
        }
        if (!m_file.is_open()) {
            printf("Error, unable to open %s ", dest_filepath);
            sourceFile.close();
            return -1;
        }
        m_file.seekg(0, std::ios::beg);
        m_total_filesize =
            static_cast<int>(std::filesystem::file_size(dest_filepath));
        m_file.read(reinterpret_cast<char *>(arr), m_chunk_size);
        m_loaded_chunk_size = m_file.gcount();
        m_loaded_chunk = 0;
        m_file.clear();

        sourceFile.close();
        const int total_size = m_total_filesize / sizeof(*arr);
        return total_size;
    }

    T &operator[](int index) {

        if (m_loaded_chunk == -1) {
            create_empty_workfile();
            return arr[index % m_max_elem_count];
        }
        size_t total_size = m_total_filesize / sizeof(*arr);

        if (m_dynamic_growth == false && (index < 0 || index >= total_size)) {
            throw std::out_of_range("Index out of bounds");
        }

        size_t chunk_off = (m_loaded_chunk * m_chunk_size) / sizeof(*arr);
        size_t chunk_off_end = (chunk_off + m_max_elem_count);

        // TODO REMOVE, only for debuggin purpose
        m_file.flush();

        if (index < chunk_off || index >= chunk_off_end) {
            flush_file();

            const int i_chunk = index / m_max_elem_count;
            size_t off_chunk = i_chunk * m_chunk_size;
            size_t lim_chunk =
                ((total_size * (sizeof(*arr))) - off_chunk) > m_chunk_size
                    ? m_chunk_size
                    : (total_size * (sizeof(*arr)));

            if (m_dynamic_growth) {
                lim_chunk = m_chunk_size;
            }

            m_file.seekg(off_chunk, std::ios::beg);
            m_file.read(reinterpret_cast<char *>(arr), lim_chunk);
            m_file.clear();
            m_loaded_chunk_size = m_file.gcount();
            m_loaded_chunk = i_chunk;
        }
        return arr[index % m_max_elem_count];
    }
    bool flush_file() {
        size_t chunk_off = m_loaded_chunk * m_chunk_size;
        m_file.seekp(chunk_off, std::ios::beg);

        // Attempt to write and check if it succeeded
        if (m_dynamic_growth) {
            m_file.write(reinterpret_cast<char *>(arr), m_chunk_size);
        } else {
            m_file.write(reinterpret_cast<char *>(arr), m_loaded_chunk_size);
        }
        m_file.clear();
        const bool res = m_file.good();

        return res;
    }

    ~ExternalContainer() {
        flush_file();
        if (!m_placement)
            delete[] arr;

        if (m_file.is_open()) {
            m_file.close();
        }
    }
};
