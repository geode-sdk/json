#pragma once

#include <Geode/Result.hpp>
#include <istream>
#include <memory>
#include <type_traits>
#include <vector>

#ifdef MAT_JSON_DYNAMIC
    #if defined(_WIN32) && !defined(__CYGWIN__)
        #ifdef MAT_JSON_EXPORTING
            #define MAT_JSON_DLL __declspec(dllexport)
        #else
            #define MAT_JSON_DLL __declspec(dllimport)
        #endif
    #else
        #ifdef MAT_JSON_EXPORTING
            #define MAT_JSON_DLL [[gnu::visibility("default")]]
        #else
            #define MAT_JSON_DLL
        #endif
    #endif
#else
    #define MAT_JSON_DLL
#endif

namespace matjson {

    enum class Type {
        Object,
        Array,
        String,
        Number,
        Bool,
        Null,
    };

    class ValueImpl;

    class Value;
    class ValueIterator;

    using Array = std::vector<Value>;
    // TODO: change these?
    using ParseError = std::string_view;
    using GenericError = std::string_view;

    static constexpr int NO_INDENTATION = 0;
    static constexpr int TAB_INDENTATION = -1;

    /// Specialize this class and implement the following methods (not all required)
    /// static Result<T, _> fromJson(matjson::Value const&);
    /// static matjson::Value toJson(T const&);
    template <class T>
    struct Serialize;

    template <class T>
    concept CanSerialize = requires(matjson::Value const& value, T t) {
        { Serialize<std::remove_cvref_t<T>>::fromJson(value) };
        { Serialize<std::remove_cvref_t<T>>::toJson(t) };
    };

    class MAT_JSON_DLL Value {
        std::unique_ptr<ValueImpl> m_impl;
        friend ValueImpl;
        Value(std::unique_ptr<ValueImpl>);

    public:
        /// Defaults to a JSON object, for convenience
        Value();
        Value(std::string_view value);
        Value(char const* value);
        Value(Array value);
        Value(std::nullptr_t);
        Value(double value);
        Value(bool value);
        explicit Value(std::intmax_t value);
        explicit Value(std::uintmax_t value);

        template <class T>
            requires std::is_integral_v<T> && std::is_signed_v<T>
        Value(T value) : Value(static_cast<std::intmax_t>(value)) {}

        template <class T>
            requires std::is_integral_v<T> && std::is_unsigned_v<T>
        Value(T value) : Value(static_cast<std::uintmax_t>(value)) {}

        template <CanSerialize T>
        Value(T&& value) :
            Value(Serialize<std::remove_cvref_t<T>>::toJson(std::forward<T>(value))) {}

        template <class T>
        // Prevents implicit conversion from pointer to bool
        Value(T*) = delete;

        Value(Value const&);
        Value(Value&&);
        ~Value();

        Value& operator=(Value);

        /// Parses JSON from a string
        /// @param source The JSON string to parse
        /// @return The parsed JSON value or an error
        static geode::Result<Value, ParseError> parse(std::string_view source);

        /// Parses JSON from an input stream
        /// @param source Stream to parse
        /// @return The parsed JSON value or an error
        static geode::Result<Value, ParseError> parse(std::istream& source);

        /// Returns the value associated with the given key
        /// @param key Object key
        /// @return The value associated with the key, or an error if it does not exist.
        geode::Result<Value&, GenericError> get(std::string_view key);

        /// Returns the value associated with the given key
        /// @param key Object key
        /// @return The value associated with the key, or an error if it does not exist.
        geode::Result<Value const&, GenericError> get(std::string_view key) const;

        /// Returns the value associated with the given index
        /// @param index Array index
        /// @return The value associated with the index, or an error if the index is out of bounds.
        geode::Result<Value&, GenericError> get(size_t index);

        /// Returns the value associated with the given index
        /// @param index Array index
        /// @return The value associated with the index, or an error if the index is out of bounds.
        geode::Result<Value const&, GenericError> get(size_t index) const;

        /// Returns the value associated with the given key
        /// @param key Object key
        /// @return The value associated with the key
        /// @note If the key does not exist, it is inserted into the object.
        ///       And if this is not an object, returns a null value
        Value& operator[](std::string_view key);

        /// Returns the value associated with the given key
        /// @param key Object key
        /// @return The value associated with the key
        /// @note If the key does not exist, or this is not an object,
        ///       returns a null value
        Value const& operator[](std::string_view key) const;

        /// Returns the value associated with the given index
        /// @param index Array index
        /// @return The value associated with the index
        /// @note If the index is out of bounds, or this is not an array,
        ///       returns a null value
        Value& operator[](size_t index);

        /// Returns the value associated with the given index
        /// @param index Array index
        /// @return The value associated with the index
        /// @note If the index is out of bounds, or this is not an array,
        ///       returns a null value
        Value const& operator[](size_t index) const;

