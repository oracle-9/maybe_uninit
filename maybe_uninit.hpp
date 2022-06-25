#ifndef MAYBE_UNINIT_INCLUDED
#define MAYBE_UNINIT_INCLUDED

#include <memory>
#include <type_traits>
#include <utility>

#ifndef MAYBE_UNINIT_NAMESPACE_NAME
#define MAYBE_UNINIT_NAMESPACE_NAME mem
#endif

#if defined __GNUC__ or defined __clang__
#   define MAYBE_UNINIT_UNREACHABLE() __builtin_unreachable()
#elif defined _MSC_VER
#   define MAYBE_UNINIT_UNREACHABLE() __assume(false)
#else
#   define MAYBE_UNINIT_UNREACHABLE() for (;;)
#endif

namespace MAYBE_UNINIT_NAMESPACE_NAME {

struct default_init_tag_t{} inline constexpr default_init_tag{};
struct value_init_tag_t{} inline constexpr value_init_tag{};

template <typename T>
    requires std::is_object_v<T> // no void/refs/functions/unbound arrays
    and requires { sizeof(T); }  // complete type.
union maybe_uninit {
  private:
    T m_t;

  public:
    maybe_uninit(maybe_uninit const&)
        requires std::is_trivially_copy_constructible_v<T>
    = default;

    maybe_uninit(maybe_uninit&&)
        requires std::is_trivially_move_constructible_v<T>
    = default;

    maybe_uninit(maybe_uninit const&)
        requires (not std::is_trivially_copy_constructible_v<T>)
    = delete;

    maybe_uninit(maybe_uninit&&)
        requires (not std::is_trivially_move_constructible_v<T>)
    = delete;

    constexpr maybe_uninit() {}

    explicit maybe_uninit(default_init_tag_t)
        noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::is_default_constructible_v<T>
    {
        default_construct();
    }

    explicit constexpr maybe_uninit(value_init_tag_t)
        noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::is_default_constructible_v<T>
    {
        construct();
    }

    template <typename... Args>
    explicit constexpr maybe_uninit(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        requires std::is_constructible_v<T, Args...>
    {
        construct(std::forward<Args>(args)...);
    }

    maybe_uninit& operator=(maybe_uninit const&)
        requires std::is_trivially_copy_assignable_v<T>
    = default;

    maybe_uninit& operator=(maybe_uninit&&)
        requires std::is_trivially_move_assignable_v<T>
    = default;

    maybe_uninit& operator=(maybe_uninit const&)
        requires (not std::is_trivially_copy_assignable_v<T>)
    = delete;

    maybe_uninit& operator=(maybe_uninit&&)
        requires (not std::is_trivially_move_assignable_v<T>)
    = delete;

    void default_construct()
        noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::is_default_constructible_v<T>
    {
        new (std::addressof(m_t)) T;
    }

    template <typename... Args>
    constexpr void construct(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        requires std::is_constructible_v<T, Args...>
    {
        std::construct_at(std::addressof(m_t), std::forward<Args>(args)...);
    }

    [[nodiscard]]
    constexpr T* ptr() noexcept {
        return std::addressof(m_t);
    }

    [[nodiscard]]
    constexpr T const* ptr() const noexcept {
        return std::addressof(m_t);
    }

    [[nodiscard]]
    constexpr T& assume_init() & noexcept {
        return m_t;
    }

    [[nodiscard]]
    constexpr T const& assume_init() const& noexcept {
        return m_t;
    }

    [[nodiscard]]
    constexpr T&& assume_init() && noexcept {
        return std::move(m_t);
    }

    constexpr void destruct()
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T>
    {
        if constexpr (not std::is_trivially_destructible_v<T>) {
            std::destroy_at(std::addressof(m_t));
        }
    }

    constexpr ~maybe_uninit() {}

    ~maybe_uninit() requires std::is_trivially_destructible_v<T> = default;
};

template <typename Arg>
maybe_uninit(Arg&&) -> maybe_uninit<std::remove_cvref_t<Arg>>;

template <typename T>
constexpr maybe_uninit<T> uninit() noexcept {
    return maybe_uninit<T>();
}

template <typename T>
inline maybe_uninit<T> default_init()
    noexcept(std::is_nothrow_default_constructible_v<T>)
{
    static_assert(
        std::is_default_constructible_v<T>,
        "type must be default constructible"
    );
    if constexpr (std::is_default_constructible_v<T>) {
        return maybe_uninit<T>(default_init_tag);
    }
    MAYBE_UNINIT_UNREACHABLE();
}

template <typename T>
constexpr maybe_uninit<T> init()
    noexcept(std::is_nothrow_default_constructible_v<T>)
{
    static_assert(
        std::is_default_constructible_v<T>,
        "type must be default constructible"
    );
    if constexpr (std::is_default_constructible_v<T>) {
        return maybe_uninit<T>(value_init_tag);
    }
    MAYBE_UNINIT_UNREACHABLE();
}

template <typename T>
constexpr maybe_uninit<std::remove_cvref_t<T>> init(T&& t)
    noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>)
{
    using value_type = std::remove_cvref_t<T>;
    static_assert(
        not std::is_same_v<default_init_tag_t, value_type>
            and not std::is_same_v<value_init_tag_t, value_type>,
        "using a tag type as a parameter is not allowed"
    );
    static_assert(
        std::is_constructible_v<value_type, T>,
        "type must be constructible from itself"
    );
    if constexpr (std::is_constructible_v<value_type, T>) {
        return mem::maybe_uninit<value_type>(std::forward<T>(t));
    }
    MAYBE_UNINIT_UNREACHABLE();
}

template <typename T, typename Arg, typename... Args>
constexpr maybe_uninit<T> init(Arg&& arg, Args&&... args)
    noexcept(
        std::is_nothrow_constructible_v<std::remove_cvref_t<T>, Arg, Args...>
    )
{
    static_assert(
        not std::is_same_v<default_init_tag_t, std::remove_cvref_t<Arg>>
            and not std::is_same_v<value_init_tag_t, std::remove_cvref_t<Arg>>
            and not std::disjunction_v<
                std::is_same<default_init_tag_t, std::remove_cvref_t<Args>>...,
                std::is_same<value_init_tag_t, std::remove_cvref_t<Args>>...
            >,
        "using a tag type as a parameter is not allowed"
    );
    using value_type = std::remove_cvref_t<T>;
    static_assert(
        std::is_constructible_v<value_type, Arg, Args...>,
        "type must be constructible from the provided arguments "
    );
    if constexpr (std::is_constructible_v<value_type, Arg, Args...>) {
        return mem::maybe_uninit<value_type>(
            std::forward<Arg>(arg),
            std::forward<Args>(args)...
        );
    }
    MAYBE_UNINIT_UNREACHABLE();
}

} // namespace MAYBE_UNINIT_NAMESPACE_NAME

#undef MAYBE_UNINIT_UNREACHABLE

#endif // MAYBE_UNINIT_INCLUDED
