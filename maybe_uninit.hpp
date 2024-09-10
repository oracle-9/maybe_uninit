/// @file
/// @brief Defines the template type `maybe_uninit`, as well as helper functions and deduction
/// guides.

#pragma once

#include <cstddef>
#include <memory>
#include <new>    // IWYU pragma: keep, false positive (need ::new).
#include <ranges> // IWYU pragma: keep, false positive (std::ranges::borrowed_range).
#include <span>
#include <type_traits>
#include <utility>

/// @brief Namespace name to use for the library. Defaults to `mem`, but can be overriden.
#ifndef MAYBE_UNINIT_NAMESPACE
#   define MAYBE_UNINIT_NAMESPACE mem
#endif

namespace MAYBE_UNINIT_NAMESPACE {

namespace detail {

/// @brief
/// [std::is_default_constructible](https://en.cppreference.com/w/cpp/types/is_default_constructible), but without
/// checking whether `T` is destructible.
template <typename T>
concept default_constructible = requires(T* p) { ::new (static_cast<void*>(p)) T; };

/// @brief [std::is_constructible](https://en.cppreference.com/w/cpp/types/is_constructible), but without checking
/// whether `T` is destructible.
template <typename T, typename... Args>
concept paren_constructible_from
    = requires(T* p, Args&&... args) { ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...); };

/// @brief [std::is_constructible](https://en.cppreference.com/w/cpp/types/is_constructible), but using braces `{}`
/// instead of parentheses `()` and without checking whether `T` is destructible.
template <typename T, typename... Args>
concept brace_constructible_from
    = requires(T* p, Args&&... args) { ::new (static_cast<void*>(p)) T{std::forward<Args>(args)...}; };

/// @brief `default_constructible` and also noexcept.
template <typename T>
concept nothrow_default_constructible = requires(T* p) {
    { ::new (static_cast<void*>(p)) T } noexcept;
};

/// @brief `paren_constructible_from` and also noexcept.
template <typename T, typename... Args>
concept nothrow_paren_constructible_from = requires(T* p, Args&&... args) {
    { ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...) } noexcept;
};

/// @brief `brace_constructible_from` and also noexcept.
template <typename T, typename... Args>
concept nothrow_brace_constructible_from = requires(T* p, Args&&... args) {
    { ::new (static_cast<void*>(p)) T{std::forward<Args>(args)...} } noexcept;
};

/// @brief Matches a non-reference, non-void, non-function type whose size is known.
template <typename T>
concept sized = std::is_object_v<T> and requires { sizeof(T); };

/// @brief Matches `T const&` and `T const&&`.
template <typename T>
concept const_ref = std::is_reference_v<T> and std::is_const_v<std::remove_reference_t<T>>;

} // namespace detail

/// @brief Tag type used disambiguate the `maybe_uninit` constructor performing default initialization.
struct default_init_t {};

/// @brief Tag type used disambiguate the `maybe_uninit` constructor performing parentheses initialization, i.e.
/// `T(args...)`.
struct paren_init_t {};

/// @brief Tag type used disambiguate the `maybe_uninit` constructor performing brace initialization, i.e. `T{args...}`.
struct brace_init_t {};

