
#include <chrono>

#include "external_container.hpp"

thread_local static std::thread::id cur_tid;

template <typename T> struct ExternalMerge {
    using unrefT = std::remove_reference_t<T>;
    ExternalMerge(int chunk_size = DefaultChunkSize,
                  int thread_count = AvailThreads)
        // prefer even
        : m_chunk_size(chunk_size - (chunk_size % 2)),
          m_thd_avail(thread_count - (thread_count % 2)),
          m_max_avail(thread_count - (thread_count % 2)) {
        // +1 for the main thread
        const int ChunkMemLim = MemLimit / (m_thd_avail + 1);
        if (ChunkMemLim < m_chunk_size) {
            m_chunk_size = ChunkMemLim;
        }
        const int req_mem = (m_chunk_size * (m_thd_avail + 1));
        memBuf = new uint8_t[req_mem];
        cur_tid = std::this_thread::get_id();
        bufMap.emplace(cur_tid, memBuf);
    }
    void merge_sort(T arr, int size);
    ~ExternalMerge() { delete[] memBuf; }

  private:
    void _merge_sort(T arr, int l, int r);
    void merge(T arr, int l, int m, int r);
    void _init_thread_tmp();
    void _init_thread_buf();
    int m_chunk_size;
    int m_max_avail;
    std::atomic<int> m_thd_avail;
    uint8_t *memBuf;
    std::mutex mapMtx;

    std::unordered_map<std::thread::id, uint8_t *> bufMap;
    std::unordered_map<std::thread::id, std::pair<unrefT, unrefT>> tmpMap;
};

template <typename T> void ExternalMerge<T>::merge_sort(T arr, int size) {
    _init_thread_buf();
    _init_thread_tmp();

    m_thd_avail = m_max_avail;
    _merge_sort(arr, 0, size - 1);
    bufMap.clear();
    tmpMap.clear();
}

template <typename T> void ExternalMerge<T>::_init_thread_tmp() {
    unrefT lp(m_chunk_size / 2, true, (bufMap[cur_tid]));
    unrefT rp(m_chunk_size / 2, true, (bufMap[cur_tid] + m_chunk_size / 2));
    tmpMap.emplace(cur_tid, std::make_pair(std::move(lp), std::move(rp)));
}

template <typename T> void ExternalMerge<T>::_init_thread_buf() {
    bufMap.emplace(cur_tid, memBuf + (m_chunk_size * bufMap.size()));
}

template <typename T> void ExternalMerge<T>::_merge_sort(T arr, int l, int r) {
    int m = l + (r - l) / 2;
    if (r - l > 1) {

        int val = m_thd_avail.load();
        if (m_thd_avail > 1 &&
            m_thd_avail.compare_exchange_strong(val, val - 2)) {
            std::thread lthd([&]() {
                cur_tid = std::this_thread::get_id();
                mapMtx.lock();
                _init_thread_buf();
                _init_thread_tmp();
                mapMtx.unlock();
                _merge_sort(arr, l, m);
            });
            std::thread rthd([&]() {
                cur_tid = std::this_thread::get_id();
                mapMtx.lock();
                _init_thread_buf();
                _init_thread_tmp();
                mapMtx.unlock();
                _merge_sort(arr, m + 1, r);
            });
            lthd.join();
            rthd.join();
        } else {
            _merge_sort(arr, l, m);
            if (r - m > 1)
                _merge_sort(arr, m + 1, r);
        }
    }

    merge(arr, l, m, r);
}

template <typename T> void ExternalMerge<T>::merge(T arr, int l, int m, int r) {

    // r - read, l - reft, a - arr
    int rla = 0, rra = 0;
    unrefT &lp = (tmpMap[cur_tid]).first;
    unrefT &rp = (tmpMap[cur_tid]).second;
    int ka = l;
    size_t llen = (1 + m - l);

    size_t rlen = (r - m);

    for (int i = 0; i < llen; i++)
        lp[i] = arr[l + rla++];
    for (int j = 0; j < rlen; j++)
        rp[j] = arr[m + 1 + rra++];

    int i = 0;
    int j = 0;
    while (i < llen && j < rlen) {

        if (lp[i] <= rp[j]) {
            arr[ka] = lp[i];

            i++;
        } else {
            arr[ka] = rp[j];
            j++;
        }

        ka++;
    }

    while (i < llen) {
        arr[ka] = lp[i];
        i++;
        ka++;
    }
    while (j < rlen) {
        arr[ka] = rp[j];
        j++;
        ka++;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("provide source and destenation filenames!\n");
        return -1;
    }
    printf("started to sort %s.\n", argv[1]);
    ExternalContainer<double> ec(MemLimit, false);
    int total_size = ec.prepare_workfile(argv[1], "./plane.wf");
    printf("generation of plane file is done.\n");
    ExternalMerge<ExternalContainer<double> &> sorter(MemLimit, 0);
    const auto start = std::chrono::high_resolution_clock::now();
    sorter.merge_sort(ec, total_size);
    const auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    printf("time spent %lf\n", diff.count());

    ec.store_readable(argv[2]);
    printf("starting file validation.\n");
    for (int j = 1; j < total_size; j++) {
        if (ec[j - 1] > ec[j]) {
            printf("Error \n%lf\n is larger than \n%lf\n", ec[j - 1], ec[j]);
            return -1;
        }
    }
    printf("file is well sorted.\ngoodbye.");
    return 0;
}
