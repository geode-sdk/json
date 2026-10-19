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
    // empty m_impl
}

Value::Value(std::unique_ptr<ValueImpl> impl) : m_impl(std::move(impl)) {}

Value::Value(Value const& other) {
    if (other.m_impl) {
        m_impl = std::make_unique<ValueImpl>(*other.m_impl);
    }
}

Value::Value(Value&& other) noexcept {
    if (other.m_impl && other.m_impl->key()) {
        auto key = *other.m_impl->key();
        m_impl = std::move(other.m_impl);
        // cant have empty m_impl here, since it had a key
        other.m_impl = ValueImpl::makeKeyedNull(std::move(key));
    }
    else {
        m_impl = std::move(other.m_impl);
        // other is left with an empty m_impl, which is fine
    }
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
    if (this == &other) return *this;
    // assignment never changes which object entry this is
    auto key = this->getKey();
    std::unique_ptr<ValueImpl> copy;
    if (other.m_impl) {
        copy = std::make_unique<ValueImpl>(*other.m_impl);
    }
    m_impl = std::move(copy);
    if (key) {
        this->setKey_(*key);
    }
    else if (m_impl) {
        // didnt have a key before, so make sure not to copy other's key
        m_impl->clearKey();
    }
    return *this;
}

Value& Value::operator=(Value&& other) {
    if (CHECK_DUMMY_NULL) return *this;
    if (this == &other) return *this;

    // `other` may live inside of our old node (e.g. `v = std::move(v["child"])`),
    // so keep the old node alive until we are done touching `other`
    auto old = std::move(m_impl);
    std::optional<std::string> myKey;
    if (old) myKey = std::move(old->key());

    std::optional<std::string> otherKey;
    if (other.m_impl) otherKey = std::move(other.m_impl->key());

    m_impl = std::move(other.m_impl);
    if (myKey) {
        if (m_impl) m_impl->setKey(std::move(*myKey));
        else m_impl = ValueImpl::makeKeyedNull(std::move(*myKey));
    }
    else if (m_impl) {
        m_impl->clearKey();
    }
    // if other had a key, make sure it has an m_impl
    if (otherKey) {
        other.m_impl = ValueImpl::makeKeyedNull(std::move(*otherKey));
    }
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
    auto it = map.find(key);
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
    if (auto found = map.find(key); found != map.end()) {
        return found.value();
    }
    std::string owned(key);
    auto [it, inserted] = map.try_emplace(owned);
    it.value().m_impl->setKey(std::move(owned));
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
    if (auto found = map.find(key); found != map.end()) {
        found.value() = std::move(value);
        return;
    }
    auto [it, inserted] = map.try_emplace(std::string(key), std::move(value));
    it.value().setKey_(key);
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
    return map.erase(key) == 1;
}

bool Value::contains(std::string_view key) const {
    if (this->type() != Type::Object) {
        return false;
    }
    return m_impl->asMap().contains(key);
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
    return m_impl ? m_impl->type() : Type::Null;
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
    if (!m_impl) return std::nullopt;
    return m_impl->key();
}

void Value::setKey_(std::string_view key) {
    if (CHECK_DUMMY_NULL) return;
    if (!m_impl) {
        // need m_impl to store keys
        m_impl = ValueImpl::makeKeyedNull(std::string(key));
        return;
    }
    m_impl->setKey(std::string(key));
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

Result<float> Value::asFloat() const {
    if (this->type() != Type::Number) {
        return Err("not a number");
    }
    return Ok(m_impl->asNumber<float>());
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

// If either side has no m_impl it is a null, so only the types can differ
bool Value::operator==(Value const& other) const {
    if (!m_impl || !other.m_impl) return this->type() == other.type();
    return *m_impl == *other.m_impl;
}

bool Value::operator<(Value const& other) const {
    if (!m_impl || !other.m_impl) return this->type() < other.type();
    return *m_impl < *other.m_impl;
}

bool Value::operator>(Value const& other) const {
    if (!m_impl || !other.m_impl) return other.type() < this->type();
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