/// @brief Constexpr wrapper of uninitialized values.
/// @details `maybe_uninit` is useful when object construction should be deferred and default construction is either not
/// possible, semantically invalid, or expensive. For example, given the following type:
/// @code {.cpp}
///     struct NonTrivial {
///         NonTrivial() = delete;
///         NonTrivial(int) {}
///     };
/// @endcode
/// It's impossible to create an array of `NonTrivial`s, as the default constructor is deleted.
/// Workarounds include:
/// - allocating the array on the heap with, which adds the runtime overhead of a dynamic allocation and error handling
/// complexity;
/// - using a byte array instead of a `NonTrivial` array and initializing with placement new, which is verbose and
/// error-prone, as it relies on casting, pointer arithmetic, pointer laundering, and proper alignment and size for the
/// byte array.
/// - using a union with a `NonTrivial` member, where the union's default constructor and destructor do nothing, which
/// doesn't scale well, as a new union would need to be defined for every type.
/// @details Through the usage of `maybe_uninit`, this boilerplate can be avoided:
/// @code {.cpp}
///     auto non_trivials = std::array<maybe_uninit<NonTrivial>, 10>{}; // NonTrivial() isn't called.
///
///     // Construction.
///     for (NonTrivial& uninit : non_trivials) {
///         uninit.paren_init(42); // 42 is forwarded, and NonTrivial is constructed inplace
///     }                          // inside the maybe_uninit.
///
///     // Destruction.
///     for (NonTrivial& init : non_trivials) {
///         init.destroy();
///     }
/// @endcode
/// @tparam T Type of the value.
/// @pre `T` is a complete [object](https://en.cppreference.com/w/cpp/types/is_object) type.
template <detail::sized T>
// Copy/move constructors/assignment operators are already implicitly defined for trivial types, and deleted for
// non-trivial types, as expected.
// NOLINTNEXTLINE(hicpp-special-member-functions)
union maybe_uninit {
  public:
    /// @brief Default constructor. Performs no initialization on the object.
    constexpr maybe_uninit() noexcept {}

    /// @brief Default initializes the object via `default_init()`.
    /// @param[in] default_init_t Disambiguation tag.
    /// @see `default_init()`
    explicit constexpr maybe_uninit(default_init_t) noexcept(detail::nothrow_default_constructible<T>)
        requires detail::default_constructible<T>
    {
        this->default_init();
    }

    /// @brief Initializes the object via `paren_init()`.
    /// @tparam ...Args Types of the arguments to initialize the object with.
    /// @param[in] paren_init_t Disambiguation tag.
    /// @param args Arguments to forward to the constructor of the object.
    /// @see `paren_init()`
    template <typename... Args>
    explicit constexpr maybe_uninit(
        paren_init_t,
        Args&&... args
    ) noexcept(detail::nothrow_paren_constructible_from<T, Args...>)
        requires detail::paren_constructible_from<T, Args...>
    {
        this->paren_init(std::forward<Args>(args)...);
    }

    /// @brief Initialization the object via `brace_init()`.
    /// @tparam ...Args Types of the arguments to initialize the object with.
    /// @param[in] brace_init_t Disambiguation tag.
    /// @param args Arguments to forward to the constructor of the object.
    /// @see `brace_init()`
    template <typename... Args>
    explicit constexpr maybe_uninit(
        brace_init_t,
        Args&&... args
    ) noexcept(detail::nothrow_brace_constructible_from<T, Args...>)
        requires detail::brace_constructible_from<T, Args...>
    {
        this->brace_init(std::forward<Args>(args)...);
    }

    /// @brief Destructor for trivial `T`s. Destruction is redundant for these types, so defaulting it trivializes
    /// `maybe_uninit` in these cases.
    constexpr ~maybe_uninit()
        requires std::is_trivially_destructible_v<T>
    = default;

    /// @brief Destructor for non-trivial `T`s. Performs no destruction.
    /// @attention It's up to the caller to ensure the object's destructor is invoked if the object was constructed in
    /// the first place.
    constexpr ~maybe_uninit() {}

    /// @brief Default initializes the object.
    /// @returns A reference to the constructed object.
    /// @pre `T` is default constructible as if by `::new (p) T`, where `p` is a `void*` to storage suitably aligned to
    /// hold an object of type `T`.
    /// @note Propagates exceptions thrown by `T`'s default constructor.
    template <typename Self>
    // rvalue-ref to lvalue-ref decay is intentional, to allow taking the address of self.object when self is an rvalue
    // reference.
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    constexpr auto default_init(this Self&& self) noexcept(detail::nothrow_default_constructible<T>) -> T&
        requires detail::default_constructible<T>
    {
        return *::new (static_cast<void*>(std::addressof(self.object))) T;
    }

