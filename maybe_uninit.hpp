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
#   define MAYBE_UNINIT_UNREACHABLE() do {} while(true)
#endif

namespace MAYBE_UNINIT_NAMESPACE_NAME {

struct default_init_tag_t{} inline constexpr default_init_tag{};
struct value_init_tag_t{} inline constexpr value_init_tag{};

namespace detail {

template <typename T>
concept default_constructible = requires {
    new (std::declval<void*>()) T;
};

template <typename T, typename... Args>
concept constructible_from = requires (Args&&... args) {
    new (std::declval<void*>()) T(std::forward<Args>(args)...);
};

template <typename... Ts>
concept not_tag =
    not std::disjunction_v<
        std::is_same<default_init_tag_t, std::remove_cvref_t<Ts>>...,
        std::is_same<value_init_tag_t, std::remove_cvref_t<Ts>>...
>;

} // namespace detail

template <typename T, bool SELF_DESTRUCT = false>
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

    maybe_uninit& operator=(maybe_uninit const&)
        requires std::is_trivially_copy_assignable_v<T>
    = default;

    maybe_uninit& operator=(maybe_uninit&&)
        requires std::is_trivially_move_assignable_v<T>
    = default;

    constexpr maybe_uninit() noexcept {}

    explicit maybe_uninit(default_init_tag_t)
        noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        static_assert(
            detail::default_constructible<T>,
            "type must be default constructible"
        );
        if constexpr (detail::default_constructible<T>) {
            default_init();
        }
    }

    explicit constexpr maybe_uninit(value_init_tag_t)
        noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        static_assert(
            detail::default_constructible<T>,
            "type must be default constructible"
        );
        if constexpr (detail::default_constructible<T>) {
            init();
        }
    }

    template <typename Arg, typename... Args>
    explicit constexpr maybe_uninit(Arg&& arg, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Arg, Args...>)
        requires (not std::is_same_v<std::remove_cvref_t<Arg>, maybe_uninit>)
    {
        static_assert(
            detail::constructible_from<T, Arg, Args...>,
            "type must be constructible from the provided arguments"
        );
        if constexpr (detail::constructible_from<T, Arg, Args...>) {
            init(std::forward<Arg>(arg), std::forward<Args>(args)...);
        }
    }

    void default_init()
        noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        static_assert(
            detail::default_constructible<T>,
            "type must be default constructible"
        );
        if constexpr (detail::default_constructible<T>) {
            new (std::addressof(m_t)) T;
        }
    }

    template <typename... Args>
    constexpr void init(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        static_assert(
            detail::constructible_from<T, Args...>,
            "type must be constructible from the provided arguments"
        );
        if constexpr (detail::constructible_from<T, Args...>) {
            std::construct_at(std::addressof(m_t), std::forward<Args>(args)...);
        }
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
    {
        static_assert(std::is_destructible_v<T>, "type must be destructible");
        if constexpr (std::is_destructible_v<T>) {
            if constexpr (std::is_trivially_destructible_v<T>) {
                std::destroy_at(std::addressof(m_t));
            }
        }
    }

    ~maybe_uninit() requires std::is_trivially_destructible_v<T> = default;

    constexpr ~maybe_uninit() {
        if constexpr (SELF_DESTRUCT) {
            static_assert(
                std::is_destructible_v<T>,
                "type must be destructible"
            );
            if constexpr (std::is_destructible_v<T>) {
                destruct();
            }
        }
    }
};

template <typename Arg>
maybe_uninit(Arg&&) -> maybe_uninit<std::remove_cvref_t<Arg>>;

template <typename T, bool SELF_DESTRUCT = false>
constexpr maybe_uninit<T, SELF_DESTRUCT> uninit() noexcept {
    return maybe_uninit<T, SELF_DESTRUCT>();
}

template <typename T, bool SELF_DESTRUCT = false>
inline maybe_uninit<T, SELF_DESTRUCT> default_init()
    noexcept(std::is_nothrow_default_constructible_v<T>)
{
    static_assert(
        detail::default_constructible<T>,
        "type must be default constructible"
    );
    if constexpr (detail::default_constructible<T>) {
        return maybe_uninit<T, SELF_DESTRUCT>(default_init_tag);
    }
    MAYBE_UNINIT_UNREACHABLE();
}

template <typename T, bool SELF_DESTRUCT = false>
constexpr maybe_uninit<T, SELF_DESTRUCT> init()
    noexcept(std::is_nothrow_default_constructible_v<T>)
{
    static_assert(
        detail::default_constructible<T>,
        "type must be default constructible"
    );
    if constexpr (detail::default_constructible<T>) {
        return maybe_uninit<T, SELF_DESTRUCT>(value_init_tag);
    }
    MAYBE_UNINIT_UNREACHABLE();
}

template <typename T, bool SELF_DESTRUCT = false>
constexpr maybe_uninit<std::remove_cvref_t<T>, SELF_DESTRUCT> init(T&& t)
    noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>)
{
    using value_type = std::remove_cvref_t<T>;
    static_assert(
        detail::not_tag<value_type>,
        "using a tag type as a parameter is not allowed"
    );
    static_assert(
        detail::constructible_from<value_type, T>,
        "type must be constructible from itself"
    );
    if constexpr (detail::constructible_from<value_type, T>) {
        return maybe_uninit<value_type, SELF_DESTRUCT>(std::forward<T>(t));
    }
    MAYBE_UNINIT_UNREACHABLE();
}

template <typename T>
constexpr maybe_uninit<std::remove_cvref_t<T>, true> init_auto(T&& t)
    noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>)
{
    using value_type = std::remove_cvref_t<T>;
    static_assert(
        detail::not_tag<value_type>,
        "using a tag type as a parameter is not allowed"
    );
    static_assert(
        detail::constructible_from<value_type, T>,
        "type must be constructible from itself"
    );
    if constexpr (detail::constructible_from<value_type, T>) {
        return maybe_uninit<value_type, true>(std::forward<T>(t));
    }
    MAYBE_UNINIT_UNREACHABLE();
}

template <
    typename T,
    bool SELF_DESTRUCT = false,
    typename Arg,
    typename... Args
>
constexpr maybe_uninit<T, SELF_DESTRUCT> init(Arg&& arg, Args&&... args)
    noexcept(
        std::is_nothrow_constructible_v<std::remove_cvref_t<T>, Arg, Args...>
    )
{
    static_assert(
        detail::not_tag<Arg, Args...>,
        "using a tag type as a parameter is not allowed"
    );
    using value_type = std::remove_cvref_t<T>;
    static_assert(
        detail::constructible_from<value_type, Arg, Args...>,
        "type must be constructible from the provided arguments"
    );
    if constexpr (detail::constructible_from<value_type, Arg, Args...>) {
        return maybe_uninit<value_type, SELF_DESTRUCT>(
            std::forward<Arg>(arg),
            std::forward<Args>(args)...
        );
    }
    MAYBE_UNINIT_UNREACHABLE();
}

} // namespace MAYBE_UNINIT_NAMESPACE_NAME

#undef MAYBE_UNINIT_UNREACHABLE
#undef MAYBE_UNINIT_NAMESPACE_NAME

#endif // MAYBE_UNINIT_INCLUDED
