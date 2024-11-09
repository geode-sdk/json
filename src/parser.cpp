#include "impl.hpp"

#include <charconv>
#include <format>
#include <iostream>
#include <istream>
#include <matjson.hpp>
#include <sstream>
#include <string>

using namespace matjson;
using namespace geode;
using namespace std::string_view_literals;

bool isWhitespace(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

struct StringStream {
    std::istream& stream;

    Result<char, ParseError> take() noexcept {
        char ch;
        if (!stream.get(ch)) return Err("eof");
        return Ok(ch);
    }

    Result<std::string, ParseError> take(size_t n) {
        std::string buffer;
        buffer.resize(n);
        if (!stream.read(buffer.data(), n)) return Err("eof");
        return Ok(buffer);
    }

    Result<char, ParseError> peek() noexcept {
        auto ch = stream.peek();
        if (ch == EOF) return Err("eof");
        return Ok(ch);
    }

    // takes until the next char is not whitespace
    void skipWhitespace() noexcept {
        while (stream.good() && isWhitespace(stream.peek())) {
            stream.get();
        }
    }

    explicit operator bool() const noexcept {
        (void)stream.peek();
        return stream.good();
    }
};

Result<ValuePtr, ParseError> parseConstant(StringStream& stream) {
    GEODE_UNWRAP_INTO(auto first, stream.peek());
    switch (first) {
        case 't': {
            GEODE_UNWRAP_INTO(auto s, stream.take(4));
            if (s != "true"sv) break;
            return Ok(std::make_unique<ValueImpl>(Type::Bool, true));
        }
        case 'f': {
            GEODE_UNWRAP_INTO(auto s, stream.take(5));
            if (s != "false"sv) break;
            return Ok(std::make_unique<ValueImpl>(Type::Bool, false));
        }
        case 'n': {
            GEODE_UNWRAP_INTO(auto s, stream.take(4));
            if (s != "null"sv) break;
            return Ok(std::make_unique<ValueImpl>(Type::Null, std::monostate{}));
        }
        default: break;
    }
    return Err("invalid constant");
}

void encodeUTF8(std::string& str, int32_t code_point) {
    if (code_point < 0x80) {
        str.push_back(static_cast<char>(code_point));
    }
    else if (code_point < 0x800) {
        str.push_back(static_cast<char>(0b11000000 | ((code_point & 0b11111'000000) >> 6)));
        str.push_back(static_cast<char>(0b10000000 | (code_point & 0b00000'111111)));
    }
    else if (code_point < 0x10000) {
        str.push_back(static_cast<char>(0b11100000 | ((code_point & 0b1111'000000'000000) >> 12)));
        str.push_back(static_cast<char>(0b10000000 | ((code_point & 0b0000'111111'000000) >> 6)));
        str.push_back(static_cast<char>(0b10000000 | (code_point & 0b0000'000000'111111)));
    }
    else {
        str.push_back(
            static_cast<char>(0b11110000 | ((code_point & 0b111'000000'000000'000000) >> 18))
        );
        str.push_back(
            static_cast<char>(0b10000000 | ((code_point & 0b000'111111'000000'000000) >> 12))
        );
        str.push_back(static_cast<char>(0b10000000 | ((code_point & 0b000'000000'111111'000000) >> 6))
        );
        str.push_back(static_cast<char>(0b10000000 | (code_point & 0b000'000000'000000'111111)));
    }
}

Result<std::string, ParseError> parseString(StringStream& stream) noexcept {
    // when this function is called we already know the first character is a quote
    GEODE_UNWRAP(stream.take());
    std::string str;
    GEODE_UNWRAP_INTO(char p, stream.peek());
    while (p != '"') {
        GEODE_UNWRAP_INTO(char c, stream.take());
        // char is signed, so utf8 high bit bytes will be interpreted as negative,
        // could switch to unsigned char however just checking for c < 0 is easier
        if (c >= 0 && c < 0x20) {
            return Err("invalid string");
        }
        // FIXME: standard should also ignore > 0x10FFFF, however that would require decoding utf-8
        if (c == '\\') {
            GEODE_UNWRAP_INTO(char s, stream.take());
            switch (s) {
                case '"': str.push_back('"'); break;
                case '\\': str.push_back('\\'); break;
                case '/': str.push_back('/'); break;
                case 'b': str.push_back('\b'); break;
                case 'f': str.push_back('\f'); break;
                case 'n': str.push_back('\n'); break;
                case 'r': str.push_back('\r'); break;
                case 't': str.push_back('\t'); break;
                case 'u': {
                    auto const takeHexChar = [&]() -> Result<uint32_t, ParseError> {
                        GEODE_UNWRAP_INTO(char c, stream.take());
                        if (c >= '0' && c <= '9') return Ok(static_cast<uint32_t>(c - '0'));
                        else if (c >= 'a' && c <= 'f')
                            return Ok(static_cast<uint32_t>(c - 'a' + 10));
                        else if (c >= 'A' && c <= 'F')
                            return Ok(static_cast<uint32_t>(c - 'A' + 10));
                        return Err("invalid hex");
                    };
                    auto const takeUnicodeHex = [&]() -> Result<int32_t, ParseError> {
                        int32_t result = 0;
                        for (int i = 0; i < 4; ++i) {
                            GEODE_UNWRAP_INTO(uint32_t temp, takeHexChar());
                            result = (result << 4) | temp;
                        }
                        return Ok(result);
                    };
                    GEODE_UNWRAP_INTO(int32_t value, takeUnicodeHex());
                    // surrogate pair, so read another unicode hex
                    if (0xd800 <= value && value <= 0xdbff) {
                        GEODE_UNWRAP_INTO(char c, stream.take());
                        if (c != '\\') {
                            return Err("expected backslash");
                        }
                        GEODE_UNWRAP_INTO(c, stream.take());
                        if (c != 'u') {
                            return Err("expected u");
                        }
                        GEODE_UNWRAP_INTO(int32_t value2, takeUnicodeHex());
                        if (0xdc00 <= value2 && value2 <= 0xdfff) {
                            value = 0x10000 + ((value & 0x3ff) << 10) + (value2 & 0x3ff);
                        }
                        else {
                            return Err("invalid surrogate pair");
                        }
                    }
                    encodeUTF8(str, value);
                } break;
                default: return Err("invalid backslash escape");
            }
        }
        else {
            str.push_back(c);
        }
        GEODE_UNWRAP_INTO(p, stream.peek());
    }
    // eat the "
    GEODE_UNWRAP(stream.take());
    return Ok(str);
}

Result<ValuePtr, ParseError> parseNumber(StringStream& stream) noexcept {
    std::string buffer;
    bool isFloating = false;
    bool isNegative = false;
    auto const addToBuffer = [&]() -> Result<void, ParseError> {
        GEODE_UNWRAP_INTO(char c, stream.take());
        buffer.push_back(c);
        return Ok();
    };
    GEODE_UNWRAP_INTO(char p, stream.peek());
    if (p == '-') {
        isNegative = true;
        GEODE_UNWRAP(addToBuffer());
    }
    auto const takeDigits = [&]() -> Result<void, ParseError> {
        bool once = false;
        while (stream) {
            auto result = stream.peek();
            if (!result) break;
            char c = result.unwrap();
            if (c >= '0' && c <= '9') {
                once = true;
                GEODE_UNWRAP(addToBuffer());
            }
            else {
                break;
            }
        }
        if (!once) {
            return Err("expected digits");
        }
        return Ok();
    };
    GEODE_UNWRAP_INTO(p, stream.peek());
    if (p == '0') {
        GEODE_UNWRAP(addToBuffer());
    }
    else {
        GEODE_UNWRAP(takeDigits());
    }
    // these are optional!
    if (stream) {
        // fraction
        GEODE_UNWRAP_INTO(p, stream.peek());
        if (p == '.') {
            isFloating = true;
            GEODE_UNWRAP(addToBuffer());

            GEODE_UNWRAP(takeDigits());
        }
    }
    if (stream) {
        // exponent
        GEODE_UNWRAP_INTO(p, stream.peek());
        if (p == 'e' || p == 'E') {
            isFloating = true;
            GEODE_UNWRAP(addToBuffer());

            GEODE_UNWRAP_INTO(p, stream.peek());
            if (p == '-' || p == '+') {
                GEODE_UNWRAP(addToBuffer());
            }
            GEODE_UNWRAP(takeDigits());
        }
    }
    auto const fromCharsHelper = [&]<class T>() -> Result<ValuePtr, ParseError> {
        T value;
        if (auto result = std::from_chars(buffer.data(), buffer.data() + buffer.size(), value);
            result.ec != std::errc()) {
            return Err("failed to parse number");
        }
        return Ok(std::make_unique<ValueImpl>(Type::Number, value));
    };
    if (isFloating) {
#ifndef __cpp_lib_to_chars
        // FIXME: std::stod is locale specific, might break on some machines
        return Ok(std::make_unique<ValueImpl>(Type::Number, std::stod(buffer)));
#else
        return fromCharsHelper.operator()<double>();
#endif
    }
    else if (isNegative) {
        return fromCharsHelper.operator()<intmax_t>();
    }
    else {
        return fromCharsHelper.operator()<uintmax_t>();
    }
}

// parses a json element with optional whitespace around it
Result<ValuePtr, ParseError> parseElement(StringStream& stream) noexcept;

Result<ValuePtr, ParseError> parseObject(StringStream& stream) noexcept {
    GEODE_UNWRAP(stream.take());
    stream.skipWhitespace();
    Array object;
    GEODE_UNWRAP_INTO(char p, stream.peek());
    if (p != '}') {
        while (true) {
            stream.skipWhitespace();
            GEODE_UNWRAP_INTO(auto key, parseString(stream));
            stream.skipWhitespace();

            GEODE_UNWRAP_INTO(char s, stream.take());
            if (s != ':') {
                return Err("expected colon");
            }

            GEODE_UNWRAP_INTO(auto value, parseElement(stream));
            value->setKey(key);
            object.push_back(ValueImpl::asValue(std::move(value)));

            GEODE_UNWRAP_INTO(char c, stream.peek());
            if (c == ',') {
                GEODE_UNWRAP(stream.take());
            }
            else if (c == '}') {
                break;
            }
            else {
                return Err("expected member");
            }
        }
    }
    // eat the }
    GEODE_UNWRAP(stream.take());
    return Ok(std::make_unique<ValueImpl>(Type::Object, object));
}

Result<ValuePtr, ParseError> parseArray(StringStream& stream) noexcept {
    GEODE_UNWRAP(stream.take());
    stream.skipWhitespace();
    Array array;
    GEODE_UNWRAP_INTO(char p, stream.peek());
    if (p != ']') {
        while (true) {
            GEODE_UNWRAP_INTO(auto element, parseElement(stream));
            array.push_back(ValueImpl::asValue(std::move(element)));

            GEODE_UNWRAP_INTO(char c, stream.peek());
            if (c == ',') {
                GEODE_UNWRAP(stream.take());
            }
            else if (c == ']') {
                break;
            }
            else {
                return Err("expected value");
            }
        }
    }
    // eat the ]
    GEODE_UNWRAP(stream.take());
    return Ok(std::make_unique<ValueImpl>(Type::Array, array));
}

Result<ValuePtr, ParseError> parseValue(StringStream& stream) noexcept {
    GEODE_UNWRAP_INTO(char p, stream.peek());
    switch (p) {
        case 't':
        case 'f':
        case 'n': return parseConstant(stream);
        case '"': {
            return parseString(stream).map([](auto value) {
                return std::make_unique<ValueImpl>(Type::String, value);
            });
        }
        case '{': return parseObject(stream);
        case '[': return parseArray(stream);
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': return parseNumber(stream);
        default: return Err("invalid value");
    }
}

Result<ValuePtr, ParseError> parseElement(StringStream& stream) noexcept {
    stream.skipWhitespace();
    GEODE_UNWRAP_INTO(auto value, parseValue(stream));
    stream.skipWhitespace();
    return Ok(std::move(value));
}

Result<Value, ParseError> Value::parse(std::istream& sourceStream) {
    StringStream stream{sourceStream};

    return parseElement(stream).map([](auto impl) {
        return ValueImpl::asValue(std::move(impl));
    });
}

Result<Value, ParseError> Value::parse(std::string_view source) {
    std::istringstream strStream{std::string(source)};
    return Value::parse(strStream);
}
