#ifndef STRING_H
#define STRING_H
#include <iostream>

class String {
 public:
  using Size = unsigned long long;

  // Constructs an empty string.
  // String foo;
  String();

  // Constructs a new string containing a given repeated character.
  String(char c, Size size);

  // Construct a string by copying the value of a nul-terminated string.
  // String foo{"Hello!"};
  String(const char* c_str);

  // Construct a string by copying a fixed number of bytes from a buffer.
  // String foo{"Hello!", 6};
  String(const char* data, Size size);

  // Rule of five - if you ever have to implement any of the next five
  // functions,  you probably need to implement the other four or you will
  // have bugs.

  // Destructor.
  //~String();

  // Copy constructor: Create a new string which is a copy of other.
  // String foo{"Hello!"};
  // String bar{foo};  // copy
  // std::cout << bar << "\n";  // shows "Hello!"
  // std::cout << foo << "\n";  // shows "Hello!"
  String(const String& other);

  // Move constructor: Create a new string from other, potentially
  // destroying other in the process. However, other must still be a valid
  // string; it will still be destructed with ~String() at some point in
  // the future so it must remain valid.
  // String foo{"Hello!"};
  // String bar{std::move(foo)};  // move
  // std::cout << bar << "\n";  // shows "Hello!"
  // std::cout << foo << "\n";  // allowed to show anything but must not crash.
  //String(String&& other);

  // Copy-assignment operator: Overwrite this string with a copy of other.
  // Take care not to have any memory leaks!
  // String foo;
  // String bar{"Hello!"};
  // foo = bar;  // copy-assign.
  // std::cout << foo << "\n" << bar << "\n";  // shows "Hello!" twice.
  //String& operator=(const String& other);

  // Move-assignment operator: Overwrite this string with the value of other,
  // potentially destroying other. The same rules apply here as for the
  // move-constructor. Take care not to have any memory leaks!
  // String foo;
  // String bar{"Hello!"};
  // foo = std::move(bar);  // move-assign.
  // std::cout << bar << "\n";  // shows "Hello!"
  // std::cout << foo << "\n";  // allowed to show anything but must not crash.
  //String& operator=(String&& other);

  // Returns a pointer to length()+1 chars, where the first length() chars are
  // the contents of the string and the last char is a nul terminator.
  // String foo{"Hello!"};
  // char* c_string = foo.data();
  // std::cout << c_string << "\n";  // shows "Hello!"
  const char* data() const;
  char* data();

  // Returns the length of the string.
  // String foo{"Hello!"};
  // std::cout << foo.length() << "\n";  // shows 6.
  Size length() const;

 private:
  // insert gubbins here.
  Size length_;
  char* first_char_;
};

// None of these functions should need access to anything except the existing
// public interface of the string to be implemented efficiently.

// output s to a stream (eg. std::cout).
std::ostream& operator<<(std::ostream& output, const String& s);

// substring from start position to end. start must be <= s.length().
String substring(const String& s, String::Size start);

// substring [start, start + length). substring indices must be fully inside s.
String substring(const String& s, String::Size start, String::Size length);

// String concatenation.
String operator+(const String& a, const String& b);

#endif // STRING_H
