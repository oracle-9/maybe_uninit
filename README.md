# maybe_uninit

`maybe_uninit` is a C++26 wrapper type of uninitialized values.

---

# Table of contents
- [Installation](#installation)
- [Use cases](#use-cases)
- [Constraints](#constraints)
- [Guarantees](#guarantees)
- [Usage](#usage)
  - [Construction](#construction)
    - [Uninitialized values](#uninitialized-values)
    - [Default construction](#default-construction)
    - [Construction from a set of parameters](#construction-from-a-set-of-parameters)
    - [Free function API](#free-function-api)
  - [Destruction](#destruction)
  - [Accessing](#accessing)
- [Custom namespace](#custom-namespace)

---

## Installation

`maybe_uninit` is a single header file library. It can be added to your project by simply dropping it in your include directory.

## Use cases

`maybe_uninit` is useful when object construction should be deferred and default construction is either not possible, semantically invalid, or expensive. For example, given the following type:
```cpp
struct NonTrivial {
    NonTrivial() = delete;
    NonTrivial(int) {}
};
```
It's impossible to create an array of `NonTrivial`s, as the default constructor is deleted. Workarounds include:
- allocating the array on the heap with, which adds the runtime overhead of a dynamic allocation and error handling complexity;
- using a byte array instead of a `NonTrivial` array and initializing with placement new, which is verbose and error-prone, as it relies on casting, pointer arithmetic, pointer laundering, and proper alignment and size for the byte array.
- using a union with a `NonTrivial` member, where the union's default constructor and destructor do nothing, which doesn't scale well, as a new union would need to be defined for every type.

Through the usage of `maybe_uninit`, this boilerplate can be avoided:
```cpp
auto non_trivials = std::array<mem::maybe_uninit<NonTrivial>, 10>; // NonTrivial() isn't called.

// Construction.
for (NonTrivial& uninit : non_trivials) {
    uninit.paren_init(42); // 42 is forwarded, and NonTrivial is constructed inplace
}                          // inside the maybe_uninit.

// Destruction.
for (NonTrivial& init : non_trivials) {
    init.destroy();
}
```

---

## Constraints

- the type of the `maybe_uninit` underlying object must be a complete object type. This means no `void`, no function types, no references, no unbounded arrays, and no incomplete struct/class types.

---

## Guarantees
- `maybe_uninit` never allocates dynamic memory on its own. However, the act of initializing an object will allocate if the object's constructor allocates;
- the value is inlined in `maybe_uninit` -- there is no pointer/reference indirection;
- `sizeof(mem::maybe_uninit<T>)` is as small as it can be.
More concretely, its size is the same as the size of a union whose only member is the object.
This typically means `sizeof(mem::maybe_uninit<T>) == sizeof(T)`;
- `noexcept` and `const` are propagated;
- everything is `constexpr`.
- `maybe_uninit` was carefully designed to provide readable error messages in case of type errors.

---

## Usage

### Construction

#### Uninitialized objects

To create an uninitialized `maybe_uninit`, use the default constructor.

```cpp
auto uninit = mem::maybe_uninit<std::string>{};
```

#### Default construction

To default initialize the object, use the member function `default_init`:

```cpp
auto uninit = mem::maybe_uninit<std::string>(); // uninitialized.
uninit.default_init(); // default constructs the string.
```

Alternatively, initialize the object using `maybe_uninit`'s constructor accepting the tag `default_init_t{}`.

```cpp
auto init = mem::maybe_uninit<std::string>(mem::default_init_t{}); // default constructs the string.
```

**NOTE**: Default initialization of POD (Plain-Old-Datatypes), such as primitives, "C structs" or arrays of such types, is equivalent to no initialization at all.
As a consequence, `default_init` and `maybe_uninit(default_init_t)` perform no initialization for those types:

```cpp
auto i = mem::maybe_uninit<int>(mem::default_init_t{}); // int has indeterminate value, reading from it is Undefined Behavior.
```

See https://en.cppreference.com/w/cpp/language/default_initialization for more information.

#### Construction from a set of parameters

To construct the object from a set of parameters, use the member functions `paren_init` and `brace_init`. `paren_init` constructs the object using parentheses (i.e. `T(args...)`), which typically results in direct-initialization (or value-initialization in case of an empty parameter pack) and `brace_init` constructs the object using curly braces (i.e. `T{args...}`), which typically results in list-initialization.

```cpp
auto i = mem::maybe_uninit<int>{}; // uninitialized.
i.paren_init(); // value-initialization of int, value is 0.
i.paren_init(42); // direct-initialization of int, value is 42.

auto v = mem::maybe_uninit<std::vector<int>>{}; // uninitialized.
v.paren_init(10uz, 42); // vector of 10 ints whose value is 42.
v.brace_init(10, 42); // vector containing 10 and 42.
```

When initializing fundamental types such as `int` or `float`, `brace_init()` emits a compile error in case of narrowing/lossy conversions, while `paren_init()` does not:

```cpp
auto i = mem::maybe_uninit<std::int32_t>{};
i.paren_init(std::numeric_limits<std::int64_t>::max()); // lossy, maybe a compile warning.
i.brace_init(std::numeric_limits<std::int64_t>::max()); // compile error.
```

Alternatively, initialize the object using `maybe_uninit`'s constructor accepting either `mem::paren_init_t{}` or `mem::brace_init_t{}`, as well as the arguments to be forwarded to the object's constructor:

```cpp
auto init = mem::maybe_uninit<int>(mem::paren_init_t{}, 42);
```

There's a deduction guide in case a single argument, apart from the tag, is supplied to the constructor. In this case, this type will be the type of the constructed object. Consequently, this is also valid:

```cpp
auto init = mem::maybe_uninit(mem::paren_init_t{}, 42);
```

---

#### Free function API

The free functions `uninit()`, `default_init()`, `paren_init()` and `brace_init()` are also provided to construct `maybe_uninit` with less boilerplate:

```cpp
auto uninit = mem::uninit<int>();
auto default_initialized = mem::default_init<int>();
auto value_initialized   = mem::paren_init<int>();
auto direct_initialized  = mem::paren_init(42);
auto list_initialized    = mem::brace_init<std::vector<int>>(1, 2, 3);
```

---

### Destruction

`maybe_uninit`'s destructor doesn't call the object's destructor, as it can't know if the object was constructed in the first place. Thus, it's up to the caller to ensure the object is destroyed if it was ever constructed. Destruction can be done with the member function `destroy()`:

```cpp
mem::paren_init("this must be destroyed or memory leaks will occur"s).destroy();
```

---

### Accessing

To access the underlying object, use the member functions `ptr()` and `ref()`.
`ptr()` returns a non-null pointer to the underlying object, and `ref()` returns a reference.
Both functions preserve the value category and constness of the `maybe_uninit` object, as well as the constness of the underlying object's type:

```cpp
auto uninit = mem::uninit<std::string>();
std::string* storage = uninit.ptr();
new (storage) std::string("manually constructing a string");
uninit.destroy();

auto init = mem::paren_init("initialized"s);
std::string& str = init.ref();
str.std::string::~string(); // call the destructor manually.

auto moved_from = mem::paren_init("this string will be moved out of maybe_uninit"s);
std::string str = std::move(moved_from).ref(); // str is move constructed.
// str's destructor will be automatically called at the end of the scope.

// destruction of a moved-from value may not be needed depending on the type.
// calling std::string::~string() on a moved-from string will probably be a nop, or at most a branch.
moved_from.destroy();
```

To access the object's representation as a span of bytes, use the member function `bytes()`:

```cpp
auto const i = mem::brace_init<std::uint32_t>(0x01'23'45'67);
auto const little_endian = bool{i.bytes[0] == std::byte{0x67}};

auto j = mem::uninit<std::uint32_t>();
std::ranges::uninitialized_fill(j.bytes(), std::byte{0xFF});
assert(j.ref() == std::uint32_t{0xFF'FF'FF'FF});
```

---

## Custom namespace

By default, `maybe_uninit` is defined in the namespace `mem`. This behavior can be overridden by setting the macro constant `MAYBE_UNINIT_NAMESPACE` before including the header:

```cpp
#define MAYBE_UNINIT_NAMESPACE memory

#include <maybe_uninit.hpp>

auto init = memory::uninit<int>();
```