        /// Sets the value associated with the given key
        /// @param key Object key
        /// @param value The value to set
        /// @note If this is not an object, nothing happens
        void set(std::string_view key, Value value);

        /// Removes the value associated with the given key
        /// @param key Object key
        /// @return true if the key was removed, false otherwise
        /// @note If this is not an object, nothing happens and returns false
        bool erase(std::string_view key);

        /// Checks if the key exists in the object
        /// @param key Object key
        /// @return true if the key exists, false otherwise
        /// @note If this is not an object, returns false
        bool contains(std::string_view key) const;

        bool operator==(Value const&) const;
        bool operator<(Value const&) const;
        bool operator>(Value const&) const;

        /// Dumps the JSON value to a string, with a given indentation.
        /// If the given indentation is matjson::NO_INDENTATION, the json is compacted.
        /// If the given indentation is matjson::TAB_INDENTATION, the json is indented with tabs.
        /// @param indentationSize The number of spaces to use for indentation
        /// @return The JSON string or an error
        geode::Result<std::string, GenericError> dump(int indentationSize = 4) const;

        Type type() const;

        std::vector<Value>::iterator begin();
        std::vector<Value>::iterator end();

        std::vector<Value>::const_iterator begin() const;
        std::vector<Value>::const_iterator end() const;

        bool isNull() const {
            return this->type() == Type::Null;
        }

        bool isString() const {
            return this->type() == Type::String;
        }

        bool isNumber() const {
            return this->type() == Type::Number;
        }

        bool isBool() const {
            return this->type() == Type::Bool;
        }

        bool isArray() const {
            return this->type() == Type::Array;
        }

        bool isObject() const {
            return this->type() == Type::Object;
        }

        geode::Result<bool, GenericError> asBool() const;
        geode::Result<std::string, GenericError> asString() const;
        geode::Result<std::intmax_t, GenericError> asInt() const;
        geode::Result<std::uintmax_t, GenericError> asUInt() const;
        geode::Result<double, GenericError> asDouble() const;

        std::optional<std::string> getKey() const;

        /// Converts the JSON value to a given type, possibly serializing to
        /// a custom type via the Serialize specialization
        /// @tparam T The type to convert to
        /// @return The converted value or an error
        template <class T>
        decltype(auto) as() const {
            if constexpr (std::is_same_v<T, bool>) {
                return this->asBool();
            }
            else if constexpr (std::is_integral_v<T>) {
                if constexpr (std::is_signed_v<T>) {
                    return this->asInt().map([](std::intmax_t v) -> T {
                        return static_cast<T>(v);
                    });
                }
                else {
                    return this->asUInt().map([](std::uintmax_t v) -> T {
                        return static_cast<T>(v);
                    });
                }
            }
            else if constexpr (std::is_floating_point_v<T>) {
                return this->asDouble().map([](double v) -> T {
                    return static_cast<T>(v);
                });
            }
            else if constexpr (CanSerialize<T>) {
                return Serialize<std::remove_cvref_t<T>>::fromJson(*this);
            }
            else if constexpr (std::is_constructible_v<std::string, T>) {
                return this->asString();
            }
            else if constexpr (std::is_same_v<T, Value>) {
                return *this;
            }
            else {
                static_assert(!std::is_same_v<T, T>, "no conversion found from matjson::Value to T");
            }
        }
    };

    /// Parses JSON from a string
    /// @param source The JSON string to parse
    /// @return The parsed JSON value or an error
    /// @note Shorthand for Value::parse
    inline geode::Result<Value, ParseError> parse(std::string_view source) {
        return Value::parse(source);
    }

    /// Parses JSON from an input stream
    /// @param source Stream to parse
    /// @return The parsed JSON value or an error
    /// @note Shorthand for Value::parse
    inline geode::Result<Value, ParseError> parse(std::istream& stream) {
        return Value::parse(stream);
    }

    // This is used for destructuring the value, useful for range for loops:
    // > for (auto const& [key, value] : object) { ... }
    template <size_t Index, class T>
        requires requires { std::is_same_v<std::decay_t<T>, Value>; }
    decltype(auto) get(T&& value) {
        if constexpr (Index == 0) {
            auto const opt = value.getKey();
            if (!opt) return std::string();
            return std::string(*opt);
        }
        else if constexpr (Index == 1) {
            return std::forward<T>(value);
        }
    }

    // For fmtlib, lol
    inline std::string format_as(matjson::Value const& value) {
        // let the exception through
        return value.dump().unwrap();
    }
}

// allow destructuring
template <>
struct std::tuple_size<matjson::Value> : std::integral_constant<size_t, 2> {};

template <>
struct std::tuple_element<0, matjson::Value> {
    using type = std::string;
};

template <>
struct std::tuple_element<1, matjson::Value> {
    using type = matjson::Value;
};
