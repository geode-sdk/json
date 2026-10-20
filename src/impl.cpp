#include "impl.hpp"

// this dummy value is used for operator[], where
// we must return a reference, but said function can fail.
// this dummy object is special in that it can never be mutated
matjson::Value* getDummyNullValue() {
    static auto* nullValue = new matjson::Value(nullptr);
    return nullValue;
}
