#pragma once

#include <chrono>
#include <cstdint>

namespace llgw {

class LatencyClock {
 public:
  static std::uint64_t NowNs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
  }
};

}  // namespace llgw
