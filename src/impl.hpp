#pragma once

#include <cstdint>
#include <matjson.hpp>
#include <variant>

using std::intmax_t;
using std::size_t;
using std::uintmax_t;

class matjson::ValueImpl {
    Type m_type;
    std::optional<std::string> m_key;
    std::variant<std::monostate, std::string, double, intmax_t, uintmax_t, bool, std::vector<Value>> m_value;

public:
    template <class T>
    ValueImpl(Type type, T value) : m_type(type), m_value(value) {}

    template <class T>
    ValueImpl(Type type, std::string key, T value) :
        m_type(type), m_key(std::move(key)), m_value(value) {}

    ValueImpl(ValueImpl const&) = default;

    Type type() const {
        return m_type;
    }

    std::optional<std::string> key() const {
        return m_key;
    }

    void setKey(std::string key) {
        m_key = std::move(key);
    }

    static Value asValue(std::unique_ptr<ValueImpl> impl) {
        return Value(std::move(impl));
    }

    bool asBool() const {
        return std::get<bool>(m_value);
    }

    std::string asString() const {
        return std::get<std::string>(m_value);
    }

    std::vector<Value>& asArray() {
        return std::get<std::vector<Value>>(m_value);
    }

    static ValueImpl& fromValue(Value const& value) {
        return *value.m_impl.get();
    }

    auto const& getVariant() {
        return m_value;
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
        else return static_cast<NumberType>(std::get<double>(m_value));
    }
};

using ValuePtr = std::unique_ptr<matjson::ValueImpl>;

matjson::Value* getDummyNullValue();

#define CHECK_DUMMY_NULL (this->m_impl == getDummyNullValue()->m_impl)
