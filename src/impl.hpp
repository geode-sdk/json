#pragma once

#include <compare>
#include <cstdint>
#include <matjson.hpp>
#include <tsl/ordered_map.h>
#include <utility>
#include <variant>

using std::intmax_t;
using std::size_t;
using std::uintmax_t;

// stolen from geode sorry

namespace matjson {

    inline uint64_t fnv1aHash(std::string_view str) {
        uint64_t hash = 0xcbf29ce484222325;
        for (char c : str) {
            hash ^= c;
            hash *= 0x100000001b3;
        }
        return hash;
    }

    struct StringHash {
        using is_transparent = void;

        size_t operator()(std::string_view str) const {
            return fnv1aHash(str);
        }

        size_t operator()(std::string const& str) const {
            return fnv1aHash(str);
        }
    };

}

using OrderedMap = tsl::ordered_map<
    std::string, matjson::Value, matjson::StringHash, std::equal_to<>,
    std::allocator<std::pair<std::string, matjson::Value>>,
    std::vector<std::pair<std::string, matjson::Value>>>;

template <class T, class U>
std::partial_ordering compareIntInt(T a, U b) {
    if (std::cmp_equal(a, b)) return std::partial_ordering::equivalent;
    return std::cmp_less(a, b) ? std::partial_ordering::less : std::partial_ordering::greater;
}

template <class I>
std::partial_ordering compareIntDouble(I i, double d) {
    if (std::isnan(d)) return std::partial_ordering::unordered;
    if (double(i) == d) return std::partial_ordering::equivalent;
    return double(i) < d ? std::partial_ordering::less : std::partial_ordering::greater;
}

template <class T, class U>
std::partial_ordering compareNumbers(T a, U b) {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        return compareIntInt(a, b);
    }
    else if constexpr (std::is_integral_v<T>) {
        // T is int, U is float
        return compareIntDouble(a, b);
    }
    else if constexpr (std::is_integral_v<U>) {
        // T is float, U is int
        auto res = compareIntDouble(b, a);
        if (res == std::partial_ordering::less) return std::partial_ordering::greater;
        if (res == std::partial_ordering::greater) return std::partial_ordering::less;
        return res;
    }
    else {
        return a <=> b;
    }
}

template <class T>
concept Numerical =
    !std::is_same_v<T, bool> && (std::is_integral_v<T> || std::is_floating_point_v<T>);

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

class matjson::ValueImpl {
    Type m_type;
    std::optional<std::string> m_key;
    std::variant<std::monostate, std::string, double, intmax_t, uintmax_t, bool, std::vector<Value>, float, OrderedMap>
        m_value;

public:
    template <class T>
    ValueImpl(Type type, T&& value) : m_type(type), m_value(std::forward<T>(value)) {}

    template <class T>
    ValueImpl(Type type, std::string key, T&& value) :
        m_type(type), m_key(std::move(key)), m_value(std::forward<T>(value)) {}

    ValueImpl(Value::ObjCtorTag, std::span<Value> values) : m_type(Type::Object) {
        auto& map = m_value.emplace<OrderedMap>();
        for (auto& value : values) {
            // a null m_impl means a plain null without a key, which should never
            // happen here, but fall back to an empty key just incase
            std::string key;
            if (value.m_impl && value.m_impl->key()) key = *value.m_impl->key();
            map.insert_or_assign(std::move(key), std::move(value));
        }
    }

    ValueImpl(ValueImpl const&) = default;

    Type type() const {
        return m_type;
    }

    std::optional<std::string> const& key() const {
        return m_key;
    }

    std::optional<std::string>& key() {
        return m_key;
    }

    void setKey(std::string key) {
        m_key = std::move(key);
    }

    void clearKey() {
        m_key.reset();
    }

    static Value asValue(std::unique_ptr<ValueImpl> impl) {
        return Value(std::move(impl));
    }

    static std::unique_ptr<ValueImpl> makeKeyedNull(std::string key) {
        return std::make_unique<ValueImpl>(Type::Null, std::move(key), std::monostate{});
    }

    bool asBool() const {
        return std::get<bool>(m_value);
    }

    std::string const& asString() const {
        return std::get<std::string>(m_value);
    }

    std::vector<Value>& asArray() {
        return std::get<std::vector<Value>>(m_value);
    }

    std::vector<Value> const& asArray() const {
        return std::get<std::vector<Value>>(m_value);
    }

    OrderedMap& asMap() {
        return std::get<OrderedMap>(m_value);
    }

    OrderedMap const& asMap() const {
        return std::get<OrderedMap>(m_value);
    }

    static ValueImpl& fromValue(Value const& value) {
        return *value.m_impl;
    }

    auto const& getVariant() {
        return m_value;
    }

    bool isDouble() const {
        return std::holds_alternative<double>(m_value);
    }

    bool isFloat() const {
        return std::holds_alternative<float>(m_value);
    }

    bool isInt() const {
        return std::holds_alternative<intmax_t>(m_value);
    }

    bool isUInt() const {
        return std::holds_alternative<uintmax_t>(m_value);
    }

    template <class NumberType>
    NumberType asNumber() const {
        if (std::holds_alternative<intmax_t>(m_value))
            return static_cast<NumberType>(std::get<intmax_t>(m_value));
        else if (std::holds_alternative<uintmax_t>(m_value))
            return static_cast<NumberType>(std::get<uintmax_t>(m_value));
        else if (std::holds_alternative<float>(m_value))
            return static_cast<NumberType>(std::get<float>(m_value));
        else return static_cast<NumberType>(std::get<double>(m_value));
    }

    template <class Op>
    bool operatorImpl(ValueImpl const& other, Op&& operation) const {
        switch (m_type) {
            case Type::Bool: return operation(asBool(), other.asBool());
            case Type::String: return operation(asString(), other.asString());
            case Type::Array: return operation(asArray(), other.asArray());
            case Type::Object: return operation(asMap(), other.asMap());
            // handled outside of operatorImpl
            case Type::Null:
            case Type::Number:
            default: return false;
        }
    }

    std::partial_ordering compareNumbers(ValueImpl const& other) const {
        return std::visit(
            overloaded{
                [&](Numerical auto const& a, Numerical auto const& b) {
                    return ::compareNumbers(a, b);
                },
                [](auto const&, auto const&) {
                    // irrelevant
                    return std::partial_ordering::unordered;
                }
            },
            m_value,
            other.m_value
        );
    }

    bool operator==(ValueImpl const& other) const {
        if (m_type != other.m_type) return false;
        if (m_type == Type::Null) return true;
        if (m_type == Type::Number) {
            return this->compareNumbers(other) == 0;
        }
        return operatorImpl(other, std::equal_to<>{});
    }

    bool operator<(ValueImpl const& other) const {
        if (m_type < other.m_type) return true;
        if (m_type > other.m_type) return false;
        if (m_type == Type::Null) return false;
        if (m_type == Type::Number) {
            return this->compareNumbers(other) < 0;
        }
        return operatorImpl(other, std::less<>{});
    }
};

using ValuePtr = std::unique_ptr<matjson::ValueImpl>;

matjson::Value* getDummyNullValue();

// Every plain null has an empty m_impl, so the dummy has to be identified by address
#define CHECK_DUMMY_NULL (this == getDummyNullValue())
