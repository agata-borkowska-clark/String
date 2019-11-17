#include "../include/String.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <sstream>
#include <tuple>
#include <type_traits>

using namespace std::literals;

constexpr char kCyan[] = "\x1B[36m";
constexpr char kGreen[] = "\x1B[32m";
constexpr char kYellow[] = "\x1B[33m";
constexpr char kRed[] = "\x1B[31m";
constexpr char kReset[] = "\x1B[0m";

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
  AssertionFailure(const char* file, int line, const char* expression,
                   std::string user_message) {
    std::ostringstream output;
    output << "Assertion failed at " << file << ":" << line << ": "
           << kCyan << expression << kReset;
    if (!user_message.empty()) output << "\n" << user_message;
    message = output.str();
  }

  std::string message;
};

template <typename T> std::string Dump(T value) {
  std::ostringstream output;
  output << value;
  return output.str();
}

template <> std::string Dump(char c) {
  std::ostringstream output;
  output << "'" << c << "'";
  return output.str();
}

template <> std::string Dump(std::string_view sv) {
  std::ostringstream output;
  output << "\"" << sv << "\"";
  return output.str();
}

std::string Dump(const char* c_str) { return Dump(std::string_view{c_str}); }
std::string Dump(char* c_str) { return Dump(std::string_view{c_str}); }
std::string Dump(std::string str) { return Dump(std::string_view{str}); }

template <typename A, typename B>
struct AssertEqual {
  using CommonType = typename std::common_type<A, B>::type;
  AssertEqual(const char* file, int line, const char* a_str, const A& a,
              const char* b_str, const B& b)
      : file(file),
        line(line),
        a_str(a_str),
        a_val(Dump(a)),
        b_str(b_str),
        b_val(Dump(b)),
        equal(static_cast<CommonType>(a) == static_cast<CommonType>(b)) {}

  const char* file;
  int line;
  const char* a_str;
  std::string a_val;
  const char* b_str;
  std::string b_val;
  bool equal;
  std::ostringstream message;
  bool message_collected = false;

  AssertionFailure Build() {
    std::ostringstream output;
    output << kCyan << a_str << " == " << b_str << kReset << "\n"
           << "Left: " << kCyan << a_val << kReset << "\n"
           << "Right: " << kCyan << b_val << kReset;
    return AssertionFailure(file, line, output.str().c_str(), message.str());
  }
};

