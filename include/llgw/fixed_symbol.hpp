#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace llgw {

class FixedSymbol {
 public:
  static constexpr std::size_t kMaxLength = 16;

  FixedSymbol() = default;

  FixedSymbol(std::string_view value) {
    Assign(value);
  }

  FixedSymbol& operator=(std::string_view value) {
    Assign(value);
    return *this;
  }

  bool Assign(std::string_view value) {
    data_.fill('\0');
    length_ = 0;

    if (value.empty() || value.size() > kMaxLength) {
      return false;
    }

    std::copy(value.begin(), value.end(), data_.begin());
    length_ = static_cast<std::uint8_t>(value.size());
    return true;
  }

  bool empty() const {
    return length_ == 0;
  }

  std::size_t size() const {
    return length_;
  }

  std::string_view view() const {
    return std::string_view(data_.data(), length_);
  }

  bool operator==(std::string_view other) const {
    return view() == other;
  }

  bool operator!=(std::string_view other) const {
    return !(*this == other);
  }

 private:
  std::array<char, kMaxLength> data_{};
  std::uint8_t length_ = 0;
};

}  // namespace llgw
