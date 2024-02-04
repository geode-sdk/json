#include <cmath>

Value::Value() : Value(Object{}) {}

Value::Value(const char* str) : Value(std::string(str)) {}

Value::Value(std::string value) {
	m_impl = std::make_unique<ValueImpl>(Type::String, std::move(value));
}

Value::Value(double value) {
	m_impl = std::make_unique<ValueImpl>(Type::Number, value);
}

Value::Value(bool value) {
	m_impl = std::make_unique<ValueImpl>(Type::Bool, value);
}

Value::Value(Object value) {
	m_impl = std::make_unique<ValueImpl>(Type::Object, value);
}

Value::Value(Array value) {
	m_impl = std::make_unique<ValueImpl>(Type::Array, value);
}

Value::Value(std::nullptr_t) {
	m_impl = std::make_unique<ValueImpl>(Type::Null, std::monostate{});
}

Value::Value(std::unique_ptr<ValueImpl> impl) : m_impl(std::move(impl)) {}

Value::Value(const Value& other) {
	m_impl = std::make_unique<ValueImpl>(*other.m_impl.get());
}

Value::Value(Value&& other) {
	m_impl.swap(other.m_impl);
}

Value& Value::operator=(Value value) {
	m_impl.swap(value.m_impl);
	return *this;
}

Value::~Value() {}

Type Value::type() const {
	return m_impl->type();
}

// TODO: these will throw a variant access exception, maybe make our own error type?
bool Value::as_bool() const { return m_impl->as_bool(); }
std::string Value::as_string() const { return m_impl->as_string(); }
int Value::as_int() const { return static_cast<int>(m_impl->as_double()); }
double Value::as_double() const { return m_impl->as_double(); }

const Object& Value::as_object() const { return m_impl->as_object(); }
Object& Value::as_object() { return m_impl->as_object(); }

const Array& Value::as_array() const { return m_impl->as_array(); }
Array& Value::as_array() { return m_impl->as_array(); }

Value Value::from_str(std::string_view source) {
	std::string error;
	auto object = parse_json(source, error);
	if (!object) throw std::runtime_error(error);
	return Value(std::move(object));
}

std::optional<Value> Value::from_str(std::string_view source, std::string& error) noexcept {
	auto object = parse_json(source, error);
	if (object) return Value(std::move(object));
	return std::nullopt;
}

bool Value::contains(std::string_view key) const {
	if (!is_object()) return false;
	return as_object().contains(key);
}

size_t Value::count(std::string_view key) const {
	if (!is_object()) return false;
	return as_object().count(key);
}

std::optional<std::reference_wrapper<Value>> Value::try_get(std::string_view key) {
	if (type() != Type::Object) return std::nullopt;
	auto& obj = as_object();
	if (auto it = obj.find(key); it != obj.end()) {
		return it->second;
	} else {
		return std::nullopt;
	}
}

std::optional<std::reference_wrapper<const Value>> Value::try_get(std::string_view key) const {
	if (type() != Type::Object) return std::nullopt;
	const auto& obj = as_object();
	if (auto it = obj.find(key); it != obj.end()) {
		return it->second;
	} else {
		return std::nullopt;
	}
}

std::optional<std::reference_wrapper<Value>> Value::try_get(size_t index) {
	if (type() != Type::Array) return std::nullopt;
	auto& arr = as_array();
	if (index < arr.size()) return arr[index];
	else return std::nullopt;
}

std::optional<std::reference_wrapper<const Value>> Value::try_get(size_t index) const {
	if (type() != Type::Array) return std::nullopt;
	const auto& arr = as_array();
	if (index < arr.size()) return arr[index];
	else return std::nullopt;
}


Value& Value::operator[](std::string_view key) {
	if (type() != Type::Object) throw std::runtime_error("not an object");
	auto& obj = as_object();
	return obj[key];
}

const Value& Value::operator[](std::string_view key) const {
	return try_get(key).value();
}

Value& Value::operator[](size_t index) {
	return try_get(index).value();
}

const Value& Value::operator[](size_t index) const {
	return try_get(index).value();
}

void Value::set(std::string_view key, Value value) {
	if (type() != Type::Object) throw std::runtime_error("not an object");
	as_object()[key] = value;
}

void Value::erase(std::string_view key) {
	if (type() != Type::Object) throw std::runtime_error("not an object");
	as_object().erase(key);
}

bool Value::try_set(std::string_view key, Value value) noexcept {
	if (type() != Type::Object) return false;
	as_object()[key] = value;
	return true;

}
bool Value::try_erase(std::string_view key) noexcept {
	if (type() != Type::Object) return false;
	as_object().erase(key);
	return true;

}

bool Value::operator==(const Value& other) const {
	if (type() != other.type()) return false;
	switch (type()) {
		case Type::Null: return true;
		case Type::Bool: return as_bool() == other.as_bool();
		case Type::String: return as_string() == other.as_string();
		case Type::Number: return as_double() == other.as_double();
		case Type::Array: return as_array() == other.as_array();
		case Type::Object: return as_object() == other.as_object();
		default: return false;
	}
}

bool Value::operator<(const Value& other) const {
	if (type() < other.type()) {
		return true;
	}
	if (type() != other.type()) return false;
	switch (type()) {
		case Type::Null: return true;
		case Type::Bool: return as_bool() < other.as_bool();
		case Type::String: return as_string() < other.as_string();
		case Type::Number: return as_double() < other.as_double();
		case Type::Array: return as_array() < other.as_array();
		case Type::Object: return as_object() < other.as_object();
		default: return false;
	}
}

