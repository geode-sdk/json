#include <variant>

class matjson::ValueImpl {
	Type m_type;
	std::variant<std::monostate, std::string, double, bool, Array, Object, intmax_t, uintmax_t> m_value;
public:
	template <class T>
	ValueImpl(Type type, T value) : m_type(type), m_value(value) {}
	ValueImpl(const ValueImpl&) = default;
	Type type() const { return m_type; }

	static Value as_value(std::unique_ptr<ValueImpl> impl) {
		return Value(std::move(impl));
	}

	bool as_bool() const { return std::get<bool>(m_value); }
	std::string as_string() const { return std::get<std::string>(m_value); }
	double as_double() const { return std::get<double>(m_value); }
	Object& as_object() { return std::get<Object>(m_value); }
	Array& as_array() { return std::get<Array>(m_value); }
	const Object& as_object() const { return std::get<Object>(m_value); }
	const Array& as_array() const { return std::get<Array>(m_value); }
	intmax_t as_integer() const { return std::get<intmax_t>(m_value); }
	uintmax_t as_uinteger() const { return std::get<uintmax_t>(m_value); }

	template <class Op>
	bool operatorImpl(const ValueImpl& other, Op&& operation, bool null, bool fall) const {
		switch (m_type) {
			case Type::Null: return null;
			case Type::Bool: return operation(as_bool(), other.as_bool());
			case Type::String: return operation(as_string(), other.as_string());
			case Type::Number: {
				switch (m_value.index()) {
					case 2: switch (other.m_value.index()) {
						case 2: return operation(as_double(), other.as_double());
						case 6: return operation(as_double(), other.as_integer());
						case 7: return operation(as_double(), other.as_uinteger());
						default: return fall;
					}
					case 6: switch (other.m_value.index()) {
						case 2: return operation(as_integer(), other.as_double());
						case 6: return operation(as_integer(), other.as_integer());
						case 7: return operation(as_integer(), other.as_uinteger());
						default: return fall;
					}
					case 7: switch (other.m_value.index()) {
						case 2: return operation(as_uinteger(), other.as_double());
						case 6: return operation(as_uinteger(), other.as_integer());
						case 7: return operation(as_uinteger(), other.as_uinteger());
						default: return fall;
					}
					default: return fall;
				}
			}
			case Type::Array: return operation(as_array(), other.as_array());
			case Type::Object: return operation(as_object(), other.as_object());
			default: return fall;
		}
	}

	bool operator==(const ValueImpl& other) const {
		if (m_type != other.m_type) return false;
		return operatorImpl(other, std::equal_to<>{}, true, false);
	}

	bool operator<(const ValueImpl& other) const {
		if (m_type < other.m_type) return true;
		if (m_type > other.m_type) return false;
		return operatorImpl(other, std::less<>{}, true, false);
	}

	bool operator>(const ValueImpl& other) const {
		if (m_type > other.m_type) return true;
		if (m_type < other.m_type) return false;
		return operatorImpl(other, std::greater<>{}, false, false);
	}

	bool is_double() const {
		return m_value.index() == 2;
	}

	bool is_integer() const {
		return m_value.index() == 6;
	}

	bool is_uinteger() const {
		return m_value.index() == 7;
	}

	double as_double_casted() const {
		switch (m_value.index()) {
			case 6: return static_cast<double>(as_integer());
			case 7: return static_cast<double>(as_uinteger());
			default: return as_double();
		}
	}

	intmax_t as_integer_casted() const {
		switch (m_value.index()) {
			case 2: return static_cast<intmax_t>(as_double());
			case 7: return static_cast<intmax_t>(as_uinteger());
			default: return as_integer();
		}
	}

	uintmax_t as_uinteger_casted() const {
		switch (m_value.index()) {
			case 2: return static_cast<uintmax_t>(as_double());
			case 6: return static_cast<uintmax_t>(as_integer());
			default: return as_uinteger();
		}
	}
};
