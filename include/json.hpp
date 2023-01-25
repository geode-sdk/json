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

	using Array = std::vector<Value>;

	class Object;

	class Value {
		std::unique_ptr<ValueImpl> m_impl;
		friend ValueImpl;
		Value(std::unique_ptr<ValueImpl>);
	public:
		Value();
		Value(std::string value);
		Value(const char* value);
		Value(int value);
		Value(double value);
		Value(bool value);
		Value(Object value);
		Value(Array value);
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

		const Object& to_object() const;
		Object& to_object();
		
		const Array& to_array() const;
		Array& to_array();

		bool operator==(const Value&) const;
	};

	class Object  {
		using value_type = std::pair<std::string, Value>;
		using iterator = typename std::vector<value_type>::iterator;
		using const_iterator = typename std::vector<value_type>::const_iterator;
		std::vector<value_type> m_data;
	public:
		Object() = default;
		Object(const Object&);
		Object(Object&&);

		template <class It>
		Object(It first, It last) : m_data(first, last) {}

		Object(std::initializer_list<value_type> init);

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

		bool operator==(const Object& other) const;
	};
}