    /// @brief Initializes the object as if by `T(std::forward<Args>(args)...)`.
    /// @tparam ...Args Types of the arguments to initialize the object with.
    /// @param args Arguments to forward to the constructor of the object.
    /// @returns A reference to the constructed object.
    /// @pre `T` is constructible from @p args as if by `::new (p) T(std::forward<Args>(args)...)`, where `p` is a
    /// `void*` to storage suitably aligned to hold an object of type `T`.
    /// @note Propagates exceptions thrown by `T`'s selected constructor.
    template <typename Self, typename... Args>
    constexpr auto
    // rvalue-ref to lvalue-ref decay is intentional, to allow taking the address of self.object when self is an rvalue
    // reference.
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    paren_init(this Self&& self, Args&&... args) noexcept(detail::nothrow_paren_constructible_from<T, Args...>) -> T&
        requires detail::paren_constructible_from<T, Args...>
    {
        return *::new (static_cast<void*>(std::addressof(self.object))) T(std::forward<Args>(args)...);
    }

    /// @brief Initializes the object as if by `T{std::forward<Args>(args)...}`.
    /// @tparam ...Args Types of the arguments to initialize the object with.
    /// @param args Arguments to forward to the constructor of the object.
    /// @returns A reference to the constructed object.
    /// @pre `T` is constructible from @p args as if by `::new (p) T{std::forward<Args>(args)...}`, where `p` is a
    /// `void*` to storage suitably aligned to hold an object of type `T`.
    /// @note Propagates exceptions thrown by `T`'s selected constructor.
    template <typename Self, typename... Args>
    constexpr auto
    // rvalue-ref to lvalue-ref decay is intentional, to allow taking the address of self.object when self is an rvalue
    // reference.
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    brace_init(this Self&& self, Args&&... args) noexcept(detail::nothrow_brace_constructible_from<T, Args...>) -> T&
        requires detail::brace_constructible_from<T, Args...>
    {
        return *::new (static_cast<void*>(std::addressof(self.object))) T{std::forward<Args>(args)...};
    }

    /// @brief Returns a pointer to the possibly uninitialized object, preserving the constness of
    /// @p self.
    /// @attention It's up to the caller to ensure accesses to the object through this pointer do not occur beyond the
    /// object's lifetime.
    template <typename Self>
    [[nodiscard]]
    // rvalue-ref to lvalue-ref decay is intentional, to allow taking the address of self.object when self is an rvalue
    // reference.
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    constexpr auto ptr(this Self&& self) noexcept -> auto* {
        return std::addressof(self.object);
    }

    /// @brief Returns a reference to the possibly uninitialized object, preserving the value category of @p self.
    /// @attention It's up to the caller to ensure accesses to the object through this reference do not occur beyond the
    /// object's lifetime.
    template <typename Self>
    [[nodiscard]]
    constexpr auto ref(this Self&& self) noexcept -> auto&& {
        return std::forward<Self>(self).object;
    }

    /// @brief Returns a byte span aliased to the possibly uninitialized object, preserving the constness of `Self` and
    /// `T`.
    /// @attention It's up to the caller to ensure accesses to the object representation through this span do not occur
    /// beyond the object's lifetime.
    template <typename Self>
    [[nodiscard]]
    // rvalue-ref to lvalue-ref decay is intentional, to allow taking the address of self.object when self is an rvalue
    // reference.
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    constexpr auto bytes(this Self&& self) noexcept -> std::ranges::borrowed_range auto {
        using byte_type = std::conditional_t<detail::const_ref<Self> or std::is_const_v<T>, std::byte const, std::byte>;
        return std::span<byte_type, sizeof(T)>(
            // Required to access object representation of self.object.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            reinterpret_cast<byte_type*>(std::addressof(self.object)),
            sizeof(T)
        );
    }

    /// @brief Destroys the possibly uninitialized object by invoking its destructor.
    /// @attention The object is assumed to be constructed when this function is invoked.
    template <typename Self>
    // rvalue-ref to lvalue-ref decay is intentional, to allow taking the address of self.object when self is an rvalue
    // reference.
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    constexpr auto destroy(this Self&& self) noexcept(std::is_nothrow_destructible_v<T>) {
        if constexpr (not std::is_trivially_destructible_v<T>) {
            std::destroy_at(std::addressof(self.object));
        }
    }