bool Value::operator>(const Value& other) const {
	if (type() > other.type()) {
		return true;
	}
	if (type() != other.type()) return false;
	switch (type()) {
		case Type::Null: return false;
		case Type::Bool: return as_bool() > other.as_bool();
		case Type::String: return as_string() > other.as_string();
		case Type::Number: return as_double() > other.as_double();
		case Type::Array: return as_array() > other.as_array();
		case Type::Object: return as_object() > other.as_object();
		default: return false;
	}
}

void dump_impl_string(const std::string& str, std::string& result) {
	result.push_back('"');
	for (auto c : str) {
		switch (c) {
			case '\b': result += "\\b"sv; break;
			case '\f': result += "\\f"sv; break;
			case '\n': result += "\\n"sv; break;
			case '\r': result += "\\r"sv; break;
			case '\t': result += "\\t"sv; break;
			case '"': result += "\\\""sv; break;
			case '\\': result += "\\\\"sv; break;
			default: {
				// TODO: exceptionless dump to make alk happy
				// in the meantime, this is better than creating
				// an invalid json :+1:
				if (c >= 0 && c < 0x20)
					throw std::runtime_error("invalid string");
				result.push_back(c); break;
			}
		}
	}
	result.push_back('"');
}

void dump_impl(const Value& value, std::string& result, int indentation, int depth) {
	switch (value.type()) {
		case Type::Null: {
			result += "null"sv;
		} break;
		case Type::Bool: {
			result += value.as_bool() ? "true"sv : "false"sv;
		} break;
		case Type::String: {
			dump_impl_string(value.as_string(), result);
		} break;
		case Type::Number: {
			auto number = value.as_double();
			// TODO: same thing abt exceptions above
			if (std::isnan(number))
				throw std::runtime_error("number cant be nan");
			if (std::isinf(number))
				throw std::runtime_error("number cant be infinity");
			#ifndef __cpp_lib_to_chars
				std::stringstream stream;
				stream.imbue(std::locale("C"));
				stream << number;
				result += stream.str();
			#else
				std::array<char, 32> buffer;
				auto chars_result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
				if (chars_result.ec == std::errc::value_too_large) {
					// this should never happen, i think
					throw std::runtime_error("failed to convert number to string");
				}
				result += std::string_view(buffer.data(), chars_result.ptr - buffer.data());
			#endif
		} break;
		case Type::Array: {
			result.push_back('[');
			const auto& arr = value.as_array();
			bool first = true;
			for (const auto& value : arr) {
				if (!first) {
					result.push_back(',');
					if (indentation != matjson::NO_INDENTATION)
						result.push_back(' ');
				}
				first = false;
				dump_impl(value, result, indentation, depth);
			}
			result.push_back(']');
		} break;
		case Type::Object: {
			result.push_back('{');
			const auto add_line = [&](int depth) {
				if (indentation != matjson::NO_INDENTATION) {
					result.push_back('\n');
					const char iden = indentation == matjson::TAB_INDENTATION ? '\t' : ' ';
					const size_t size = indentation == matjson::TAB_INDENTATION ? depth : depth * indentation;
					result.reserve(result.size() + size);
					for (size_t i = 0; i < size; ++i) {
						result.push_back(iden);
					}
				}
			};
			add_line(depth + 1);
			const auto& obj = value.as_object();
			bool first = true;
			for (const auto& [key, value] : obj) {
				if (!first) {
					result.push_back(',');
					add_line(depth + 1);
				}
				first = false;

				dump_impl_string(key, result);
				
				result.push_back(':');
				if (indentation != matjson::NO_INDENTATION)
					result.push_back(' ');

				dump_impl(value, result, indentation, depth + 1);
			}
			add_line(depth);
			result.push_back('}');
		} break;
	}
}

std::string Value::dump(int indentation_size) const {
	std::string result;
	dump_impl(*this, result, indentation_size, 0);
	return result;
}

// from boost::hash_combine
static std::size_t hash_combine(std::size_t seed, std::size_t h) noexcept {
	seed ^= h + 0x9e3779b9 + (seed << 6U) + (seed >> 2U);
	return seed;
}

std::size_t std::hash<matjson::Value>::operator()(matjson::Value const& value) const {
	if (value.is_null()) {
		return std::size_t(-1);
	}
	if (value.is_bool()) {
		return value.as_bool();
	}
	if (value.is_number()) {
		return std::hash<double>()(value.as_double());
	}
	if (value.is_string()) {
		return std::hash<std::string>()(value.as_string());
	}

	auto const type_index = static_cast<size_t>(value.type());
	if (value.is_array()) {
		auto const& arr = value.as_array();
		auto seed = hash_combine(type_index, arr.size());
		for (auto const& item : arr) {
			seed = hash_combine(seed, hash<matjson::Value>()(item));
		}
		return seed;
	}

	if (value.is_object()) {
		auto const& obj = value.as_object();
		auto seed = hash_combine(type_index, obj.size());
		for (auto& [key, item] : obj) {
			seed = hash_combine(seed, std::hash<std::string>()(key));
			seed = hash_combine(seed, std::hash<matjson::Value>()(item));
		}
		return seed;
	}

	return 0;
}