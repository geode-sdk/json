#include "impl.hpp"

#include <matjson.hpp>
#include <variant>

using namespace matjson;
using namespace geode;

ValueIterator::ValueIterator(ValueImpl* value, bool begin) {
    if (value->type() == Type::Array) {
        auto& arr = value->asArray();
        data = begin ? arr.begin() : arr.end();
    }
    else if (value->type() == Type::Object) {
        auto& map = value->asMap();
        data = begin ? map.begin().m_iterator : map.end().m_iterator;
    }
}

ConstValueIterator::ConstValueIterator(ValueImpl* value, bool begin) {
    if (value->type() == Type::Array) {
        auto& arr = value->asArray();
        data = begin ? arr.cbegin() : arr.cend();
    }
    else if (value->type() == Type::Object) {
        auto& map = value->asMap();
        data = begin ? map.cbegin().m_iterator : map.cend().m_iterator;
    }
}
