#pragma once
#include <atomic>
#include <cstdint>

namespace asyncwork {
struct WorkerFailures { uint64_t work=0, notification=0; };
// Callbacks run outside the scheduling lock. A failed snapshot must not escape
// the thread entry point and terminate every unrelated Toolbox feature.
class WorkerFailureState {
 std::atomic<uint64_t> work_{0}, notification_{0};
public:
 void workFailed() noexcept { work_.fetch_add(1,std::memory_order_relaxed); }
 void notificationFailed() noexcept { notification_.fetch_add(1,std::memory_order_relaxed); }
 WorkerFailures get() const noexcept { return {work_.load(std::memory_order_relaxed),notification_.load(std::memory_order_relaxed)}; }
};
}
