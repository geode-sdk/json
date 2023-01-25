#include <json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

std::string read_file(std::string filepath) {
	std::ifstream is { filepath, std::ios::binary };
	std::stringstream ss;
	ss << is.rdbuf();
	is.close();
	return std::move(ss).str();
}

void debug(const json::Value& value) {
	switch (value.type()) {
		case json::Type::String: {
			std::cout << '"' << value.to_string() << '"';
		} break;
		case json::Type::Number: {
			std::cout << value.to_double();
		} break;
		case json::Type::Boolean: {
			std::cout << std::boolalpha << value.to_bool();
		} break;
		case json::Type::Null: {
			std::cout << "null";
		} break;
		case json::Type::Object: {
			std::cout << '{';
			const auto& obj = value.to_object();
			for (const auto& [key, value] : obj) {
				std::cout << '"' << key << "\": ";
				debug(value);
				std::cout << ", ";
			}
			std::cout << '}';
		} break;
		case json::Type::Array: {
			std::cout << '[';
			const auto& obj = value.to_array();
			for (const auto& value : obj) {
				debug(value);
				std::cout << ", ";
			}
			std::cout << ']';
		} break;
		default: break;
	}
}

int main() {
	json::Value foo = json::ValueObject {
		{"hello", 69}
	};

	debug(foo); std::cout << std::endl;

	foo["world"] = 31;

	debug(foo); std::cout << std::endl;

	json::Value bar;

	bar = foo;

	bar["hello"] = 10;

	debug(foo); std::cout << std::endl;
	debug(bar); std::cout << std::endl;

	{
		json::Value obj1 = json::ValueObject {};
		json::Value obj2 = json::ValueObject {};

		obj1["hello"] = 10;
		obj1["world"] = 20;

		obj2["world"] = 20;
		obj2["hello"] = 10;

		debug(obj1); std::cout << std::endl;
		debug(obj2); std::cout << std::endl;

		std::cout << std::boolalpha << "equal? " << (obj1 == obj2) << std::endl;
	}
}