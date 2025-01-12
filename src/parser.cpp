#include "impl.hpp"

#include <charconv>
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

template <class S = std::istream&>
struct StringStream {
    S stream;
    std::streambuf* buffer = nullptr;
    int line = 1, column = 1, offset = 0;

    static constexpr bool isStream = std::is_same_v<S, std::istream&>;

    auto error(std::string_view msg) const noexcept {
        return Err(ParseError(std::string(msg), offset, line, column));
    }

    Result<char, ParseError> take() noexcept {
        char ch;
        if constexpr (isStream) {
            auto res = buffer->sbumpc();
            if (res == std::char_traits<char>::eof()) {
                stream.clear(stream.rdstate() | std::ios::eofbit);
                return this->error("eof");
            }
            ch = static_cast<char>(res);
        } else {
            if (stream.empty()) return this->error("eof");
            ch = stream[0];
            stream = stream.substr(1);
        }
        ++offset;
        if (ch == '\n') {
            ++line;
            column = 1;
        }
        else {
            ++column;
        }
        return Ok(ch);
    }

    Result<std::string, ParseError> take(size_t n) {
        // this is only used for constants so its fine to not count lines
        if constexpr (isStream) {
            std::string str;
            str.resize(n);
            auto res = static_cast<std::size_t>(buffer->sgetn(str.data(), static_cast<std::streamsize>(n)));
            if (res < n) {
                stream.clear(stream.rdstate() | std::ios::eofbit);
                return this->error("eof");
            }
            column += n;
            offset += n;
            return Ok(std::move(str));
        } else {
            if (stream.size() < n) return this->error("eof");
            std::string buffer = std::string(stream.substr(0, n));
            stream = stream.substr(n);
            column += n;
            offset += n;
            return Ok(std::move(buffer));
        }
    }

    Result<char, ParseError> peek() noexcept {
        if constexpr (isStream) {
            auto ret = buffer->sgetc();
            if (ret == std::char_traits<char>::eof()) {
                stream.clear(stream.rdstate() | std::ios::eofbit);
                return this->error("eof");
            }
            return Ok(static_cast<char>(ret));
        } else {
            if (stream.empty()) return this->error("eof");
            return Ok(stream[0]);
        }
    }

    // takes until the next char is not whitespace
    void skipWhitespace() noexcept {
        if constexpr (isStream) {
            // while (stream.good() && isWhitespace(stream.peek())) {
            //     char ch = stream.get();
            //     ++offset;
            //     if (ch == '\n') {
            //         ++line;
            //         column = 1;
            //     }
            //     else {
            //         ++column;
            //     }
            // }
            while (true) {
                auto ret = buffer->sgetc();
                if (ret == std::char_traits<char>::eof()) {
                    stream.clear(stream.rdstate() | std::ios::eofbit);
                    return;
                }
                char ch = static_cast<char>(ret);
                if (!isWhitespace(ch)) return;
                buffer->sbumpc();
                ++offset;
                if (ch == '\n') {
                    ++line;
                    column = 1;
                }
                else {
                    ++column;
                }
            }
        } else {
            while (!stream.empty() && isWhitespace(stream[0])) {
                char ch = stream[0];
                stream = stream.substr(1);
                ++offset;
                if (ch == '\n') {
                    ++line;
                    column = 1;
                }
                else {
                    ++column;
                }
            }
        }
    }

    explicit operator bool() const noexcept {
        if constexpr (isStream) {
            // (void)stream.peek();
            // return stream.good();
            return buffer->sgetc() != std::char_traits<char>::eof();
        } else {
            return !stream.empty();
        }
    }
};

template <class S>
Result<ValuePtr, ParseError> parseConstant(StringStream<S>& stream) {
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
    return stream.error("invalid constant");
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

template <class S>
Result<std::string, ParseError> parseString(StringStream<S>& stream) noexcept {
    // when this function is called we already know the first character is a quote
    GEODE_UNWRAP(stream.take());
    std::string str;
    GEODE_UNWRAP_INTO(char p, stream.peek());
    while (p != '"') {
        GEODE_UNWRAP_INTO(char c, stream.take());
        // char is signed, so utf8 high bit bytes will be interpreted as negative,
        // could switch to unsigned char however just checking for c < 0 is easier
        if (c >= 0 && c < 0x20) {
            return stream.error("invalid string");
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
                        return stream.error("invalid hex");
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
                            return stream.error("expected backslash");
                        }
                        GEODE_UNWRAP_INTO(c, stream.take());
                        if (c != 'u') {
                            return stream.error("expected u");
                        }
                        GEODE_UNWRAP_INTO(int32_t value2, takeUnicodeHex());
                        if (0xdc00 <= value2 && value2 <= 0xdfff) {
                            value = 0x10000 + ((value & 0x3ff) << 10) + (value2 & 0x3ff);
                        }
                        else {
                            return stream.error("invalid surrogate pair");
                        }
                    }
                    encodeUTF8(str, value);
                } break;
                default: return stream.error("invalid backslash escape");
            }
        }
        else {
            str.push_back(c);
        }
        GEODE_UNWRAP_INTO(p, stream.peek());
    }
    // eat the "
    GEODE_UNWRAP(stream.take());
    return Ok(std::move(str));
}

