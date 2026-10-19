#include "impl.hpp"

#include <matjson.hpp>
#include <variant>

using namespace matjson;
using namespace geode;

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

template <bool IsConst>
struct IterState {
    using ArrayIt =
        std::conditional_t<IsConst, std::vector<Value>::const_iterator, std::vector<Value>::iterator>;
    using MapIt = std::conditional_t<IsConst, OrderedMap::const_iterator, OrderedMap::iterator>;
    using Variant = std::variant<ArrayIt, MapIt>;

    Variant it;
};

template <bool IsConst>
using ValueRef = std::conditional_t<IsConst, Value const&, Value&>;

template <bool IsConst>
ValueRef<IsConst> deref(IterState<IsConst> const& s) {
    return std::visit(
        Overloaded{
            [](auto const& it) -> ValueRef<IsConst> {
                using It = std::decay_t<decltype(it)>;
                if constexpr (std::is_same_v<It, typename IterState<IsConst>::ArrayIt>) {
                    return *it;
                }
                else {
                    It copy = it;
                    return copy.value();
                }
            },
        },
        s.it
    );
}

template <bool IsConst>
void advance(IterState<IsConst>& s) {
    std::visit(
        [](auto& it) {
            ++it;
        },
        s.it
    );
}

template <bool IsConst>
bool equal(IterState<IsConst> const& a, IterState<IsConst> const& b) {
    if (a.it.index() != b.it.index()) return false;
    return std::visit(
        [](auto const& x, auto const& y) -> bool {
            if constexpr (std::is_same_v<decltype(x), decltype(y)>) {
                return x == y;
            }
            else {
                return false;
            }
        },
        a.it,
        b.it
    );
}

// ValueIterator

struct matjson::ValueIterator::Impl : IterState<false> {};

ValueIterator::Impl* ValueIterator::impl() {
    return m_impl.get();
}

ValueIterator::Impl const* ValueIterator::impl() const {
    return m_impl.get();
}

ValueIterator::ValueIterator() {
    m_impl = std::make_unique<Impl>();
}

ValueIterator::ValueIterator(ValueIterator const& other) {
    m_impl = std::make_unique<Impl>(*other.impl());
}

ValueIterator::ValueIterator(ValueIterator&& other) noexcept {
    m_impl = std::make_unique<Impl>(std::move(*other.impl()));
}

ValueIterator::ValueIterator(ValueImpl* value, bool begin) {
    if (value->type() == Type::Array) {
        auto& arr = value->asArray();
        m_impl = std::make_unique<Impl>(Impl{begin ? arr.begin() : arr.end()});
    }
    else if (value->type() == Type::Object) {
        auto& map = value->asMap();
        m_impl = std::make_unique<Impl>(Impl{begin ? map.begin() : map.end()});
    }
    else {
        m_impl = std::make_unique<Impl>();
    }
}

ValueIterator& ValueIterator::operator=(ValueIterator const& other) {
    *impl() = *other.impl();
    return *this;
}

ValueIterator& ValueIterator::operator=(ValueIterator&& other) noexcept {
    *impl() = std::move(*other.impl());
    return *this;
}

ValueIterator::~ValueIterator() {}

Value& ValueIterator::operator*() const {
    return deref<false>(*impl());
}

Value* ValueIterator::operator->() const {
    return &**this;
}

ValueIterator& ValueIterator::operator++() {
    advance<false>(*impl());
    return *this;
}

ValueIterator ValueIterator::operator++(int) {
    auto copy = *this;
    ++*this;
    return copy;
}

bool ValueIterator::operator==(ValueIterator const& other) const {
    return equal<false>(*impl(), *other.impl());
}

bool ValueIterator::operator!=(ValueIterator const& other) const {
    return !(*this == other);
}

// ConstValueIterator

struct matjson::ConstValueIterator::Impl : IterState<true> {};

ConstValueIterator::Impl* ConstValueIterator::impl() {
    return m_impl.get();
}

ConstValueIterator::Impl const* ConstValueIterator::impl() const {
    return m_impl.get();
}

ConstValueIterator::ConstValueIterator() {
    m_impl = std::make_unique<Impl>();
}

ConstValueIterator::ConstValueIterator(ConstValueIterator const& other) {
    m_impl = std::make_unique<Impl>(*other.impl());
}

ConstValueIterator::ConstValueIterator(ConstValueIterator&& other) noexcept {
    m_impl = std::make_unique<Impl>(std::move(*other.impl()));
}

ConstValueIterator::ConstValueIterator(ValueImpl* value, bool begin) {
    if (value->type() == Type::Array) {
        auto& arr = value->asArray();
        m_impl = std::make_unique<Impl>(Impl{begin ? arr.cbegin() : arr.cend()});
    }
    else if (value->type() == Type::Object) {
        auto& map = value->asMap();
        m_impl = std::make_unique<Impl>(Impl{begin ? map.cbegin() : map.cend()});
    }
    else {
        m_impl = std::make_unique<Impl>();
    }
}

ConstValueIterator& ConstValueIterator::operator=(ConstValueIterator const& other) {
    *impl() = *other.impl();
    return *this;
}

ConstValueIterator& ConstValueIterator::operator=(ConstValueIterator&& other) noexcept {
    *impl() = std::move(*other.impl());
    return *this;
}

ConstValueIterator::~ConstValueIterator() {}

Value const& ConstValueIterator::operator*() const {
    return deref<true>(*impl());
}

Value const* ConstValueIterator::operator->() const {
    return &**this;
}

ConstValueIterator& ConstValueIterator::operator++() {
    advance<true>(*impl());
    return *this;
}

ConstValueIterator ConstValueIterator::operator++(int) {
    auto copy = *this;
    ++*this;
    return copy;
}

bool ConstValueIterator::operator==(ConstValueIterator const& other) const {
    return equal<true>(*impl(), *other.impl());
}

bool ConstValueIterator::operator!=(ConstValueIterator const& other) const {
    return !(*this == other);
}
