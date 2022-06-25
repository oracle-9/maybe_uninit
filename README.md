# maybe_uninit
`maybe_uninit` is a C++20 wrapper type of uninitialized values.

## Installation
`maybe_uninit` is a single header file library. It can be added to your project by simply dropping it in your include directory.

## Use cases
`maybe_uninit` is useful when avoiding the default constructor of an object is desirable, either because it's not cheap, or because it's deleted.<br />
For example, given the following type:
```cpp
struct NonTrivial {
    NonTrivial() = delete;
    NonTrivial(int) {}
    ~NonTrivial() {}
};
```
It's impossible to create an array of `NonTrivial`s, as the default constructor is deleted.<br />
Workarounds include:
- allocating the array on the heap with `std::make_unique_for_overwrite`/`::operator new[]`/`std::malloc` and using placement new on each element.
This strategy adds the runtime overhead of a dynamic allocation and the complexity of `nullptr`/exception handling;
- using an `((un)signed) char`/`std::byte` array instead of a `NonTrivial` array, coupled with placement new and `std::launder`.
This strategy is verbose and extremely error-prone, as it relies on `reinterpret_cast` and pointer offset arithmetic, and assumes proper alignment and size for the byte array. There's also the issue that `reinterpret_cast` is currently (as of C++20) not a constant expression;
- using a union with a `NonTrivial` member, where the union's default constructor does not construct the `NonTrivial`.
In order to propagate exception guarantees, `noexcept` must be carefully applied.

Through the usage of `maybe_uninit`, this boilerplate can be avoided:
```cpp
mem::maybe_uninit<NonTrivial> non_trivials[10]; // NonTrivial() isn't called.

// Construction.
for (NonTrivial& nt : non_trivials) {
    nt.construct(42); // 42 is forwarded, and NonTrivial is constructed inplace inside the maybe_uninit.
}

// Destruction.
for (NonTrivial& nt : non_trivials) {
    nt.destruct();
}
```

---
### Constraints
- the type of a `maybe_uninit`'s value must be an object and a complete type.<br />
This means no `void`, no function types, no references, no unbounded arrays, ~~no capes!~~ and no incomplete struct/class types.

---
### Guarantees
- `maybe_uninit` never allocates dynamic memory on its own. However, the act of constructing a value will allocate if the value's constructor allocates;
- the value is inlined in `maybe_uninit`, there is no pointer/reference indirection;
- `sizeof(mem::maybe_uninit<T>)` is as small as it can be.
More concretly, its size is the same as the size of a union whose only member is the value.
This tipically means that `sizeof(mem::maybe_uninit<T>) == sizeof(T)`;
- `noexcept` is propagated;
- `constexpr` is applied wherever possible. As of C++20, placement new is not a constant expression.
As such, the following functions, which rely on placement new, cannot be marked `constexpr`:
  - `mem::maybe_uninit::default_construct`;
  - `mem::maybe_uninit::maybe_uninit(mem::default_init_tag_t)`;
  - `mem::default_init`.

---
## Usage
### Construction
#### Uninitialized values
To create an uninitialized `maybe_uninit`, call the default constructor.
```cpp
auto uninit = mem::maybe_uninit<std::string>();
```
#### Default construction
To default construct the value, call the member function `default_construct`:
```cpp
auto uninit = mem::maybe_uninit<int>(); // uninitialized.
uninit.default_construct(); // default initialization of int, value is garbage.
```
It's also possible to default construct the value from `maybe_uninit`'s constructor, using the tag `default_init_tag`.
```cpp
auto init = mem::maybe_uninit<int>(default_init_tag); // default initialization of int, value is garbage.
```
#### Construction from a set of parameters
To construct the value from a set of parameters, call the member function `construct` with the desired arguments:
```cpp
auto uninit = mem::maybe_uninit<int>(); // uninitialized.
uninit.construct(); // value initialzation of int, value is 0.
uninit.construct(42); // direct initialzation of int, value is 42.
```
It's also possible to construct the value from `maybe_uninit`'s constructor by simply passing the desired arguments:
```cpp
auto init = mem::maybe_uninit<int>(42);
```
**NOTE:** `mem::maybe_uninit<int>();` will not value-initialize the int, it will call `maybe_uninit`'s default constructor, which will leave it in an uninitialized state.<br />
If the desired behavior is initializing `maybe_uninit`'s value as if by value initialization, use the tag `value_init_tag`:
```cpp
auto init = mem::maybe_uninit<int>(mem::value_init_tag); // value initialzation of int, value is 0.
```
It's also provided a deduction guide for when the constructed value type is the same as the argument type.
Consequently, this is also valid:
```cpp
auto init = mem::maybe_uninit(42);
```

---
#### Free function API
The free functions `uninit`, `default_init` and `init` are also provided to construct `maybe_uninit` with less boilerplate:
```cpp
auto uninit = mem::uninit<int>();
auto default_initialized = mem::default_init<int>();
auto value_initialized   = mem::init<int>();
auto direct_initialized  = mem::init(42);

static_assert(
    std::is_same_v<decltype(uninit), maybe_uninit<int>>
        and std::is_same_v<decltype(default_initialized), maybe_uninit<int>>
        and std::is_same_v<decltype(value_initialized),   maybe_uninit<int>>
        and std::is_same_v<decltype(direct_initialized),  maybe_uninit<int>>
);
```

---
### Destruction
`maybe_uninit`'s destructor doesn't call the value's destructor, as it can't know if the value was constructed in the first place.<br />
Hence, destructing must be done manually with the member function `destruct`:
```cpp
auto init = mem::init("this must be destroyed or memory leaks will occur"s);
init.destruct();
```

---
### Accessing
To access the underlying value, call the member functions `ptr` and `assume_init`.<br />
`ptr` returns a non-null pointer to the underlying value, and `assume_init` returns a reference.<br />
Both functions are `const`/non-`const` overloaded.
`assume_init` is also overloaded for rvalue-reference.
```cpp
auto uninit = mem::uninit<std::string>(); // uninitialized.
std::string* storage = uninit.ptr();
new (storage) std::string("manually constructing a string");
uninit.destruct();

auto init = mem::init("initialized"s);
std::string& str = init.assume_init();
str.std::string::~string(); // call destructor manually.

auto moved_from = mem::init("this string will be moved out of maybe_uninit"s);
std::string str = std::move(moved_from).assume_init(); // str is move constructed.
// str's destructor will be automatically called at the end of the scope.

// destruction of a moved-from value may not be needed depending on the type.
// calling std::string::~string() on a moved-from string will probably be a nop, or at most a branch.
moved_from.destruct(); 
```

---
### Custom namespace
By default, `maybe_uninit` is placed in the namespace `mem`.<br />
This behavior can be overriden by setting the macro constant `MAYBE_UNINIT_NAMESPACE_NAME` before including the header:
```cpp
#define MAYBE_UNINIT_NAMESPACE_NAME memory

#include <maybe_uninit.hpp>

auto init = memory::init(42);
```
