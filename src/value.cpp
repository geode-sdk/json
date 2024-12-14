#include "impl.hpp"

#include <matjson.hpp>

using namespace matjson;
using namespace geode;

Value::Value() {
    m_impl = std::make_unique<ValueImpl>(Type::Object, std::vector<Value>{});
}

Value::Value(char const* str) : Value(std::string(str)) {}

Value::Value(std::string value) {
    m_impl = std::make_unique<ValueImpl>(Type::String, std::move(value));
}

Value::Value(std::string_view value) {
    m_impl = std::make_unique<ValueImpl>(Type::String, std::string(value));
}

Value::Value(double value) {
    m_impl = std::make_unique<ValueImpl>(Type::Number, value);
}

Value::Value(intmax_t value) {
    m_impl = std::make_unique<ValueImpl>(Type::Number, value);
}

Value::Value(uintmax_t value) {
    m_impl = std::make_unique<ValueImpl>(Type::Number, value);
}

Value::Value(bool value) {
    m_impl = std::make_unique<ValueImpl>(Type::Bool, value);
}

Value::Value(std::vector<Value> value) {
    m_impl = std::make_unique<ValueImpl>(Type::Array, std::move(value));
}

Value::Value(std::vector<Value> value, bool) {
    m_impl = std::make_unique<ValueImpl>(Type::Object, value);
}

Value::Value(std::nullptr_t) {
    m_impl = std::make_unique<ValueImpl>(Type::Null, std::monostate{});
}

Value::Value(std::unique_ptr<ValueImpl> impl) : m_impl(std::move(impl)) {}

Value::Value(Value const& other) {
    m_impl = std::make_unique<ValueImpl>(*other.m_impl.get());
}

Value::Value(Value&& other) noexcept {
    if (other.m_impl == getDummyNullValue()->m_impl) {
        m_impl = std::make_unique<ValueImpl>(Type::Null, std::monostate{});
        return;
    }
    m_impl.swap(other.m_impl);
    // this is silly but its easier than checking m_impl == nullptr for a moved Value everywhere else
    other.m_impl = std::make_unique<ValueImpl>(Type::Null, std::monostate{});
}

Value::~Value() {}

Value Value::object() {
    return Value(std::make_unique<ValueImpl>(Type::Object, std::vector<Value>{}));
}

Value Value::array() {
    return Value(std::make_unique<ValueImpl>(Type::Array, std::vector<Value>{}));
}

Value& Value::operator=(Value value) {
    if (CHECK_DUMMY_NULL) return *this;
    auto key = m_impl->key();
    m_impl.swap(value.m_impl);
    if (key) m_impl->setKey(*key);
    return *this;
}

static Value& asNotConst(Value const& value) {
    return const_cast<Value&>(value);
}

Result<Value&> Value::get(std::string_view key) {
    return std::as_const(*this).get(key).map(asNotConst);
}

Result<Value const&> Value::get(std::string_view key) const {
    if (this->type() != Type::Object) {
        return Err("not an object");
    }
    auto& arr = m_impl->asArray();
    for (auto& value : arr) {
        if (value.m_impl->key().value() == key) {
            return Ok(value);
        }
    }
    return Err("key not found");
}

Result<Value&> Value::get(size_t index) {
    return std::as_const(*this).get(index).map(asNotConst);
}

Result<Value const&> Value::get(size_t index) const {
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
        return *getDummyNullValue();
    }
    auto& arr = m_impl->asArray();
    for (auto& value : arr) {
        if (value.m_impl->key().value() == key) {
            return value;
        }
    }
    arr.emplace_back(Value());
    arr.back().m_impl->setKey(std::string(key));
    return arr.back();
}

Value const& Value::operator[](std::string_view key) const {
    return this->get(key).unwrapOrElse([]() -> Value const& {
        return *getDummyNullValue();
    });
}

Value& Value::operator[](size_t index) {
    return this->get(index).unwrapOrElse([]() -> Value& {
        return *getDummyNullValue();
    });
}

