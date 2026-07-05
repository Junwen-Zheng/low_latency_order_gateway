#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace llgw {

template <typename T, std::size_t Capacity>
class RingBuffer {
 public:
  static_assert(Capacity > 0, "RingBuffer capacity must be positive");

  bool Push(const T& value) {
    if (Full()) {
      return false;
    }

    storage_[tail_] = value;
    tail_ = NextIndex(tail_);
    ++size_;
    return true;
  }

  std::optional<T> Pop() {
    if (Empty()) {
      return std::nullopt;
    }

    T value = storage_[head_];
    head_ = NextIndex(head_);
    --size_;
    return value;
  }

  bool Empty() const { return size_ == 0; }
  bool Full() const { return size_ == Capacity; }

  std::size_t Size() const { return size_; }
  constexpr std::size_t CapacityValue() const { return Capacity; }

 private:
  static constexpr std::size_t NextIndex(std::size_t index) {
    return (index + 1) % Capacity;
  }

  std::array<T, Capacity> storage_{};
  std::size_t head_ = 0;
  std::size_t tail_ = 0;
  std::size_t size_ = 0;
};

}  // namespace llgw
