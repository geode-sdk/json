#define CONCAT_(left, right) left ## right
#define CONCAT(left, right) CONCAT_(left, right)

#define UNWRAP(...)                                       \
	(__VA_ARGS__);                                        \
	if (!error.empty()) {                                 \
		return {};                                        \
	}


#define VALUE_UNWRAP(into, ...)                           \
	auto CONCAT(optional_res_, __LINE__) = (__VA_ARGS__); \
	if (!error.empty()) {                                 \
		return {};                                        \
	}                                                     \
	into = std::move(CONCAT(optional_res_, __LINE__));


char take_one(std::string_view& string, std::string& error) noexcept {
	if (string.empty()) {
		error = "eof";
		return '\0';
	}
	auto c = string.front();
	string = string.substr(1);
	return c;
}

char peek(std::string_view& string, std::string& error) noexcept {
	if (string.empty()) {
		error = "eof";
		return '\0';
	}
	return string.front();
}

bool is_ws(char c) {
	return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

void skip_ws(std::string_view& string, std::string& error) noexcept {
	while (!string.empty() && is_ws(peek(string, error)) && error.empty()) {
		take_one(string, error);
		if (!error.empty()) return;
	}
}

std::string_view take_n(std::string_view& string, size_t n, std::string& error) noexcept {
	if (string.size() < n) {
		error = "eof";
		return {};
	}
	const auto sub = string.substr(0, n);
	string = string.substr(n);
	return sub;
}

using ValuePtr = std::unique_ptr<ValueImpl>;


ValuePtr parse_constant(std::string_view& source, std::string& error) noexcept {
	VALUE_UNWRAP(char p, peek(source, error));
	std::string_view s;
	switch (p) {
		case 't': {
			VALUE_UNWRAP(s, take_n(source, 4, error));
			if (s != "true"sv) break;
			return std::make_unique<ValueImpl>(Type::Bool, true);
		}
		case 'f': {
			VALUE_UNWRAP(s, take_n(source, 5, error));
			if (s != "false"sv) break;
			return std::make_unique<ValueImpl>(Type::Bool, false);
		}
		case 'n': {
			VALUE_UNWRAP(s, take_n(source, 4, error));
			if (s != "null"sv) break;
			return std::make_unique<ValueImpl>(Type::Null, std::monostate{});
		}
		default: break;
	}
	error = "invalid constant";
	return std::unique_ptr<ValueImpl>{};
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

std::string parse_string(std::string_view& source, std::string& error) noexcept {
	UNWRAP(take_one(source, error));
	std::string str;
	VALUE_UNWRAP(char p, peek(source, error));
	while (p != '"') {
		VALUE_UNWRAP(char c, take_one(source, error));
		// char is signed, so utf8 high bit bytes will be interpreted as negative,
		// could switch to unsigned char however just checking for c < 0 is easier
		if (c >= 0 && c < 0x20) {
			error = "invalid string";
			return {};
		}
		// FIXME: standard should also ignore > 0x10FFFF, however that would require decoding utf-8
		if (c == '\\') {
			VALUE_UNWRAP(char s, take_one(source, error));
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
					const auto take_hex = [&] {
						char c = take_one(source, error);
						if (!error.empty()) return 0u;
						if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
						else if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(c - 'a' + 10);
						else if (c >= 'A' && c <= 'F') return static_cast<uint32_t>(c - 'A' + 10);
						error = "invalid hex";
						return 0u;
					};
					int32_t value = 0;
					value |= take_hex() << 12;
					value |= take_hex() << 8;
					value |= take_hex() << 4;
					value |= take_hex();
					if (!error.empty()) return {};
					encode_utf8(str, value);
				} break;
				default: 
					error = "invalid backslash escape";
					return {};
			}
		} else {
			str.push_back(c);
		}
		VALUE_UNWRAP(p, peek(source, error));
	}
	UNWRAP(take_one(source, error));
	return str;
}

