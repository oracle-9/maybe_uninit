#ifndef MAYBE_UNINIT_INCLUDED
#define MAYBE_UNINIT_INCLUDED

#include <memory>
#include <type_traits>
#include <utility>

#ifndef MAYBE_UNINIT_NAMESPACE_NAME
#define MAYBE_UNINIT_NAMESPACE_NAME mem
#endif

namespace MAYBE_UNINIT_NAMESPACE_NAME {

struct default_construct_tag_t{} inline constexpr default_construct_tag{};
struct value_construct_tag_t{} inline constexpr value_construct_tag{};

template <typename T>
    requires std::is_object_v<T> and requires { sizeof(T); }
union maybe_uninit {
  private:
    struct unit_t{} m_unit;
    T m_t;

    friend maybe_uninit<T> init<>();

  public:
    constexpr maybe_uninit() : m_unit{} {}

    explicit maybe_uninit(default_construct_tag_t)
        noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::is_default_constructible_v<T>
        : m_unit{}
    {
        default_construct();
    }

    explicit constexpr maybe_uninit(value_construct_tag_t)
        noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::is_default_constructible_v<T>
        : m_unit{}
    {
        emplace_construct();
    }

    template <typename... Args>
    explicit constexpr maybe_uninit(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        requires std::is_constructible_v<T, Args...>
    {
        emplace_construct(std::forward<Args>(args)...);
    }

    void default_construct()
        noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::is_default_constructible_v<T>
    {
        new (std::addressof(m_t)) T;
    }

    template <typename... Args>
    constexpr void emplace_construct(Args&&... args)
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
    constexpr T&& assume_init() && noexcept {
        return std::move(m_t);
    }

    [[nodiscard]]
    constexpr T& assume_init() & noexcept {
        return m_t;
    }

    [[nodiscard]]
    constexpr T const& assume_init() const& noexcept {
        return m_t;
    }

    constexpr void destroy()
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T>
    {
        if constexpr (not std::is_trivially_destructible_v<T>) {
            std::destroy_at(std::addressof(m_t));
        }
    }

    constexpr ~maybe_uninit() {};

    ~maybe_uninit() requires std::is_trivially_destructible_v<T> = default;
};

template <typename Arg>
maybe_uninit(Arg&&) -> maybe_uninit<Arg>;

template <typename T>
constexpr maybe_uninit<T> uninit() noexcept {
    return maybe_uninit<T>();
}

template <typename T>
maybe_uninit<T> default_init()
    noexcept(noexcept(maybe_uninit<T>(default_construct_tag)))
{
    return maybe_uninit<T>(default_construct_tag);
}

template <typename T>
constexpr maybe_uninit<T> init()
    noexcept(noexcept(maybe_uninit<T>(value_construct_tag)))
{
    return maybe_uninit<T>(value_construct_tag);
}

template <typename T>
constexpr maybe_uninit<T> init(T&& t)
    noexcept(noexcept(maybe_uninit(std::forward<T>(t))))
{
    return maybe_uninit(std::forward<T>(t));
}

template <typename T, typename Arg, typename... Args>
constexpr maybe_uninit<T> init(Arg&& arg, Args&&... args)
    noexcept(noexcept(
        maybe_uninit<T>(std::forward<Arg>(arg), std::forward<Args>(args)...)
    ))
{
    return maybe_uninit<T>(std::forward<Arg>(arg), std::forward<Args>(args)...);
}

} // namespace MAYBE_UNINIT_NAMESPACE_NAME

#endif // MAYBE_UNINIT_INCLUDED
