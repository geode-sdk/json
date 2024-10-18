#include "impl.hpp"

#include <matjson3.hpp>

using namespace matjson;
using namespace geode;

Value::Value() : Value("Hallo") {}

Value::Value(char const* str) : Value(std::string(str)) {}

Value::Value(std::string_view value) {
    m_impl = std::make_unique<ValueImpl>(Type::String, std::string(value));
}

Value::Value(double value) {
    m_impl = std::make_unique<ValueImpl>(Type::Number, value);
}

Value::Value(bool value) {
    m_impl = std::make_unique<ValueImpl>(Type::Bool, value);
}

Value::Value(Array value) {
    m_impl = std::make_unique<ValueImpl>(Type::Array, value);
}

Value::Value(std::nullptr_t) {
    m_impl = std::make_unique<ValueImpl>(Type::Null, std::monostate{});
}

Value::Value(std::unique_ptr<ValueImpl> impl) : m_impl(std::move(impl)) {}

Value::Value(Value const& other) {
    m_impl = std::make_unique<ValueImpl>(*other.m_impl.get());
}

Value::Value(Value&& other) {
    m_impl.swap(other.m_impl);
}

Value& Value::operator=(Value value) {
    auto key = m_impl->key();
    m_impl.swap(value.m_impl);
    if (key) m_impl->setKey(*key);
    return *this;
}

Value::~Value() {}

Result<Value, PenisError> Value::get(std::string_view key) const {
    if (this->type() != Type::Object) {
        return Err("not an object");
    }
    auto const& arr = m_impl->asArray();
    for (auto const& value : arr) {
        if (value.m_impl->key().value() == key) {
            return Ok(value);
        }
    }
    return Err("key not found");
}

Result<Value, PenisError> Value::get(size_t index) const {
    if (this->type() != Type::Array) {
        return Err("not an array");
    }
    auto const& arr = m_impl->asArray();
    if (index >= arr.size()) {
        return Err("index out of bounds");
    }
    return Ok(arr[index]);
}

Value& Value::operator[](std::string_view key) {
    if (this->type() != Type::Object) {
        return *this;
    }
    auto& arr = m_impl->asArray();
    for (auto& value : arr) {
        if (value.m_impl->key().value() == key) {
            return value;
        }
    }
    Value val;
    val.m_impl->setKey(std::string(key));
    arr.push_back(val);
    return arr.back();
}

Type Value::type() const {
    return m_impl->type();
}

std::vector<Value>::iterator Value::begin() {
    return m_impl->asArray().begin();
}

std::vector<Value>::iterator Value::end() {
    return m_impl->asArray().end();
}

std::optional<std::string> Value::getKey() const {
    return m_impl->key();
}
