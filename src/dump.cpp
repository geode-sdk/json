#include "external/zmij.h"
#include "impl.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <matjson.hpp>
#include <string>

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
        auto const dumpFloat = [&](auto number) {
            if (std::isnan(number) || std::isinf(number)) {
                // JSON does not support inf/nan values, so copy what other libraries do
                // which is to just turn them into null
                out += "null"sv;
                return;
            }
            // always use zmij instead of to_chars for consistency across platforms
            std::array<char, 40> buffer;
            auto* end = zmij::write(buffer.data(), buffer.size(), number);
            auto str = std::string_view(buffer.data(), end - buffer.data());
            out += str;
            if (str.find('.') == std::string_view::npos && str.find('e') == std::string_view::npos) {
                out += ".0";
            }
        };
        if (impl.isFloat()) {
            dumpFloat(impl.asNumber<float>());
        }
        else {
            dumpFloat(impl.asNumber<double>());
        }
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