template <class S>
Result<ValuePtr, ParseError> parseNumber(StringStream<S>& stream) noexcept {
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
            return stream.error("expected digits");
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
            return stream.error("failed to parse number");
        }
        return Ok(std::make_unique<ValueImpl>(Type::Number, value));
    };
    if (isFloating) {
#ifndef __cpp_lib_to_chars
        // FIXME: std::stod is locale specific, might break on some machines
        return Ok(std::make_unique<ValueImpl>(Type::Number, std::stod(buffer)));
#else
        return fromCharsHelper.template operator()<double>();
#endif
    }
    else if (isNegative) {
        return fromCharsHelper.template operator()<intmax_t>();
    }
    else {
        return fromCharsHelper.template operator()<uintmax_t>();
    }
}

// parses a json element with optional whitespace around it
template <class S>
Result<ValuePtr, ParseError> parseElement(StringStream<S>& stream) noexcept;

template <class S>
Result<ValuePtr, ParseError> parseObject(StringStream<S>& stream) noexcept {
    GEODE_UNWRAP(stream.take());
    stream.skipWhitespace();
    std::vector<Value> object;
    GEODE_UNWRAP_INTO(char p, stream.peek());
    if (p != '}') {
        while (true) {
            stream.skipWhitespace();
            {
                GEODE_UNWRAP_INTO(char c, stream.peek());
                if (c != '"') {
                    return stream.error("expected string");
                }
            }
            GEODE_UNWRAP_INTO(auto key, parseString(stream));
            stream.skipWhitespace();

            GEODE_UNWRAP_INTO(char s, stream.take());
            if (s != ':') {
                return stream.error("expected colon");
            }

            GEODE_UNWRAP_INTO(auto value, parseElement(stream));
            value->setKey(std::move(key));
            object.emplace_back(std::move(ValueImpl::asValue(std::move(value))));

            GEODE_UNWRAP_INTO(char c, stream.peek());
            if (c == ',') {
                GEODE_UNWRAP(stream.take());
            }
            else if (c == '}') {
                break;
            }
            else {
                return stream.error("expected comma");
            }
        }
    }
    // eat the }
    GEODE_UNWRAP(stream.take());
    return Ok(std::make_unique<ValueImpl>(Type::Object, std::move(object)));
}

template <class S>
Result<ValuePtr, ParseError> parseArray(StringStream<S>& stream) noexcept {
    GEODE_UNWRAP(stream.take());
    stream.skipWhitespace();
    std::vector<Value> array;
    GEODE_UNWRAP_INTO(char p, stream.peek());
    if (p != ']') {
        while (true) {
            GEODE_UNWRAP_INTO(auto element, parseElement(stream));
            array.emplace_back(std::move(ValueImpl::asValue(std::move(element))));

            GEODE_UNWRAP_INTO(char c, stream.peek());
            if (c == ',') {
                GEODE_UNWRAP(stream.take());
            }
            else if (c == ']') {
                break;
            }
            else {
                return stream.error("expected value");
            }
        }
    }
    // eat the ]
    GEODE_UNWRAP(stream.take());
    return Ok(std::make_unique<ValueImpl>(Type::Array, std::move(array)));
}

template <class S>
Result<ValuePtr, ParseError> parseValue(StringStream<S>& stream) noexcept {
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
        default: return stream.error("invalid value");
    }
}

template <class S>
Result<ValuePtr, ParseError> parseElement(StringStream<S>& stream) noexcept {
    stream.skipWhitespace();
    GEODE_UNWRAP_INTO(auto value, parseValue(stream));
    stream.skipWhitespace();
    return Ok(std::move(value));
}

template <class S>
Result<ValuePtr, ParseError> parseRoot(StringStream<S>& stream) noexcept {
    GEODE_UNWRAP_INTO(auto value, parseElement(stream));
    // if theres anything left in the stream that is not whitespace
    // it should be considered an error
    if (stream) {
        return stream.error("expected eof");
    }
    return Ok(std::move(value));
}

Result<Value, ParseError> Value::parse(std::istream& sourceStream) {
    StringStream<std::istream&> stream{sourceStream, sourceStream.rdbuf()};

    return parseRoot(stream).map([](auto impl) {
        return ValueImpl::asValue(std::move(impl));
    });
}

Result<Value, ParseError> Value::parse(std::string_view source) {
    StringStream<std::string_view> stream{source, nullptr};

    return parseRoot(stream).map([](auto impl) {
        return ValueImpl::asValue(std::move(impl));
    });
}
