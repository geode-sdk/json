#pragma once

#include <string_view>
#include <optional>
#include <memory>
#include <vector>
#include <string>

namespace json {
    enum class Type {
        Object,
        Array,
        String,
        Number,
        Boolean,
        Null,
    };

    class ValueImpl;

    class Value;

    using ValueArray = std::vector<Value>;

    class ValueObject;

    class Value {
        std::unique_ptr<ValueImpl> m_impl;
        friend ValueImpl;
        Value(std::unique_ptr<ValueImpl>);
    public:
        Value();
        Value(std::string value);
        Value(int value);
        Value(double value);
        Value(bool value);
        Value(ValueObject value);
        Value(ValueArray value);
        Value(std::nullptr_t);

        Value(const Value&);
        ~Value();

        Value& operator=(Value);

        template <class T>
        // Prevents implicit conversion from pointer to bool
        Value(T*) = delete;

        static Value from_str(std::string_view source);

        std::optional<std::reference_wrapper<Value>> try_get(std::string_view key);
        std::optional<std::reference_wrapper<const Value>> try_get(std::string_view key) const;
        std::optional<std::reference_wrapper<Value>> try_get(size_t index);
        std::optional<std::reference_wrapper<const Value>> try_get(size_t index) const;

        Value& operator[](std::string_view key);
        const Value& operator[](std::string_view key) const;
        Value& operator[](size_t index);
        const Value& operator[](size_t index) const;

        void set(std::string_view key, Value value);

        Type type() const;

        bool to_bool() const;
        std::string to_string() const;
        int to_int() const;
        double to_double() const;
        bool is_null() const;

        const ValueObject& to_object() const;
        ValueObject& to_object();
        
        const ValueArray& to_array() const;
        ValueArray& to_array();

        bool operator==(const Value&) const;

        void debug() const;
    };

    class ValueObject  {
        using value_type = std::pair<std::string, Value>;
        using iterator = typename std::vector<value_type>::iterator;
        using const_iterator = typename std::vector<value_type>::const_iterator;
        std::vector<value_type> m_data;
    public:
        ValueObject() = default;
        ValueObject(const ValueObject&);
        ValueObject(ValueObject&&);

        template <class It>
        ValueObject(It first, It last) : m_data(first, last) {}

        ValueObject(std::initializer_list<value_type> init);

        size_t size() const { return m_data.size(); }
        bool empty() const { return m_data.empty(); }

        Value& operator[](std::string_view key);

        iterator begin();
        iterator end();

        const_iterator begin() const;
        const_iterator end() const;

        const_iterator cbegin() const;
        const_iterator cend() const;

        iterator find(std::string_view key);
        const_iterator find(std::string_view key) const;

        std::pair<iterator, bool> insert(const value_type& value);
        size_t count(std::string_view key) const;

        bool operator==(const ValueObject& other) const;
    };
}