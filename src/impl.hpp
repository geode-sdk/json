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
    ValueImpl(Type type, T&& value) : m_type(type), m_value(std::forward<T>(value)) {}

    template <class T>
    ValueImpl(Type type, std::string key, T&& value) :
        m_type(type), m_key(std::move(key)), m_value(std::forward<T>(value)) {}

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

    std::vector<Value> const& asArray() const {
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

    template <class Op>
    bool operatorImpl(ValueImpl const& other, Op&& operation, bool null, bool fall) const {
        switch (m_type) {
            case Type::Null: return null;
            case Type::Bool: return operation(asBool(), other.asBool());
            case Type::String: return operation(asString(), other.asString());
            case Type::Object:
            case Type::Array: return operation(asArray(), other.asArray());
            case Type::Number: {
                static constexpr auto INDEX_DOUBLE = 2;
                static constexpr auto INDEX_INT = 3;
                static constexpr auto INDEX_UINT = 4;
                static_assert(std::is_same_v<
                              std::variant_alternative_t<INDEX_DOUBLE, std::remove_cvref_t<decltype(m_value)>>,
                              double>);
                static_assert(std::is_same_v<
                              std::variant_alternative_t<INDEX_INT, std::remove_cvref_t<decltype(m_value)>>,
                              std::intmax_t>);
                static_assert(std::is_same_v<
                              std::variant_alternative_t<INDEX_UINT, std::remove_cvref_t<decltype(m_value)>>,
                              std::uintmax_t>);
                auto const asDouble = [](auto const& value) {
                    return std::get<double>(value.m_value);
                };
                auto const asInt = [](auto const& value) {
                    return std::get<std::intmax_t>(value.m_value);
                };
                auto const asUInt = [](auto const& value) {
                    return std::get<std::uintmax_t>(value.m_value);
                };
                switch (m_value.index()) {
                    case INDEX_DOUBLE:
                        switch (other.m_value.index()) {
                            case INDEX_DOUBLE: return operation(asDouble(*this), asDouble(other));
                            case INDEX_INT: return operation(asDouble(*this), asInt(other));
                            case INDEX_UINT: return operation(asDouble(*this), asUInt(other));
                            default: return fall;
                        }
                    case INDEX_INT:
                        switch (other.m_value.index()) {
                            case INDEX_DOUBLE: return operation(asInt(*this), asDouble(other));
                            case INDEX_INT: return operation(asInt(*this), asInt(other));
                            case INDEX_UINT: return operation(asInt(*this), asUInt(other));
                            default: return fall;
                        }
                    case INDEX_UINT:
                        switch (other.m_value.index()) {
                            case INDEX_DOUBLE: return operation(asUInt(*this), asDouble(other));
                            case INDEX_INT: return operation(asUInt(*this), asInt(other));
                            case INDEX_UINT: return operation(asUInt(*this), asUInt(other));
                            default: return fall;
                        }
                    default: return fall;
                }
            }
            default: return fall;
        }
    }

    bool operator==(ValueImpl const& other) const {
        if (m_type != other.m_type) return false;
        return operatorImpl(other, std::equal_to<>{}, true, false);
    }

    bool operator<(ValueImpl const& other) const {
        if (m_type < other.m_type) return true;
        if (m_type > other.m_type) return false;
        return operatorImpl(other, std::less<>{}, true, false);
    }

    bool operator>(ValueImpl const& other) const {
        if (m_type > other.m_type) return true;
        if (m_type < other.m_type) return false;
        return operatorImpl(other, std::greater<>{}, false, false);
    }
};

using ValuePtr = std::unique_ptr<matjson::ValueImpl>;

matjson::Value* getDummyNullValue();

#define CHECK_DUMMY_NULL (this->m_impl == getDummyNullValue()->m_impl)
