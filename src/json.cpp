#include <json.hpp>
#include <variant>
#include <stdexcept>

using namespace json;

class json::ValueImpl {
	Type m_type;
	std::variant<std::monostate, std::string, double, bool, Array, Object> m_value;
public:
	template <class T>
	ValueImpl(Type type, T value) : m_type(type), m_value(value) {}
	ValueImpl(const ValueImpl&) = default;
	Type type() const { return m_type; }

	static Value to_value(std::unique_ptr<ValueImpl> impl) {
		return Value(std::move(impl));
	}

	bool to_bool() const { return std::get<bool>(m_value); }
	std::string to_string() const { return std::get<std::string>(m_value); }
	double to_double() const { return std::get<double>(m_value); }
	Object& to_object() { return std::get<Object>(m_value); }
	Array& to_array() { return std::get<Array>(m_value); }
};

char take_one(std::string_view& string) {
	if (string.empty()) throw std::runtime_error("eof");
	auto c = string.front();
	string = string.substr(1);
	return c;
}

char peek(std::string_view& string) {
	if (string.empty()) throw std::runtime_error("eof");
	return string.front();
}

bool is_ws(char c) {
	return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

void skip_ws(std::string_view& string) {
	while (!string.empty() && is_ws(peek(string))) {
		take_one(string);
	}
}

std::string_view take_n(std::string_view& string, int n) {
	if (string.size() < n) throw std::runtime_error("eof");
	const auto sub = string.substr(0, n);
	string = string.substr(n);
	return sub;
}

using ValuePtr = std::unique_ptr<ValueImpl>;

using namespace std::literals::string_view_literals;
using namespace std::literals::string_literals;


ValuePtr parse_constant(std::string_view& source) {
	switch (peek(source)) {
		case 't':
			if (take_n(source, 4) != "true"sv) break;
			return std::make_unique<ValueImpl>(Type::Boolean, true);
		case 'f':
			if (take_n(source, 5) != "false"sv) break;
			return std::make_unique<ValueImpl>(Type::Boolean, false);
		case 'n':
			if (take_n(source, 4) != "null"sv) break;
			return std::make_unique<ValueImpl>(Type::Null, std::monostate{});
		default: break;
	}
	throw std::runtime_error("unknown value");
}

void encode_utf8(std::string& str, int32_t code_point) {
	if (code_point < 0x80) {
		str.push_back(static_cast<char>(code_point));
	} else if (code_point < 0x800) {
		str.push_back(static_cast<char>(0b11000000 | ((code_point & 0b11111'000000) >> 6)));
		str.push_back(static_cast<char>(0b10000000 |  (code_point & 0b00000'111111)));
	} else if (code_point < 0x10000) {
		str.push_back(static_cast<char>(0b11100000 | ((code_point & 0b1111'000000'000000) >> 12)));
		str.push_back(static_cast<char>(0b10000000 | ((code_point & 0b0000'111111'000000) >> 6)));
		str.push_back(static_cast<char>(0b10000000 |  (code_point & 0b0000'000000'111111)));
	} else {
		str.push_back(static_cast<char>(0b11110000 | ((code_point & 0b111'000000'000000'000000) >> 18)));
		str.push_back(static_cast<char>(0b10000000 | ((code_point & 0b000'111111'000000'000000) >> 12)));
		str.push_back(static_cast<char>(0b10000000 | ((code_point & 0b000'000000'111111'000000) >> 6)));
		str.push_back(static_cast<char>(0b10000000 |  (code_point & 0b000'000000'000000'111111)));
	}
}

std::string parse_string(std::string_view& source) {
	take_one(source);
	std::string str;
	while (peek(source) != '"') {
		char c = take_one(source);
		if (c < 0x20) throw std::runtime_error("invalid string");
		// FIXME: standard should also ignore > 0x10FFFF, however that would require decoding utf-8
		if (c == '\\') {
			switch (take_one(source)) {
				case '"': str.push_back('"'); break;
				case '\\': str.push_back('\\'); break;
				case '/': str.push_back('/'); break;
				case 'b': str.push_back('\b'); break;
				case 'f': str.push_back('\f'); break;
				case 'n': str.push_back('\n'); break;
				case 'r': str.push_back('\r'); break;
				case 't': str.push_back('\t'); break;
				case 'u': {
					const auto take_hex = [&] {
						char c = take_one(source);
						if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
						else if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(c - 'a' + 10);
						else if (c >= 'A' && c <= 'F') return static_cast<uint32_t>(c - 'A' + 10);
						throw std::runtime_error("invalid hex");
					};
					int32_t value = 0;
					value |= take_hex() << 12;
					value |= take_hex() << 8;
					value |= take_hex() << 4;
					value |= take_hex();
					encode_utf8(str, value);
				} break;
				default: throw std::runtime_error("invalid backslash escape");
			}
		} else {
			str.push_back(c);
		}
	}
	take_one(source);
	return str;
}

ValuePtr parse_number(std::string_view& source) {
	size_t size = 0;
	auto start = source;
	if (peek(source) == '-') {
		take_one(source);
		++size;
	}
	const auto take_digits = [&] {
		bool once = false;
		while (true) {
			char c = peek(source);
			if (c >= '0' && c <= '9') {
				once = true;
				take_one(source);
				++size;
			} else {
				break;
			}
		}
		if (!once) throw std::runtime_error("expected digits");
	};
	if (peek(source) == '0') {
		take_one(source);
		++size;
	} else {
		take_digits();
	}
	if (peek(source) == '.') {
		take_one(source);
		++size;
		take_digits();
	}
	if (peek(source) == 'e' || peek(source) == 'E') {
		take_one(source);
		++size;
		if (peek(source) == '-' || peek(source) == '+') {
			take_one(source);
			++size;
		}
		take_digits();
	}
	const std::string str(start.substr(0, size));
	return std::make_unique<ValueImpl>(Type::Number, std::stod(str));
}

ValuePtr parse_element(std::string_view& source);

ValuePtr parse_object(std::string_view& source) {
	take_one(source);
	skip_ws(source);
	Object object;
	if (peek(source) != '}') {
		while (true) {
			skip_ws(source);
			auto key = parse_string(source);
			skip_ws(source);
			
			if (take_one(source) != ':') throw std::runtime_error("missing colon");
			
			auto value = parse_element(source);
			object.insert({key, ValueImpl::to_value(std::move(value))});

			char c = peek(source);
			if (c == ',') {
				take_one(source);
			} else if (c == '}') {
				break;
			} else {
				throw std::runtime_error("expected member");
			}
		}
	}
	take_one(source);
	return std::make_unique<ValueImpl>(Type::Object, object);
}

ValuePtr parse_array(std::string_view& source) {
	take_one(source);
	skip_ws(source);
	Array array;
	if (peek(source) != ']') {
		while (true) {
			array.push_back(ValueImpl::to_value(parse_element(source)));

			char c = peek(source);
			if (c == ',') {
				take_one(source);
			} else if (c == ']') {
				break;
			} else {
				throw std::runtime_error("expected value");
			}
		}
	}
	take_one(source);
	return std::make_unique<ValueImpl>(Type::Array, array);
}

ValuePtr parse_value(std::string_view& source) {
	switch (peek(source)) {
		case 't':
		case 'f':
		case 'n': return parse_constant(source);
		case '"': return std::make_unique<ValueImpl>(Type::String, parse_string(source));
		case '{': return parse_object(source);
		case '[': return parse_array(source);
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
		case '9': return parse_number(source);
		default: throw std::runtime_error("invalid value");
	}
}

ValuePtr parse_element(std::string_view& source) {
	skip_ws(source);
	auto value = parse_value(source);
	skip_ws(source);
	return value;
}

ValuePtr parse_json(std::string_view source) {
	auto value = parse_element(source);
	skip_ws(source);
	if (!source.empty()) throw std::runtime_error("expected eof");
	return value;
}

Value::Value() : Value(nullptr) {}

Value::Value(std::string value) {
	m_impl = std::make_unique<ValueImpl>(Type::String, std::move(value));
}

Value::Value(int value) {
	m_impl = std::make_unique<ValueImpl>(Type::Number, static_cast<double>(value));
}

Value::Value(double value) {
	m_impl = std::make_unique<ValueImpl>(Type::Number, value);
}

Value::Value(bool value) {
	m_impl = std::make_unique<ValueImpl>(Type::Boolean, value);
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

Value& Value::operator=(Value value) {
	m_impl.swap(value.m_impl);
	return *this;
}

Value::~Value() {}

Type Value::type() const {
	return m_impl->type();
}

bool Value::is_null() const { return type() == Type::Null; }

// TODO: these will throw a variant access exception, maybe make our own error type?
bool Value::to_bool() const { return m_impl->to_bool(); }
std::string Value::to_string() const { return m_impl->to_string(); }
int Value::to_int() const { return static_cast<int>(m_impl->to_double()); }
double Value::to_double() const { return static_cast<int>(m_impl->to_double()); }

const Object& Value::to_object() const { return m_impl->to_object(); }
Object& Value::to_object() { return m_impl->to_object(); }

const Array& Value::to_array() const { return m_impl->to_array(); }
Array& Value::to_array() { return m_impl->to_array(); }

Value Value::from_str(std::string_view source) {
	return Value(parse_json(source));
}

std::optional<std::reference_wrapper<Value>> Value::try_get(std::string_view key) {
	if (type() != Type::Object) return std::nullopt;
	auto& obj = to_object();
	if (auto it = obj.find(key); it != obj.end()) {
		return it->second;
	} else {
		return std::nullopt;
	}
}

std::optional<std::reference_wrapper<const Value>> Value::try_get(std::string_view key) const {
	if (type() != Type::Object) return std::nullopt;
	const auto& obj = to_object();
	if (auto it = obj.find(key); it != obj.end()) {
		return it->second;
	} else {
		return std::nullopt;
	}
}

std::optional<std::reference_wrapper<Value>> Value::try_get(size_t index) {
	if (type() != Type::Array) return std::nullopt;
	auto& arr = to_array();
	if (index < arr.size()) return arr[index];
	else return std::nullopt;
}

std::optional<std::reference_wrapper<const Value>> Value::try_get(size_t index) const {
	if (type() != Type::Array) return std::nullopt;
	const auto& arr = to_array();
	if (index < arr.size()) return arr[index];
	else return std::nullopt;
}


Value& Value::operator[](std::string_view key) {
	if (type() != Type::Object) throw std::runtime_error("not an object");
	auto& obj = to_object();
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

bool Value::operator==(const Value& other) const {
	if (type() != other.type()) return false;
	switch (type()) {
		case Type::Null: return true;
		case Type::Boolean: return to_bool() == other.to_bool();
		case Type::String: return to_string() == other.to_string();
		case Type::Number: return to_double() == other.to_double();
		case Type::Array: return to_array() == other.to_array();
		case Type::Object: return to_object() == other.to_object();
	}
}

Object::Object(const Object& object) : m_data(object.m_data) {}
Object::Object(Object&& object) : m_data(std::move(object.m_data)) {}
Object::Object(std::initializer_list<value_type> init) : m_data(init) {}

Object::iterator Object::begin() { return m_data.begin(); }
Object::iterator Object::end() { return m_data.end(); }
Object::const_iterator Object::begin() const { return m_data.begin(); }
Object::const_iterator Object::end() const { return m_data.end(); }
Object::const_iterator Object::cbegin() const { return m_data.cbegin(); }
Object::const_iterator Object::cend() const { return m_data.cend(); }

bool Object::operator==(const Object& other) const {
	for (const auto& [key, value] : *this) {
		if (auto it = other.find(key); it != other.end()) {
			if (value != it->second) return false;
		} else {
			return false;
		}
	}
	return true;
}

Object::iterator Object::find(std::string_view key) {
	auto end = this->end();
	for (auto it = this->begin(); it != end; ++it) {
		if (it->first == key) return it;
	}
	return end;
}

Object::const_iterator Object::find(std::string_view key) const {
	auto end = this->cend();
	for (auto it = this->cbegin(); it != end; ++it) {
		if (it->first == key) return it;
	}
	return end;
}

std::pair<Object::iterator, bool> Object::insert(const Object::value_type& value) {
	if (auto it = this->find(value.first); it != this->end()) {
		return {it, false};
	} else {
		m_data.push_back(value);
		return {--m_data.end(), true};
	}
}

size_t Object::count(std::string_view key) const {
	return this->find(key) == this->end() ? 0 : 1;
}

Value& Object::operator[](std::string_view key) {
	if (auto it = this->find(key); it != this->end()) {
		return it->second;
	} else {
		m_data.push_back({std::string(key), Value{}});
		return m_data.back().second;
	}
}