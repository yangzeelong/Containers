#pragma once

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

#include "containers/Config.hpp"

namespace containers {

template <typename T, typename E>
class Result;

namespace detail {

// Builds `Result<U, E>` from a mapped value, with a specialization for the
// `void` case so `map` can stay a single expression-shaped implementation.
template <typename U, typename E>
struct ResultBuilder {
    template <typename V>
    static Result<U, E> ok(V&& value) {
        return Result<U, E>::Ok(std::forward<V>(value));
    }

    template <typename V>
    static Result<U, E> err(V&& error) {
        return Result<U, E>::Err(std::forward<V>(error));
    }
};

template <typename E>
struct ResultBuilder<void, E> {
    static Result<void, E> ok() {
        return Result<void, E>::Ok();
    }

    template <typename V>
    static Result<void, E> err(V&& error) {
        return Result<void, E>::Err(std::forward<V>(error));
    }
};

// Applies a `map` callable whose result type is known (`U`). The void
// specialization drops the result instead of forwarding it to Ok(), which has
// no argument in Result<void, E>.
template <typename U>
struct ResultMapOk {
    template <typename E, typename F, typename V>
    static Result<U, E> call(F&& f, V&& value) {
        return Result<U, E>::Ok(std::forward<F>(f)(std::forward<V>(value)));
    }
};

template <>
struct ResultMapOk<void> {
    template <typename E, typename F, typename V>
    static Result<void, E> call(F&& f, V&& value) {
        std::forward<F>(f)(std::forward<V>(value));
        return Result<void, E>::Ok();
    }
};

// Same as ResultMapOk, for the no-argument callables used by Result<void, E>.
template <typename U>
struct ResultMapOkNoArg {
    template <typename E, typename F>
    static Result<U, E> call(F&& f) {
        return Result<U, E>::Ok(std::forward<F>(f)());
    }
};

template <>
struct ResultMapOkNoArg<void> {
    template <typename E, typename F>
    static Result<void, E> call(F&& f) {
        std::forward<F>(f)();
        return Result<void, E>::Ok();
    }
};

} // namespace detail

// Rust's `Result<T, E>`: either an ok value or an error, with combinators
// (`map`, `mapErr`, `andThen`) modelled on the Rust API. `Result<void, E>`
// stands in for `Result<(), E>`.
//
// The class never throws on its own, but copying/assigning may throw when `T`
// or `E` does; assignment offers the basic guarantee only (the object is
// destroyed before the new state is constructed), which is documented in the
// header rather than hidden behind exception machinery.
template <typename T, typename E>
class Result {
    static_assert(!std::is_void<T>::value, "use Result<void, E> for empty payloads");
    static_assert(!std::is_reference<T>::value, "Result does not hold references");
    static_assert(!std::is_reference<E>::value, "Result does not hold references");

    union Storage {
        T value;
        E error;
        Storage() noexcept {}
        ~Storage() noexcept {}
    };

    struct OkTag {};
    struct ErrTag {};

public:
    using value_type = T;
    using error_type = E;

    Result(const Result& other) : m_ok(other.m_ok) {
        if (m_ok) {
            new (&m_storage.value) T(other.m_storage.value);
        } else {
            new (&m_storage.error) E(other.m_storage.error);
        }
    }

    Result(Result&& other) : m_ok(other.m_ok) {
        if (m_ok) {
            new (&m_storage.value) T(std::move(other.m_storage.value));
        } else {
            new (&m_storage.error) E(std::move(other.m_storage.error));
        }
    }

    Result& operator=(const Result& other) {
        if (this != &other) {
            destroy();
            m_ok = other.m_ok;
            if (m_ok) {
                new (&m_storage.value) T(other.m_storage.value);
            } else {
                new (&m_storage.error) E(other.m_storage.error);
            }
        }
        return *this;
    }

    Result& operator=(Result&& other) {
        if (this != &other) {
            destroy();
            m_ok = other.m_ok;
            if (m_ok) {
                new (&m_storage.value) T(std::move(other.m_storage.value));
            } else {
                new (&m_storage.error) E(std::move(other.m_storage.error));
            }
        }
        return *this;
    }

    ~Result() { destroy(); }

    static Result Ok(const T& value) { return Result(OkTag{}, value); }
    static Result Ok(T&& value) { return Result(OkTag{}, std::move(value)); }
    static Result Err(const E& error) { return Result(ErrTag{}, error); }
    static Result Err(E&& error) { return Result(ErrTag{}, std::move(error)); }

    CS_NODISCARD bool isOk() const noexcept { return m_ok; }
    CS_NODISCARD bool isErr() const noexcept { return !m_ok; }

    T& value() {
        assert(m_ok);
        return m_storage.value;
    }

    const T& value() const {
        assert(m_ok);
        return m_storage.value;
    }

    E& error() {
        assert(!m_ok);
        return m_storage.error;
    }

    const E& error() const {
        assert(!m_ok);
        return m_storage.error;
    }

    T unwrap() const {
        assert(m_ok);
        return m_storage.value;
    }

    E unwrapErr() const {
        assert(!m_ok);
        return m_storage.error;
    }

    T unwrapOr(T fallback) const {
        return m_ok ? m_storage.value : std::move(fallback);
    }

