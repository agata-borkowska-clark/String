#include <iostream>
#include "../include/String.h"

using Size = unsigned long long;

// Constructs an empty string.
// String foo;
String::String() {
  first_char_ = new char[1];
  first_char_[0] = '\0';
  length_ = 0;
}

// Constructs a new string containing a given repeated character.
String::String(char c, Size size) {
  length_ = size;
  first_char_ = new char[length_ + 1]();
  for (Size i = 0; i < length_; i++) {
    first_char_[i] = c;
  }
  first_char_[length_] = '\0';
}

// Construct a string by copying the value of a null-terminated string.
// String foo{"Hello!"};
String::String(const char* c_str) {
  // tis inefficient
  length_ = 0;
  const char* current = c_str;
  while (*current != '\0') {
    length_++;
    current++;
  }
  current = c_str;
  first_char_ = new char[length_ + 1];
  for (Size i = 0; i < length_; i ++) {
    first_char_[i] = *current;
    current++;
  }
  first_char_[length_] = '\0';
}


// Construct a string by copying a fixed number of bytes from a buffer.
// String foo{"Hello!", 6};
String::String(const char* data, Size size) {
  length_ = size;
  first_char_ = new char[length_ +1]();
  const char* current = data;
  for (Size i = 0; i < length_; i++) {
    first_char_[i] = *current;
    current++;
  }
  first_char_[length_] = '\0';
}

// Rule of five - if you ever have to implement any of the next five
// functions,  you probably need to implement the other four or you will
// have bugs.

// Destructor.
String::~String() {
  delete[] first_char_;
}

// Copy constructor: Create a new string which is a copy of other.
// String foo{"Hello!"};
// String bar{foo};  // copy
// std::cout << bar << "\n";  // shows "Hello!"
// std::cout << foo << "\n";  // shows "Hello!"
String::String(const String& other) {
  length_ = other.length();
  first_char_ = new char[length_ + 1]();
  const char* current = other.data();
  for (Size i = 0; i < length_; i++) {
    first_char_[i] = *current;
    ++current;
  }
}

// Move constructor: Create a new string from other, potentially
// destroying other in the process. However, other must still be a valid
// string; it will still be destructed with ~String() at some point in
// the future so it must remain valid.
// String foo{"Hello!"};
// String bar{std::move(foo)};  // move
// std::cout << bar << "\n";  // shows "Hello!"
// std::cout << foo << "\n";  // allowed to show anything but must not crash.
String::String(String&& other) {
  first_char_ = other.data();
  length_ = other.length();
  other.first_char_ = new char[1]{'\0'};
  other.length_ = 0;
}

// Copy-assignment operator: Overwrite this string with a copy of other.
// Take care not to have any memory leaks!
// String foo;
// String bar{"Hello!"};
// foo = bar;  // copy-assign.
// std::cout << foo << "\n" << bar << "\n";  // shows "Hello!" twice.
String& String::operator=(const String& other) {
  if (this != &other) {
    char* temp = new char[other.length() + 1];
    length_ = other.length();
    delete first_char_;
    first_char_ = temp;
    const char* current = other.data();
    for (Size i = 0; i < length_; i++) {
      first_char_[i] = *current;
      ++current;
    }
    first_char_[length_] = '\0';
  }
  return *this;
}

// Move-assignment operator: Overwrite this string with the value of other,
// potentially destroying other. The same rules apply here as for the
// move-constructor. Take care not to have any memory leaks!
// String foo;
// String bar{"Hello!"};
// foo = std::move(bar);  // move-assign.
// std::cout << bar << "\n";  // shows "Hello!"
// std::cout << foo << "\n";  // allowed to show anything but must not crash.
String& String::operator=(String&& other) {
  // first_char_ = new char[1]{'\0'};
  first_char_ = other.data();
  length_ = other.length();
  String* temp = new String();
  other.first_char_ = temp->first_char_;
  other.length_ = 0;
  return *this;
}

// Returns a pointer to length()+1 chars, where the first length() chars are
// the contents of the string and the last char is a nul terminator.
// String foo{"Hello!"};
// char* c_string = foo.data();
// std::cout << c_string << "\n";  // shows "Hello!"
const char* String::data() const {
  return first_char_;
}

char* String::data() {
  return first_char_;
}

// Returns the length of the string.
// String foo{"Hello!"};
// std::cout << foo.length() << "\n";  // shows 6.
Size String::length() const {
  return length_;
}


// None of these functions should need access to anything except the existing
// public interface of the string to be implemented efficiently.

// output s to a stream (eg. std::cout).
std::ostream& operator<<(std::ostream& output, const String& s) {
  output.write(s.data(), s.length());
  return output;
}

// substring from start position to end. start must be <= s.length().
String substring(const String& s, String::Size start) {
  if (start >= s.length()) {
    return String();
  }
  return String(s.data() + start);
}

// substring [start, start + length). substring indices must be fully inside s.
String substring(const String& s, String::Size start, String::Size length) {
  if (start >= s.length() - length) {
    return String();
  }
  return String(s.data() + start, length);
}

// String concatenation.
String operator+(const String&, const String&) {
  return String();
}
