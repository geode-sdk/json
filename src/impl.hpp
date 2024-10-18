#pragma once

#include <cstdint>
#include <matjson3.hpp>
#include <variant>

class matjson::ValueImpl {
    Type m_type;
    std::optional<std::string> m_key;
    std::variant<std::monostate, std::string, double, std::int64_t, bool, Array> m_value;

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

    double asDouble() const {
        return std::get<double>(m_value);
    }

    std::int64_t asInt() const {
        return std::get<std::int64_t>(m_value);
    }

    bool isInteger() const {
        return std::holds_alternative<std::int64_t>(m_value);
    }

    Array& asArray() {
        return std::get<Array>(m_value);
    }

    static ValueImpl& fromValue(Value const& value) {
        return *value.m_impl.get();
    }

    auto const& getVariant() {
        return m_value;
    }
};

using ValuePtr = std::unique_ptr<matjson::ValueImpl>;

matjson::Value* getDummyNullValue();

#define CHECK_DUMMY_NULL (this->m_impl == getDummyNullValue()->m_impl)