    template <typename F>
    CS_NODISCARD auto map(F&& f) const
        -> Result<typename std::decay<decltype(std::declval<F&>()(
                      std::declval<const T&>()))>::type,
                  E> {
        using U = typename std::decay<decltype(std::declval<F&>()(
            std::declval<const T&>()))>::type;
        if (m_ok) {
            return detail::ResultMapOk<U>::template call<E>(std::forward<F>(f),
                                                             m_storage.value);
        }
        return detail::ResultBuilder<U, E>::err(m_storage.error);
    }

    template <typename F>
    CS_NODISCARD auto mapErr(F&& f) const
        -> Result<T, typename std::decay<decltype(std::declval<F&>()(
                       std::declval<const E&>()))>::type> {
        using F2 = typename std::decay<decltype(std::declval<F&>()(
            std::declval<const E&>()))>::type;
        if (m_ok) {
            return Result<T, F2>::Ok(m_storage.value);
        }
        return Result<T, F2>::Err(std::forward<F>(f)(m_storage.error));
    }

    template <typename F>
    CS_NODISCARD auto andThen(F&& f) const
        -> typename std::decay<decltype(std::declval<F&>()(
            std::declval<const T&>()))>::type {
        using R = typename std::decay<decltype(std::declval<F&>()(
            std::declval<const T&>()))>::type;
        if (m_ok) {
            return std::forward<F>(f)(m_storage.value);
        }
        return R::Err(m_storage.error);
    }

private:
    explicit Result(OkTag, const T& value) : m_ok(true) {
        new (&m_storage.value) T(value);
    }

    explicit Result(OkTag, T&& value) : m_ok(true) {
        new (&m_storage.value) T(std::move(value));
    }

    explicit Result(ErrTag, const E& error) : m_ok(false) {
        new (&m_storage.error) E(error);
    }

    explicit Result(ErrTag, E&& error) : m_ok(false) {
        new (&m_storage.error) E(std::move(error));
    }

    void destroy() noexcept {
        if (m_ok) {
            m_storage.value.~T();
        } else {
            m_storage.error.~E();
        }
    }

    Storage m_storage;
    bool m_ok;
};

// `Result<void, E>`: no payload, only success/failure. Kept as an explicit
// specialization rather than `Result<Unit, E>` so callers can write
// `Result<void, E>::Ok()` without dragging a value type through every
// combinator.
template <typename E>
class Result<void, E> {
    static_assert(!std::is_reference<E>::value, "Result does not hold references");

    union Storage {
        E error;
        Storage() noexcept {}
        ~Storage() noexcept {}
    };

    struct OkTag {};
    struct ErrTag {};

public:
    using value_type = void;
    using error_type = E;

    Result(const Result& other) : m_ok(other.m_ok) {
        if (!m_ok) {
            new (&m_storage.error) E(other.m_storage.error);
        }
    }

    Result(Result&& other) : m_ok(other.m_ok) {
        if (!m_ok) {
            new (&m_storage.error) E(std::move(other.m_storage.error));
        }
    }

    Result& operator=(const Result& other) {
        if (this != &other) {
            destroy();
            m_ok = other.m_ok;
            if (!m_ok) {
                new (&m_storage.error) E(other.m_storage.error);
            }
        }
        return *this;
    }

    Result& operator=(Result&& other) {
        if (this != &other) {
            destroy();
            m_ok = other.m_ok;
            if (!m_ok) {
                new (&m_storage.error) E(std::move(other.m_storage.error));
            }
        }
        return *this;
    }

    ~Result() { destroy(); }

    static Result Ok() noexcept { return Result(OkTag{}); }
    static Result Err(const E& error) { return Result(ErrTag{}, error); }
    static Result Err(E&& error) { return Result(ErrTag{}, std::move(error)); }

    CS_NODISCARD bool isOk() const noexcept { return m_ok; }
    CS_NODISCARD bool isErr() const noexcept { return !m_ok; }

    E& error() {
        assert(!m_ok);
        return m_storage.error;
    }

    const E& error() const {
        assert(!m_ok);
        return m_storage.error;
    }

    E unwrapErr() const {
        assert(!m_ok);
        return m_storage.error;
    }

    void unwrap() const { assert(m_ok); }

    template <typename F>
    CS_NODISCARD auto map(F&& f) const
        -> Result<typename std::decay<decltype(std::declval<F&>()())>::type, E> {
        using U = typename std::decay<decltype(std::declval<F&>()())>::type;
        if (m_ok) {
            return detail::ResultMapOkNoArg<U>::template call<E>(std::forward<F>(f));
        }
        return detail::ResultBuilder<U, E>::err(m_storage.error);
    }

    template <typename F>
    CS_NODISCARD auto andThen(F&& f) const
        -> typename std::decay<decltype(std::declval<F&>()())>::type {
        using R = typename std::decay<decltype(std::declval<F&>()())>::type;
        if (m_ok) {
            return std::forward<F>(f)();
        }
        return R::Err(m_storage.error);
    }

private:
    explicit Result(OkTag) noexcept : m_ok(true) {}
    explicit Result(ErrTag, const E& error) : m_ok(false) {
        new (&m_storage.error) E(error);
    }
    explicit Result(ErrTag, E&& error) : m_ok(false) {
        new (&m_storage.error) E(std::move(error));
    }

    void destroy() noexcept {
        if (!m_ok) {
            m_storage.error.~E();
        }
    }

    Storage m_storage;
    bool m_ok;
};

} // namespace containers
