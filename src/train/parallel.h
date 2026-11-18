/// @file parallel.h
/// @brief Doing many independent jobs at once on several processor cores.
///
/// Playing an episode is slow and each one is independent of the others, so
/// training plays many at the same time, one per core.

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace ant {

/// Runs `work(i)` for every i from 0 to `count - 1`, sharing the jobs between
/// several threads. Each job must not touch anything another job touches.
///
/// @param threads How many jobs run at once; 0 means one per processor core.
template <typename Work>
void parallelFor(std::size_t count, int threads, Work work) {
    const unsigned available = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t workerCount =
        std::min<std::size_t>(count, threads > 0 ? static_cast<unsigned>(threads) : available);

    // Every worker repeatedly takes the next job number that nobody has taken yet.
    std::atomic<std::size_t> next{0};
    const auto worker = [&] {
        for (std::size_t i = next++; i < count; i = next++) {
            work(i);
        }
    };

    std::vector<std::thread> workers;
    for (std::size_t i = 1; i < workerCount; ++i) {
        workers.emplace_back(worker);
    }
    worker();  // the calling thread does its share too
    for (std::thread& thread : workers) {
        thread.join();
    }
}

}  // namespace ant