ValuePtr parse_number(std::string_view& source, std::string& error) noexcept {
	size_t size = 0;
	auto start = source;
	VALUE_UNWRAP(char p, peek(source, error));
	if (p == '-') {
		UNWRAP(take_one(source, error));
		++size;
	}
	const auto take_digits = [&] {
		bool once = false;
		while (!source.empty()) {
			char c = peek(source, error);
			if (!error.empty()) return;
			if (c >= '0' && c <= '9') {
				once = true;
				take_one(source, error);
				if (!error.empty()) return;
				++size;
			} else {
				break;
			}
		}
		if (!once) {
			error = "expected digits";
			return;
		}
	};
	VALUE_UNWRAP(p, peek(source, error));
	if (p == '0') {
		UNWRAP(take_one(source, error));
		++size;
	} else {
		UNWRAP(take_digits());
	}
	// these are optional!
	if (!source.empty()) {
		// fraction
		VALUE_UNWRAP(p, peek(source, error));
		if (p == '.') {
			UNWRAP(take_one(source, error));
			++size;
			UNWRAP(take_digits());
		}
	}
	if (!source.empty()) {
		// exponent
		VALUE_UNWRAP(p, peek(source, error));
		if (p == 'e' || p == 'E') {
			UNWRAP(take_one(source, error));
			++size;
			VALUE_UNWRAP(p, peek(source, error));
			if (p == '-' || p == '+') {
				UNWRAP(take_one(source, error));
				++size;
			}
			UNWRAP(take_digits());
		}
	}
	#ifndef __cpp_lib_to_chars
		const std::string str(start.substr(0, size));
		// FIXME: std::stod is locale specific, might break on some machines
		return std::make_unique<ValueImpl>(Type::Number, std::stod(str));
	#else
		double value;
		if (auto result = std::from_chars(start.data(), start.data() + size, value); result.ec != std::errc()) {
			error = "failed to parse number";
			return {};
		}
		return std::make_unique<ValueImpl>(Type::Number, value);
	#endif
}

ValuePtr parse_element(std::string_view& source, std::string& error) noexcept;

ValuePtr parse_object(std::string_view& source, std::string& error) noexcept {
	UNWRAP(take_one(source, error));
	UNWRAP(skip_ws(source, error));
	Object object;
	VALUE_UNWRAP(char p, peek(source, error));
	if (p != '}') {
		while (true) {
			UNWRAP(skip_ws(source, error));
			VALUE_UNWRAP(auto key, parse_string(source, error));
			UNWRAP(skip_ws(source, error));
			
			VALUE_UNWRAP(char s, take_one(source, error));
			if (s != ':') {
				error = "expected colon";
				return {};
			}
			
			VALUE_UNWRAP(auto value, parse_element(source, error));
			object.insert({key, ValueImpl::as_value(std::move(value))});

			VALUE_UNWRAP(char c, peek(source, error));
			if (c == ',') {
				UNWRAP(take_one(source, error));
			} else if (c == '}') {
				break;
			} else {
				error = "expected member";
				return {};
			}
		}
	}
	UNWRAP(take_one(source, error));
	return std::make_unique<ValueImpl>(Type::Object, object);
}

ValuePtr parse_array(std::string_view& source, std::string& error) noexcept {
	UNWRAP(take_one(source, error));
	UNWRAP(skip_ws(source, error));
	Array array;
	VALUE_UNWRAP(char p, peek(source, error));
	if (p != ']') {
		while (true) {
			VALUE_UNWRAP(auto element, parse_element(source, error));
			array.push_back(ValueImpl::as_value(std::move(element)));

			VALUE_UNWRAP(char c, peek(source, error));
			if (c == ',') {
				UNWRAP(take_one(source, error));
			} else if (c == ']') {
				break;
			} else {
				error = "expected value";
				return {};
			}
		}
	}
	UNWRAP(take_one(source, error));
	return std::make_unique<ValueImpl>(Type::Array, array);
}

ValuePtr parse_value(std::string_view& source, std::string& error) noexcept {
	VALUE_UNWRAP(char p, peek(source, error));
	switch (p) {
		case 't':
		case 'f':
		case 'n': return parse_constant(source, error);
		case '"': {
			VALUE_UNWRAP(auto value, parse_string(source, error));
			return std::make_unique<ValueImpl>(Type::String, value);
		}
		case '{': return parse_object(source, error);
		case '[': return parse_array(source, error);
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
		case '9': return parse_number(source, error);
		default:
			error = "invalid value";
			return {};
	}
}

ValuePtr parse_element(std::string_view& source, std::string& error) noexcept {
	UNWRAP(skip_ws(source, error));
	VALUE_UNWRAP(auto value, parse_value(source, error));
	UNWRAP(skip_ws(source, error));
	return value;
}

ValuePtr parse_json(std::string_view source, std::string& error) noexcept {
	auto value = parse_element(source, error);
	// dont replace an existing error, since it probably left some
	// stuff in the stream anyways
	if (!source.empty() && error.empty()) {
		error = "expected eof";
		return std::unique_ptr<ValueImpl>{};
	}
	return value;
}
