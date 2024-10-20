#include "impl.hpp"

// oh this is evil
matjson::Value* getDummyNullValue() {
    static matjson::Value nullValue = nullptr;
    return &nullValue;
}
