#include "String.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <sstream>
#include <tuple>
#include <type_traits>

using namespace std::literals;

// Create a registry for test cases. Each test will register itself into this
// map with a name.
using Test = void();
std::map<std::string_view, Test*> tests;

// Macro for creating a test case. The test case will automatically register
// itself.
#define TEST(name)  \
  struct Test_##name {  \
    Test_##name() { tests.emplace(#name, &Test_##name::Run); }  \
    static void Run();  \
  } test_##name;  \
  void Test_##name::Run()

// In tests we can use ASSERT to verify conditions. If any ASSERT fails then the
// test fails and a message will be displayed showing the condition that failed.
struct AssertionFailure {
  AssertionFailure(const char* expression, std::string user_message) {
    std::ostringstream output;
    output << "Assertion failed: " << expression;
    if (!user_message.empty()) output << "\n" << user_message;
    message = output.str();
  }

  std::string message;
};

#define ASSERT(expr)                                     \
  for (auto [done, result, output] =                     \
           ::std::tuple{false, !static_cast<bool>(expr), \
                        ::std::ostringstream{}};         \
       result; done = true)                              \
    if (done)                                            \
      throw ::AssertionFailure{#expr, output.str()};     \
    else                                                 \
      output

// Replace the default new and delete with custom ones which track allocations.
// This allows the code to catch accidental double-deletes or memory leaks.
struct Allocation {
  enum State {
    ACTIVE,
    FREED,
  };
  void* address;
  std::size_t size;
  State state;
};

constexpr std::size_t kMaxAllocations = 1024;
std::size_t num_allocations = 0;
std::size_t total_size = 0;
Allocation allocations[kMaxAllocations];
bool force_next_allocation_failure = false;

struct {
  bool raised = false;
  void* address;
  const char* message;
} allocation_failure;

void* operator new(std::size_t size) {
  if (force_next_allocation_failure) {
    force_next_allocation_failure = false;
    throw std::bad_alloc();
  }
  void* p = std::malloc(size);
  Allocation allocation;
  allocation.size = size;
  allocation.address = p;
  allocation.state = Allocation::ACTIVE;
  total_size += size;
  auto begin = allocations, end = allocations + num_allocations;
  auto i =
      std::find_if(begin, end, [&](Allocation a) { return a.address == p; });
  if (i != end) {
    *i = allocation;
  } else if (num_allocations == kMaxAllocations) {
    throw std::bad_alloc();
  } else {
    allocations[num_allocations++] = allocation;
  }
  return allocation.address;
}

void operator delete(void* p) noexcept {
  auto begin = allocations, end = allocations + num_allocations;
  auto i =
      std::find_if(begin, end, [&](Allocation a) { return a.address == p; });
  if (i == end) {
    allocation_failure.address = p;
    allocation_failure.message =
        "Deallocated an address which was not allocated.";
    allocation_failure.raised = true;
    return;
  } else if (i->state == Allocation::FREED) {
    allocation_failure.address = p;
    allocation_failure.message =
        "Deallocated an address which was already freed.";
    allocation_failure.raised = true;
    return;
  }
  total_size -= i->size;
  i->state = Allocation::FREED;
  std::free(i->address);
}

TEST(EmptyString) {
  String empty;
  ASSERT(empty.length() == 0);
  ASSERT(empty.data()[0] == '\0');
}

TEST(FilledString) {
  String filled{'a', 3};
  ASSERT(filled.length() == 3);
  ASSERT(filled.data() == "aaa"sv);
}

TEST(NulTerminatedString) {
  const char* input = "Hello, World!";
  String cstring{input};
  ASSERT(cstring.data() == std::string_view{input});
  ASSERT(cstring.data() != input) << "String should allocate its own buffer.";
}

TEST(StringCopy) {
  String foo{"foo"};
  String copy = foo;
  ASSERT(foo.data() == "foo"sv);
  ASSERT(copy.data() == "foo"sv);
  ASSERT(foo.data() != copy.data())
      << "String copy should have its own buffer.";
}

TEST(StringMove) {
  String foo{"foo"};
  const char* data = foo.data();
  String moved = std::move(foo);
  ASSERT(moved.data() == data)
      << "String move should steal the existing buffer.";

  // We want to test that an allocation failure when trying to move-construct
  // string will leave the source string in a valid state. I can't think of any
  // good way of testing this without assuming an implementation except to force
  // the code path to happen and see if it causes a segmentation fault. Most of
  // the logic in this section is just checking that the exception actually
  // happened and making sure it doesn't happen elsewhere accidentally.
  force_next_allocation_failure = true;
  bool had_exception = false;
  try {
    String moved_again = std::move(moved);
  } catch (const std::bad_alloc&) {
    had_exception = true;
  }
  force_next_allocation_failure = false;
  ASSERT(had_exception) << "Whoops! The test isn't working properly :(";
}

TEST(CopyAssign) {
  String foo{"foo"};
  String bar;
  bar = foo;
  ASSERT(foo.data() == "foo"sv);
  ASSERT(bar.data() == "foo"sv);
  ASSERT(foo.data() != bar.data())
      << "String copy should have its own buffer.";
  bar = bar;  // This shouldn't explode but concievably could...
  ASSERT(bar.data() == "foo"sv);

  String baz{'!', 128};  // has to be long enough to force allocation.

  // This part is a little intricate. We want to test that an allocation failure
  // when trying to copy-assign a string will leave the destination string
  // untouched. To do this, we force the custom allocator to fail its next
  // allocation. Most of the logic in this section is just checking that the
  // exception actually happened and making sure it doesn't happen elsewhere
  // accidentally.
  force_next_allocation_failure = true;
  bool had_exception = false;
  try { bar = baz; } catch (const std::bad_alloc&) { had_exception = true; }
  force_next_allocation_failure = false;
  ASSERT(had_exception) << "Whoops! The test isn't working properly :(";

  ASSERT(bar.data() == "foo"sv)
      << "Copy-assign with failed allocation should have no effect.";
}

TEST(MoveAssign) {
  String foo{"foo"};
  const char* data = foo.data();
  String bar;
  bar = std::move(foo);
  ASSERT(bar.data() == data)
      << "String move should steal the existing buffer.";
  bar = bar;  // This shouldn't explode but concievably could...
  ASSERT(bar.data() == "foo"sv);
}

// We want to check that data() returns a pointer to const char, not to char. To
// do this, we pass it to IsConst and rely on overload resolution to pick the
// right version. The static_assert tests below prove that this will do what we
// want it to.
constexpr bool IsConst(char* value) { return false; }
constexpr bool IsConst(const char* value) { return true; }

char x = 0;
const char y = 0;
static_assert(!IsConst(&x));
static_assert(IsConst(&y));

TEST(ConstAccess) {
  const String foo{"foo"};
  ASSERT(IsConst(foo.data()))
      << "data() on a const String should be const char*, not char*.";
  // If const is set up wrong, these lines won't compile at all.
  const char* data = foo.data();
  auto length = foo.length();
}

TEST(MutableData) {
  String foo{"foo"};
  ASSERT(!std::is_const_v<decltype(foo.data())>)
      << "data() on a non-const String should be non-const.";
}

// // This test will fail because of memory corruption.
// TEST(DoubleDelete) {
//   int* a = new int;
//   delete a;
//   delete a;
// }

// // This test will fail because it leaks memory.
// TEST(Leak) {
//   int* b = new int;
// }

// Function to run a test case, catching any assertions which fail and verifying
// memory allocation.
void RunTest(std::string_view name, Test* test) {
  constexpr char kGreen[] = "\x1B[32m";
  constexpr char kYellow[] = "\x1B[33m";
  constexpr char kRed[] = "\x1B[31m";
  constexpr char kReset[] = "\x1B[0m";
  std::cout << kYellow << name << kReset << ": ";
  force_next_allocation_failure = false;
  allocation_failure.raised = false;
  try {
    auto heap_before = total_size;
    test();
    auto heap_after = total_size;
    ASSERT(heap_before == heap_after)
        << (heap_after - heap_before) << " byte(s) of memory were leaked.";
    if (allocation_failure.raised) {
      std::ostringstream output;
      output << "Address: " << allocation_failure.address;
      throw AssertionFailure{allocation_failure.message, output.str()};
    }
    std::cout << kGreen << "PASSED" << kReset << "\n";
  } catch (const AssertionFailure& failure) {
    std::cout << kRed << "FAILED" << kReset << "\n" << failure.message << "\n";
  }
}

int main() {
  for (auto [name, test] : tests) RunTest(name, test);
}