  private:
    /// @brief The wrapped object.
    T object;
};

/// @brief Deduction guide that allows type deduction from a single argument, where the deduced `maybe_uninit`'s
/// underlying object type is `T`.
/// @tparam T Type of the iterators.
/// @relatedalso maybe_uninit
template <detail::sized T>
maybe_uninit(paren_init_t, T) -> maybe_uninit<T>;

/// @brief Deduction guide that allows type deduction from a single argument, where the deduced `maybe_uninit`'s
/// underlying object type is `T`.
/// @tparam T Type of the iterators.
/// @relatedalso maybe_uninit
template <detail::sized T>
maybe_uninit(brace_init_t, T) -> maybe_uninit<T>;

/// @brief Deduction guide that allows type deduction from a single argument, where the deduced `maybe_uninit`'s
/// underlying object type is `T`.
/// @tparam T Type of the iterators.
/// @relatedalso maybe_uninit
template <detail::sized T>
constexpr auto uninit() noexcept -> maybe_uninit<T> {
    return maybe_uninit<T>();
}

/// @brief Shorthand for `maybe_uninit<T>(paren_init_t{}, std::forward<Args>(args)...)`.
/// @relatedalso maybe_uninit
template <detail::sized T>
constexpr auto default_init() noexcept(detail::nothrow_default_constructible<T>) -> maybe_uninit<T>
    requires detail::default_constructible<T>
{
    return maybe_uninit<T>{default_init_t{}};
}

/// @brief Shorthand for `maybe_uninit<T>(paren_init_t{}, std::forward<Args>(args)...)`.
/// @relatedalso maybe_uninit
template <detail::sized T, typename... Args>
constexpr auto paren_init(Args&&... args) noexcept(detail::nothrow_paren_constructible_from<T, Args...>)
    -> maybe_uninit<T>
    requires detail::paren_constructible_from<T, Args...>
{
    return maybe_uninit<T>{paren_init_t{}, std::forward<Args>(args)...};
}

/// @brief Shorthand for `maybe_uninit<T>(paren_init_t{}, std::forward<T>(t))`.
/// @note Single argument case is singled out to facilitate type deduction.
/// @relatedalso maybe_uninit
template <detail::sized T>
constexpr auto paren_init(T&& t) noexcept(detail::nothrow_paren_constructible_from<std::remove_reference_t<T>, T>)
    -> maybe_uninit<std::remove_reference_t<T>>
    requires detail::paren_constructible_from<std::remove_reference_t<T>, T>
{
    return maybe_uninit<std::remove_reference_t<T>>{paren_init_t{}, std::forward<T>(t)};
}

/// @brief Shorthand for `maybe_uninit<T>{paren_init_t{}, std::forward<Args>(args)...}`.
/// @relatedalso maybe_uninit
template <detail::sized T, typename... Args>
constexpr auto brace_init(Args&&... args) noexcept(detail::nothrow_brace_constructible_from<T, Args...>)
    -> maybe_uninit<T>
    requires detail::brace_constructible_from<T, Args...>
{
    return maybe_uninit<T>{brace_init_t{}, std::forward<Args>(args)...};
}

/// @brief Shorthand for `maybe_uninit<T>{paren_init_t{}, std::forward<T>(t)}`.
/// @note Single argument case is singled out to facilitate type deduction.
/// @relatedalso maybe_uninit
template <detail::sized T>
constexpr auto brace_init(T&& t) noexcept(detail::nothrow_brace_constructible_from<std::remove_reference_t<T>, T>)
    -> maybe_uninit<std::remove_reference_t<T>>
    requires detail::brace_constructible_from<std::remove_reference_t<T>, T>
{
    return maybe_uninit<std::remove_reference_t<T>>{brace_init_t{}, std::forward<T>(t)};
}

} // namespace MAYBE_UNINIT_NAMESPACE
