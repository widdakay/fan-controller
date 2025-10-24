#pragma once
#include <utility>
#include <type_traits>

namespace util {

// Simple Result<T, E> type for error handling without exceptions
template<typename T, typename E>
class Result {
public:
    // Constructors
    static Result Ok(T value) {
        Result r;
        r.isOk_ = true;
        new (&r.value_) T(std::move(value));
        return r;
    }

    static Result Err(E error) {
        Result r;
        r.isOk_ = false;
        new (&r.error_) E(std::move(error));
        return r;
    }

    // Copy constructor
    Result(const Result& other) : isOk_(other.isOk_) {
        if (isOk_) {
            new (&value_) T(other.value_);
        } else {
            new (&error_) E(other.error_);
        }
    }

    // Move constructor
    Result(Result&& other) noexcept : isOk_(other.isOk_) {
        if (isOk_) {
            new (&value_) T(std::move(other.value_));
        } else {
            new (&error_) E(std::move(other.error_));
        }
    }

    // Destructor
    ~Result() {
        if (isOk_) {
            value_.~T();
        } else {
            error_.~E();
        }
    }

    // Query methods
    bool isOk() const { return isOk_; }
    bool isErr() const { return !isOk_; }
    explicit operator bool() const { return isOk_; }

    // Access methods
    T& value() { return value_; }
    const T& value() const { return value_; }
    E& error() { return error_; }
    const E& error() const { return error_; }

    // Safe access with default
    T valueOr(T defaultVal) const {
        return isOk_ ? value_ : defaultVal;
    }

    // Map operation: transform success value
    template<typename F>
    auto map(F&& f) const -> Result<decltype(f(std::declval<const T&>())), E> {
        using U = decltype(f(std::declval<const T&>()));
        if (isOk_) {
            return Result<U, E>::Ok(f(value_));
        } else {
            return Result<U, E>::Err(error_);
        }
    }

private:
    Result() : isOk_(false) {}

    bool isOk_;
    union {
        T value_;
        E error_;
    };
};

// Specialization for void success type
template<typename E>
class Result<void, E> {
public:
    static Result Ok() {
        Result r;
        r.isOk_ = true;
        return r;
    }

    static Result Err(E error) {
        Result r;
        r.isOk_ = false;
        new (&r.error_) E(std::move(error));
        return r;
    }

    ~Result() {
        if (!isOk_) {
            error_.~E();
        }
    }

    bool isOk() const { return isOk_; }
    bool isErr() const { return !isOk_; }
    explicit operator bool() const { return isOk_; }

    E& error() { return error_; }
    const E& error() const { return error_; }

private:
    Result() : isOk_(false) {}

    bool isOk_;
    union {
        char dummy_;
        E error_;
    };
};

} // namespace util
