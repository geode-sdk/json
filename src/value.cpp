#include "impl.hpp"

#include <matjson.hpp>

using namespace matjson;
using namespace geode;

Value::Value() {
    m_impl = std::make_unique<ValueImpl>(ObjCtorTag{}, std::span<Value>{});
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

Value::Value(float value) {
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

Value::Value(ObjCtorTag, std::span<Value> value) {
    m_impl = std::make_unique<ValueImpl>(ObjCtorTag{}, value);
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
    // copy it
    auto key = other.getKey();
    m_impl.swap(other.m_impl);

    other.m_impl = std::make_unique<ValueImpl>(Type::Null, std::monostate{});
    other.m_impl->key() = std::move(key);
}

Value::~Value() {}

Value Value::object() {
    return Value(std::make_unique<ValueImpl>(ObjCtorTag{}, std::span<Value>{}));
}

Value Value::array() {
    return Value(std::make_unique<ValueImpl>(Type::Array, std::vector<Value>{}));
}

Value& Value::operator=(Value const& other) {
    if (CHECK_DUMMY_NULL) return *this;
    auto key = this->getKey();
    m_impl = std::make_unique<ValueImpl>(*other.m_impl.get());
    m_impl->key() = std::move(key);
    return *this;
}

Value& Value::operator=(Value&& other) {
    if (CHECK_DUMMY_NULL) return *this;
    if (this == &other) return *this;

    auto myKey = std::move(m_impl->key());

    if (other.m_impl == getDummyNullValue()->m_impl) {
        m_impl = std::make_unique<ValueImpl>(Type::Null, std::monostate{});
        m_impl->key() = std::move(myKey);
        return *this;
    }

    auto otherKey = std::move(other.m_impl->key());
    m_impl.swap(other.m_impl);
    m_impl->key() = std::move(myKey);

    other.m_impl = std::make_unique<ValueImpl>(Type::Null, std::monostate{});
    other.m_impl->key() = std::move(otherKey);
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
    auto& map = m_impl->asMap();
    auto it = map.find(std::string(key));
    if (it != map.end()) {
        return Ok(it.value());
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
    auto& map = m_impl->asMap();
    auto [it, inserted] = map.try_emplace(std::string(key));
    if (inserted) {
        it.value().m_impl->setKey(std::string(key));
    }
    return it.value();
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
    auto& map = m_impl->asMap();
    auto [it, inserted] = map.insert_or_assign(std::string(key), std::move(value));
    if (inserted) {
        it.value().m_impl->setKey(std::string(key));
    }
}

void Value::push(Value value) {
    if (this->type() != Type::Array) {
        return;
    }
    m_impl->asArray().emplace_back(std::move(value));
}

void Value::clear() {
    if (this->type() == Type::Array) {
        m_impl->asArray().clear();
    }
    else if (this->type() == Type::Object) {
        m_impl->asMap().clear();
    }
}

bool Value::erase(std::string_view key) {
    if (this->type() != Type::Object) {
        return false;
    }
    auto& map = m_impl->asMap();
    return map.erase(std::string(key)) == 1;
}

bool Value::contains(std::string_view key) const {
    if (this->type() != Type::Object) {
        return false;
    }
    return m_impl->asMap().contains(std::string(key));
}

size_t Value::size() const {
    if (this->type() == Type::Array) {
        return m_impl->asArray().size();
    }
    else if (this->type() == Type::Object) {
        return m_impl->asMap().size();
    }
    else {
        return 0;
    }
}

Type Value::type() const {
    return m_impl->type();
}

ValueIterator Value::begin() {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return {};
    }
    return ValueIterator(m_impl.get(), true);
}

ValueIterator Value::end() {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return {};
    }
    return ValueIterator(m_impl.get(), false);
}

ConstValueIterator Value::begin() const {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return {};
    }
    return ConstValueIterator(m_impl.get(), true);
}

ConstValueIterator Value::end() const {
    if (this->type() != Type::Array && this->type() != Type::Object) {
        return {};
    }
    return ConstValueIterator(m_impl.get(), false);
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

bool Value::isExactlyDouble() const {
    if (!this->isNumber()) return false;
    return m_impl->isDouble();
}

bool Value::isExactlyFloat() const {
    if (!this->isNumber()) return false;
    return m_impl->isFloat();
}

bool Value::isExactlyInt() const {
    if (!this->isNumber()) return false;
    return m_impl->isInt();
}

bool Value::isExactlyUInt() const {
    if (!this->isNumber()) return false;
    return m_impl->isUInt();
}
