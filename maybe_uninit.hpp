#ifndef MAYBE_UNINIT_INCLUDED
#define MAYBE_UNINIT_INCLUDED

#include <memory>
#include <type_traits>
#include <utility>

#ifndef MAYBE_UNINIT_NAMESPACE_NAME
#define MAYBE_UNINIT_NAMESPACE_NAME mem
#endif

#define MAYBE_UNINIT_COMMA ,

#if defined __GNUC__ or defined __clang__
#   define MAYBE_UNINIT_UNREACHABLE() __builtin_unreachable()
#elif defined _MSC_VER
#   define MAYBE_UNINIT_UNREACHABLE() __assume(false)
#else
#   define MAYBE_UNINIT_UNREACHABLE() do {} while(true)
#endif

#define MAYBE_UNINIT_STATIC_IF(cond, action, ...)                              \
    do {                                                                       \
        static_assert(cond __VA_OPT__(, __VA_ARGS__));                         \
        if constexpr (cond) {                                                  \
            action;                                                            \
        } else {                                                               \
            MAYBE_UNINIT_UNREACHABLE();                                        \
        }                                                                      \
    } while (false)

#define MAYBE_UNINIT_IS_TAG(type)                                              \
    (                                                                          \
        std::is_same_v<default_init_tag_t, type>                               \
        or std::is_same_v<value_init_tag_t, type>                              \
    )

namespace MAYBE_UNINIT_NAMESPACE_NAME {

struct default_init_tag_t{} inline constexpr default_init_tag{};
struct value_init_tag_t{} inline constexpr value_init_tag{};

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

    maybe_uninit(maybe_uninit const&)
        requires (not std::is_trivially_copy_constructible_v<T>)
    = delete;

    maybe_uninit(maybe_uninit&&)
        requires (not std::is_trivially_move_constructible_v<T>)
    = delete;

    constexpr maybe_uninit() noexcept {}

    explicit maybe_uninit(default_init_tag_t)
        noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        MAYBE_UNINIT_STATIC_IF(
            std::is_default_constructible_v<T>,
            default_init(),
            "type must be default constructible"
        );
    }

    explicit constexpr maybe_uninit(value_init_tag_t)
        noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        MAYBE_UNINIT_STATIC_IF(
            std::is_default_constructible_v<T>,
            init(),
            "type must be default constructible"
        );
    }

    template <typename... Args>
    explicit constexpr maybe_uninit(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        MAYBE_UNINIT_STATIC_IF(
            std::is_constructible_v<T MAYBE_UNINIT_COMMA Args...>,
            init(std::forward<Args>(args)...),
            "type must be constructible from the provided arguments"
        );
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

    void default_init()
        noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        MAYBE_UNINIT_STATIC_IF(
            std::is_default_constructible_v<T>,
            new (std::addressof(m_t)) T,
            "type must be default constructible"
        );
    }

    template <typename... Args>
    constexpr void init(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        MAYBE_UNINIT_STATIC_IF(
            std::is_constructible_v<T MAYBE_UNINIT_COMMA Args...>,
            std::construct_at(
                std::addressof(m_t) MAYBE_UNINIT_COMMA
                std::forward<Args>(args)...
            ),
            "type must be constructible from the provided arguments"
        );
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

    ~maybe_uninit() requires std::is_trivially_destructible_v<T> = default;

    constexpr ~maybe_uninit() {
        if constexpr (SELF_DESTRUCT) {
            destruct();
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
    MAYBE_UNINIT_STATIC_IF(
        std::is_default_constructible_v<T>,
        return maybe_uninit<
            T MAYBE_UNINIT_COMMA
            SELF_DESTRUCT
        >(default_init_tag),
        "type must be default constructible"
    );
}

template <typename T, bool SELF_DESTRUCT = false>
constexpr maybe_uninit<T, SELF_DESTRUCT> init()
    noexcept(std::is_nothrow_default_constructible_v<T>)
{
    MAYBE_UNINIT_STATIC_IF(
        std::is_default_constructible_v<T>,
        return maybe_uninit<T MAYBE_UNINIT_COMMA SELF_DESTRUCT>(value_init_tag),
        "type must be default constructible"
    );
}

template <typename T, bool SELF_DESTRUCT = false>
constexpr maybe_uninit<std::remove_cvref_t<T>, SELF_DESTRUCT> init(T&& t)
    noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>)
{
    using value_type = std::remove_cvref_t<T>;
    static_assert(
        not MAYBE_UNINIT_IS_TAG(value_type),
        "using a tag type as a parameter is not allowed"
    );
    MAYBE_UNINIT_STATIC_IF(
        std::is_constructible_v<value_type MAYBE_UNINIT_COMMA T>,
        return maybe_uninit<
            value_type MAYBE_UNINIT_COMMA
            SELF_DESTRUCT
        >(std::forward<T>(t)),
        "type must be constructible from itself"
    );
}

template <typename T>
constexpr maybe_uninit<std::remove_cvref_t<T>, true> init_auto(T&& t)
    noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>)
{
    using value_type = std::remove_cvref_t<T>;
    static_assert(
        not MAYBE_UNINIT_IS_TAG(value_type),
        "using a tag type as a parameter is not allowed"
    );
    MAYBE_UNINIT_STATIC_IF(
        std::is_constructible_v<value_type MAYBE_UNINIT_COMMA T>,
        return maybe_uninit<
            value_type MAYBE_UNINIT_COMMA
            true
        >(std::forward<T>(t)),
        "type must be constructible from itself"
    );
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
    using arg_value_type = std::remove_cvref_t<Arg>;
    static_assert(
        not MAYBE_UNINIT_IS_TAG(arg_value_type)
            and not std::disjunction_v<
                std::is_same<default_init_tag_t, std::remove_cvref_t<Args>>...,
                std::is_same<value_init_tag_t, std::remove_cvref_t<Args>>...
            >,
        "using a tag type as a parameter is not allowed"
    );
    using value_type = std::remove_cvref_t<T>;
    MAYBE_UNINIT_STATIC_IF(
        std::is_constructible_v<
            value_type MAYBE_UNINIT_COMMA
            Arg MAYBE_UNINIT_COMMA
            Args...
        >,
        return maybe_uninit<value_type MAYBE_UNINIT_COMMA SELF_DESTRUCT>(
            std::forward<Arg>(arg) MAYBE_UNINIT_COMMA
            std::forward<Args>(args)...
        ),
        "type must be constructible from the provided arguments"
    );
}

} // namespace MAYBE_UNINIT_NAMESPACE_NAME

#undef MAYBE_UNINIT_IS_TAG
#undef MAYBE_UNINIT_STATIC_IF
#undef MAYBE_UNINIT_UNREACHABLE
#undef MAYBE_UNINIT_COMMA

#endif // MAYBE_UNINIT_INCLUDED