#define ASSERT(expr)                                                     \
  for (auto [done, result, output] =                                     \
           ::std::tuple{false, !static_cast<bool>(expr),                 \
                        ::std::ostringstream{}};                         \
       result; done = true)                                              \
    if (done)                                                            \
      throw ::AssertionFailure{__FILE__, __LINE__, #expr, output.str()}; \
    else                                                                 \
      [[maybe_unused]] const auto& internal_stream_result_ = output

#define ASSERT_EQ(a, b)                                               \
  for (AssertEqual assert_data{__FILE__, __LINE__, #a, (a), #b, (b)}; \
       !assert_data.equal; assert_data.message_collected = true)      \
    if (assert_data.message_collected)                                \
      throw assert_data.Build();                                      \
    else                                                              \
      [[maybe_unused]] const auto& internal_stream_result_ = assert_data.message

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
  std::memset(p, '!', size);
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

void DoDelete(void* p, std::size_t s, bool check_size) noexcept {
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
  } else if (check_size && i->size != s) {
    allocation_failure.address = p;
    allocation_failure.message =
        "Performed a sized deallocation with the wrong size.";
    allocation_failure.raised = true;
    return;
  }
  total_size -= i->size;
  i->state = Allocation::FREED;
  std::memset(i->address, '?', i->size);
}

void operator delete(void* p) noexcept { DoDelete(p, 0, false); }
void operator delete(void* p, std::size_t s) noexcept { DoDelete(p, s, true); }

TEST(EmptyString) {
  String empty;
  ASSERT_EQ(empty.length(), 0);
  ASSERT_EQ(empty.data()[0], '\0');
}

TEST(FilledString) {
  String filled{'a', 3};
  ASSERT_EQ(filled.length(), 3);
  ASSERT_EQ(filled.data(), "aaa"sv);
  ASSERT_EQ(filled.data()[3], '\0');
}

TEST(NulTerminatedString) {
  const char* input = "Hello, World!";
  String cstring{input};
  ASSERT_EQ(cstring.data(), std::string_view{input});
  ASSERT(cstring.data() != input) << "String should allocate its own buffer.";
}

TEST(StringCopy) {
  String foo{"foo"};
  String copy = foo;
  ASSERT_EQ(foo.data(), "foo"sv);
  ASSERT_EQ(copy.data(), "foo"sv);
  ASSERT(foo.data() != copy.data())
      << "String copy should have its own buffer.";
}

TEST(StringMove) {
  String foo{"foo"};
  const char* data = foo.data();
  String moved = std::move(foo);
  ASSERT_EQ(moved.data(), data)
      << "String move should steal the existing buffer.";
}

TEST(StringMoveWithFailedAllocation) {
  // We want to test that an allocation failure when trying to move-construct
  // string will leave the source string in a valid state. I can't think of any
  // good way of testing this without assuming an implementation except to force
  // the code path to happen and see if it causes a segmentation fault. Most of
  // the logic in this section is just checking that the exception actually
  // happened and making sure it doesn't happen elsewhere accidentally.
  String foo{"foo"};
  force_next_allocation_failure = true;
  bool had_exception = false;
  try {
    String moved = std::move(foo);
    (void) moved;
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
  ASSERT_EQ(foo.data(), "foo"sv);
  ASSERT_EQ(bar.data(), "foo"sv);
  ASSERT(foo.data() != bar.data())
      << "String copy should have its own buffer.";
}

TEST(CopyAssignSelf) {
  // Test self-assignment. This indirects via a reference to avoid a warning.
  String foo{"foo"};
  auto& x = foo;
  foo = x;
  ASSERT_EQ(foo.data(), "foo"sv) << "Self-copy-assignment is broken.";
}

TEST(CopyAssignWithFailedAllocation) {
  String foo;
  String bar{'#', 128};  // has to be long enough to force allocation.
  const char* foo_data = foo.data();

  // This part is a little intricate. We want to test that an allocation failure
  // when trying to copy-assign a string will leave the destination string
  // untouched. To do this, we force the custom allocator to fail its next
  // allocation. Most of the logic in this section is just checking that the
  // exception actually happened and making sure it doesn't happen elsewhere
  // accidentally.
  force_next_allocation_failure = true;
  bool had_exception = false;
  try { foo = bar; } catch (const std::bad_alloc&) { had_exception = true; }
  force_next_allocation_failure = false;
  ASSERT(had_exception) << "Whoops! The test isn't working properly :(";
  ASSERT_EQ(foo.data(), foo_data) << "Copy-assign with failed allocation "
                                     "should leave the destination unchanged.";
}

TEST(MoveAssign) {
  String foo{"foo"};
  const char* data = foo.data();
  String bar;
  bar = std::move(foo);
  ASSERT_EQ(bar.data(), data)
      << "String move should steal the existing buffer.";
}

TEST(MoveAssignSelf) {
  // Test self-assignment. This indirects via a reference to avoid a warning.
  String foo{"foo"};
  auto& x = foo;
  foo = x;
  ASSERT_EQ(foo.data(), "foo"sv) << "Self-move-assignment is broken.";
}

TEST(MoveAssignWithFailedAllocation) {
  String foo;
  String bar{'!', 128};  // has to be long enough to force allocation.
  const char* foo_data = foo.data();
  const char* bar_data = bar.data();

  // This part is a little intricate. We want to test that an allocation failure
  // when trying to copy-assign a string will leave the destination string
  // untouched. To do this, we force the custom allocator to fail its next
  // allocation. Most of the logic in this section is just checking that the
  // exception actually happened and making sure it doesn't happen elsewhere
  // accidentally.
  force_next_allocation_failure = true;
  bool had_exception = false;
  try {
    foo = std::move(bar);
  } catch (const std::bad_alloc&) {
    had_exception = true;
  }
  if (force_next_allocation_failure) {
    // This can only happen if the implementation never invoked new. This is
    // perfectly reasonable, so we can just skip the test in this case.
    return;
  }
  force_next_allocation_failure = false;
  ASSERT(had_exception) << "Whoops! The test isn't working properly :(";
  ASSERT_EQ(foo.data(), foo_data) << "Move-assign with failed allocation "
                                     "should leave the destination unchanged.";
  ASSERT_EQ(bar.data(), bar_data) << "Move-assign with failed allocation "
                                     "should leave the source unchanged.";
}

// We want to check that data() returns a pointer to const char, not to char. To
// do this, we pass it to IsConst and rely on overload resolution to pick the
// right version. The static_assert tests below prove that this will do what we
// want it to.
constexpr bool IsConst(char*) { return false; }
constexpr bool IsConst(const char*) { return true; }

char x = 0;
const char y = 0;
static_assert(!IsConst(&x));
static_assert(IsConst(&y));

TEST(ConstAccess) {
  const String foo{"foo"};
  ASSERT(IsConst(foo.data()))
      << "data() on a const String should be const char*, not char*.";
  // If const is set up wrong, these lines won't compile at all.
  [[maybe_unused]] const char* data = foo.data();
  [[maybe_unused]] auto length = foo.length();
}

TEST(MutableData) {
  String foo{"foo"};
  ASSERT(!std::is_const_v<decltype(foo.data())>)
      << "data() on a non-const String should be non-const.";
}

TEST(StringsWithZeros) {
  String foo('\0', 3);
  ASSERT_EQ(foo.length(), 3) << "Strings should be able to hold '\\0'.";
}

TEST(Output) {
  std::ostringstream foo_output;
  auto text = "Hello, World!"sv;
  String foo{text.data(), text.length()};
  foo_output << foo;
  ASSERT_EQ(foo_output.str(), text);

  std::ostringstream bar_output;
  auto text_with_zero = "Hello\0World"sv;
  String bar{text_with_zero.data(), text_with_zero.length()};
  bar_output << bar;
  ASSERT_EQ(bar_output.str(), text_with_zero)
      << "Output should support strings with '\\0' in them.";
}

TEST(BasicSubstring) {
  String foo{"Nobody thinks that Joe is awesome."};
  String bar{substring(foo, 19)};
  ASSERT_EQ(bar.data(), "Joe is awesome."sv);
}

TEST(DualSubstring) {
  String foo{"It is widely accepted that C++ is fantastically hard to use."};
  String bar{substring(foo, 27, 16)};
  ASSERT_EQ(bar.data(), "C++ is fantastic"sv);
}

TEST(SubstringWithZeros) {
  String zeros{'\0', 20};
  ASSERT_EQ(substring(zeros, 5).length(), 15);
}

TEST(Concat) {
  String hello{"Hello, "};
  String world{"World!"};
  ASSERT_EQ((hello + world).data(), "Hello, World!"sv);
}

TEST(ConcatWithZeros) {
  ASSERT_EQ((String('\0', 5) + String('\0', 5)).length(), 10);
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
bool RunTest(std::string_view name, Test* test) {
  force_next_allocation_failure = false;
  allocation_failure.raised = false;
  try {
    auto heap_before = total_size;
    test();
    auto heap_after = total_size;
    if (allocation_failure.raised) {
      std::ostringstream output;
      output << "Address: " << allocation_failure.address;
      throw AssertionFailure{__FILE__, __LINE__, allocation_failure.message,
                             output.str()};
    }
    ASSERT_EQ(heap_before, heap_after)
        << (heap_after - heap_before) << " byte(s) of memory were leaked.";
    return true;
  } catch (const AssertionFailure& failure) {
    std::cout << kYellow << name << kReset << ": " << kRed << "FAILED" << kReset
              << "\n"
              << failure.message << "\n";
    return false;
  } catch (const std::exception& error) {
    std::cout << kYellow << name << kReset << ": " << kRed << "FAILED" << kReset
              << "\nUnhandled exception: " << error.what() << "\n";
    return false;
  } catch (...) {
    std::cout << kYellow << name << kReset << ": " << kRed << "FAILED" << kReset
              << "\nUnhandled (and unrecognised) exception.\n";
    return false;
  }
}

int main() {
  int passes = 0, failures = 0;
  for (auto [name, test] : tests) (RunTest(name, test) ? passes : failures)++;
  std::cout << kGreen << passes << " pass" << (passes == 1 ? "" : "es")
            << kReset << ", " << kRed << failures << " failure"
            << (failures == 1 ? "" : "s") << kReset << ".\n";
  return failures;
}
