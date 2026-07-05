#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "llgw/ring_buffer.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

void TestEmptyBuffer() {
  llgw::RingBuffer<int, 3> buffer;

  Expect(buffer.Empty(), "new buffer should be empty");
  Expect(!buffer.Full(), "new buffer should not be full");
  Expect(buffer.Size() == 0, "new buffer size should be 0");
  Expect(buffer.CapacityValue() == 3, "capacity should be 3");

  const std::optional<int> value = buffer.Pop();
  Expect(!value.has_value(), "pop from empty buffer should return nullopt");
}

void TestPushPopSingleValue() {
  llgw::RingBuffer<int, 3> buffer;

  Expect(buffer.Push(42), "push should succeed");
  Expect(!buffer.Empty(), "buffer should not be empty after push");
  Expect(buffer.Size() == 1, "buffer size should be 1");

  const std::optional<int> value = buffer.Pop();

  Expect(value.has_value(), "pop should return value");
  Expect(*value == 42, "popped value should match pushed value");
  Expect(buffer.Empty(), "buffer should be empty after pop");
  Expect(buffer.Size() == 0, "buffer size should return to 0");
}

void TestFifoOrdering() {
  llgw::RingBuffer<int, 3> buffer;

  Expect(buffer.Push(1), "push 1 should succeed");
  Expect(buffer.Push(2), "push 2 should succeed");
  Expect(buffer.Push(3), "push 3 should succeed");

  Expect(buffer.Full(), "buffer should be full");
  Expect(buffer.Size() == 3, "buffer size should be 3");

  Expect(buffer.Pop().value() == 1, "first pop should be 1");
  Expect(buffer.Pop().value() == 2, "second pop should be 2");
  Expect(buffer.Pop().value() == 3, "third pop should be 3");
  Expect(buffer.Empty(), "buffer should be empty after all pops");
}

void TestRejectPushWhenFull() {
  llgw::RingBuffer<int, 2> buffer;

  Expect(buffer.Push(1), "push 1 should succeed");
  Expect(buffer.Push(2), "push 2 should succeed");
  Expect(buffer.Full(), "buffer should be full");

  Expect(!buffer.Push(3), "push when full should fail");
  Expect(buffer.Size() == 2, "failed push should not change size");

  Expect(buffer.Pop().value() == 1, "first value should still be 1");
  Expect(buffer.Pop().value() == 2, "second value should still be 2");
  Expect(buffer.Empty(), "buffer should be empty after two pops");
}

void TestWrapAroundOrdering() {
  llgw::RingBuffer<int, 3> buffer;

  Expect(buffer.Push(1), "push 1 should succeed");
  Expect(buffer.Push(2), "push 2 should succeed");
  Expect(buffer.Push(3), "push 3 should succeed");

  Expect(buffer.Pop().value() == 1, "pop should return 1");

  Expect(buffer.Push(4), "push 4 should succeed after wrap-around");
  Expect(buffer.Full(), "buffer should be full again");

  Expect(buffer.Pop().value() == 2, "pop should return 2");
  Expect(buffer.Pop().value() == 3, "pop should return 3");
  Expect(buffer.Pop().value() == 4, "pop should return 4");
  Expect(buffer.Empty(), "buffer should be empty after wrap-around pops");
}

void TestStringValues() {
  llgw::RingBuffer<std::string, 2> buffer;

  Expect(buffer.Push("AAPL"), "push AAPL should succeed");
  Expect(buffer.Push("MSFT"), "push MSFT should succeed");

  Expect(buffer.Pop().value() == "AAPL", "first string should be AAPL");
  Expect(buffer.Pop().value() == "MSFT", "second string should be MSFT");
}

}  // namespace

int main() {
  TestEmptyBuffer();
  TestPushPopSingleValue();
  TestFifoOrdering();
  TestRejectPushWhenFull();
  TestWrapAroundOrdering();
  TestStringValues();

  std::cout << "All ring buffer tests passed.\n";
  return 0;
}
