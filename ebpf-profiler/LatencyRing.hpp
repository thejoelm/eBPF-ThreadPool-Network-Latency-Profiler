#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <vector>
#include <cstdint>

template <size_t N = 8192>

class LatencyRing {
    private:
        std::array<uint64_t, N> ringBuf;
        std::atomic<size_t> writeIdx;

        static std::mutex& registryMutex() {
            static std::mutex mtx;
            return mtx;
        }

    public:
        static std::vector<LatencyRing<N>*>& registry() {
            static std::vector<LatencyRing<N>*> reg;
            return reg;
        }
        LatencyRing() {
            std::lock_guard<std::mutex> lock(registryMutex());
            registry().push_back(this);
        }
        void record(uint64_t value) {
            size_t idx = writeIdx.fetch_add(1, std::memory_order_relaxed) % N;
            ringBuf[idx] = value;
        }

        std::vector<uint64_t> drain() const {
            size_t count = std::min(writeIdx.load(std::memory_order_relaxed), N);
            return std::vector<uint64_t>(ringBuf.begin(), ringBuf.begin() + count);
        }
};