Value const& Value::operator[](size_t index) const {
    return this->get(index).unwrapOrElse([]() -> Value const& {
        return *getDummyNullValue();
    });
}

void Value::set(std::string_view key, Value value) {
    if (this->type() != Type::Object) {
        return;
    }
    (void)this->get(key)
        .inspect([&value](Value& v) {
            v = std::move(value);
        })
        .inspectErr([&](auto&&) {
            auto& arr = m_impl->asArray();
            arr.emplace_back(std::move(value));
            arr.back().m_impl->setKey(std::string(key));
        });
}

void Value::push(Value value) {
    if (this->type() != Type::Array) {
        return;
    }
    m_impl->asArray().emplace_back(std::move(value));
}

void Value::clear() {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return;
    }
    m_impl->asArray().clear();
}

bool Value::erase(std::string_view key) {
    if (this->type() != Type::Object) {
        return false;
    }
    auto& arr = m_impl->asArray();
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        if (it->m_impl->key().value() == key) {
            arr.erase(it);
            return true;
        }
    }
    return false;
}

bool Value::contains(std::string_view key) const {
    if (this->type() != Type::Object) {
        return false;
    }
    auto const& arr = m_impl->asArray();
    for (auto const& value : arr) {
        if (value.m_impl->key().value() == key) {
            return true;
        }
    }
    return false;
}

size_t Value::size() const {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return 0;
    }
    return m_impl->asArray().size();
}

Type Value::type() const {
    return m_impl->type();
}

std::vector<Value>::iterator Value::begin() {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return {};
    }
    return m_impl->asArray().begin();
}

std::vector<Value>::iterator Value::end() {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return {};
    }
    return m_impl->asArray().end();
}

std::vector<Value>::const_iterator Value::begin() const {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return {};
    }
    return m_impl->asArray().begin();
}

std::vector<Value>::const_iterator Value::end() const {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return {};
    }
    return m_impl->asArray().end();
}

std::optional<std::string> Value::getKey() const {
    return m_impl->key();
}

void Value::setKey_(std::string_view key) {
    if (CHECK_DUMMY_NULL) return;
    return m_impl->setKey(std::string(key));
}

Result<bool> Value::asBool() const {
    if (this->type() != Type::Bool) {
        return Err("not a bool");
    }
    return Ok(m_impl->asBool());
}

Result<std::string> Value::asString() const {
    if (this->type() != Type::String) {
        return Err("not a string");
    }
    return Ok(m_impl->asString());
}

Result<intmax_t> Value::asInt() const {
    if (this->type() != Type::Number) {
        return Err("not a number");
    }
    return Ok(m_impl->asNumber<intmax_t>());
}

Result<uintmax_t> Value::asUInt() const {
    if (this->type() != Type::Number) {
        return Err("not a number");
    }
    return Ok(m_impl->asNumber<uintmax_t>());
}

Result<double> Value::asDouble() const {
    if (this->type() != Type::Number) {
        return Err("not a number");
    }
    return Ok(m_impl->asNumber<double>());
}

Result<std::vector<Value>&> Value::asArray() & {
    if (this->type() != Type::Array) {
        return Err("not an array");
    }
    return Ok(m_impl->asArray());
}

Result<std::vector<Value>> Value::asArray() && {
    if (this->type() != Type::Array) {
        return Err("not an array");
    }
    return Ok(std::move(m_impl->asArray()));
}

Result<std::vector<Value> const&> Value::asArray() const& {
    if (this->type() != Type::Array) {
        return Err("not an array");
    }
    return Ok(m_impl->asArray());
}

bool Value::operator==(Value const& other) const {
    return *m_impl == *other.m_impl;
}

bool Value::operator<(Value const& other) const {
    return *m_impl < *other.m_impl;
}

bool Value::operator>(Value const& other) const {
    return *m_impl > *other.m_impl;
}
