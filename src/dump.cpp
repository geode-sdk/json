#include "impl.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <matjson.hpp>
#include <string>

// macOS and android still lack floating point std::to_chars support
#ifndef __cpp_lib_to_chars
    #include "external/dragonbox.h"
#endif

using namespace matjson;
using namespace geode;
using namespace std::string_view_literals;

void dumpJsonString(std::string_view str, std::string& out) {
    out.push_back('"');
    for (char c : str) {
        switch (c) {
            case '"': out += "\\\""sv; break;
            case '\\': out += "\\\\"sv; break;
            case '\b': out += "\\b"sv; break;
            case '\f': out += "\\f"sv; break;
            case '\n': out += "\\n"sv; break;
            case '\r': out += "\\r"sv; break;
            case '\t': out += "\\t"sv; break;
            default: {
                if (c >= 0 && c < 0x20) {
                    std::array<char, 7> buffer;
                    snprintf(buffer.data(), buffer.size(), "\\u%04x", c);
                    out += buffer.data();
                }
                else {
                    out.push_back(c);
                }
            } break;
        }
    }
    out.push_back('"');
}

void dumpJsonNumber(ValueImpl const& impl, std::string& out) {
    if (impl.isInt()) {
        out += std::to_string(impl.asNumber<intmax_t>());
    }
    else if (impl.isUInt()) {
        out += std::to_string(impl.asNumber<uintmax_t>());
    }
    else {
        auto number = impl.asNumber<double>();
        if (std::isnan(number) || std::isinf(number)) {
            // JSON does not support inf/nan values, so copy what other libraries do
            // which is to just turn them into null
            out += "null"sv;
            return;
        }
#ifndef __cpp_lib_to_chars
        // use the dragonbox algorithm, code from
        // https://github.com/abolz/Drachennest/blob/master/src/dragonbox.cc
        std::array<char, dragonbox::DtoaMinBufferLength> buffer;
        auto* end = dragonbox::Dtoa(buffer.data(), number);
        out += std::string_view(buffer.data(), end - buffer.data());
#else
        std::array<char, 32> buffer;
        auto chars_result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
        if (chars_result.ec == std::errc::value_too_large) [[unlikely]] {
            // this should never happen
            std::abort();
        }
        out += std::string_view(buffer.data(), chars_result.ptr - buffer.data());
#endif
    }
}

void dumpImpl(Value const& value, std::string& out, int indentation, int depth) {
    auto& impl = ValueImpl::fromValue(value);
    switch (value.type()) {
        case Type::Null: {
            out += "null"sv;
        } break;
        case Type::Bool: {
            out += impl.asBool() ? "true"sv : "false"sv;
        } break;
        case Type::String: {
            dumpJsonString(impl.asString(), out);
        } break;
        case Type::Number: {
            dumpJsonNumber(impl, out);
        } break;
        case Type::Array: {
            out.push_back('[');
            auto const& arr = impl.asArray();
            bool first = true;
            for (auto const& value : arr) {
                if (!first) {
                    out.push_back(',');
                    if (indentation != NO_INDENTATION) out.push_back(' ');
                }
                first = false;
                dumpImpl(value, out, indentation, depth);
            }
            out.push_back(']');
        } break;
        case Type::Object: {
            out.push_back('{');
            auto addLine = [&](int depth) {
                if (indentation != NO_INDENTATION) {
                    out.push_back('\n');
                    char iden = indentation == TAB_INDENTATION ? '\t' : ' ';
                    size_t size = indentation == TAB_INDENTATION ? depth : depth * indentation;
                    out.reserve(out.size() + size);
                    for (size_t i = 0; i < size; ++i) {
                        out.push_back(iden);
                    }
                }
            };
            auto const& obj = impl.asArray();
            if (obj.empty()) {
                out.push_back('}');
                break;
            }

            addLine(depth + 1);
            bool first = true;
            for (auto const& [key, value] : obj) {
                if (!first) {
                    out.push_back(',');
                    addLine(depth + 1);
                }
                first = false;

                dumpJsonString(key, out);

                out.push_back(':');
                if (indentation != NO_INDENTATION) out.push_back(' ');

                dumpImpl(value, out, indentation, depth + 1);
            }
            addLine(depth);
            out.push_back('}');
        }
    }
}

std::string Value::dump(int indentationSize) const {
    std::string out;
    dumpImpl(*this, out, indentationSize, 0);
    return out;
}
