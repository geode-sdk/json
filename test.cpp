#include <json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cassert>
#include <serialize.hpp>

std::string read_file(std::string filepath) {
	std::ifstream is { filepath, std::ios::binary };
	std::stringstream ss;
	ss << is.rdbuf();
	is.close();
	return std::move(ss).str();
}

template <class S>
void println(S str) {
	std::cout << str << std::endl;
}

void debug(const json::Value& value) {
	switch (value.type()) {
		case json::Type::String: {
			std::cout << '"' << value.as_string() << '"';
		} break;
		case json::Type::Number: {
			std::cout << value.as_double();
		} break;
		case json::Type::Bool: {
			std::cout << std::boolalpha << value.as_bool();
		} break;
		case json::Type::Null: {
			std::cout << "null";
		} break;
		case json::Type::Object: {
			std::cout << '{';
			const auto& obj = value.as_object();
			for (const auto& [key, value] : obj) {
				std::cout << '"' << key << "\": ";
				debug(value);
				std::cout << ", ";
			}
			std::cout << '}';
		} break;
		case json::Type::Array: {
			std::cout << '[';
			const auto& obj = value.as_array();
			for (const auto& value : obj) {
				debug(value);
				std::cout << ", ";
			}
			std::cout << ']';
		} break;
		default: break;
	}
}

struct CoolStruct {
	std::string name;
	int value;
};

template <>
struct json::Serialize<CoolStruct> {
	static json::Value to_json(const CoolStruct& cool) {
		return json::Object {
			{ "value", cool.value },
			{ "name", cool.name }
		};
	}
	static CoolStruct from_json(const json::Value& value) {
		return CoolStruct {
			.name = value["name"].as_string(),
			.value = value["value"].as_int()
		};
	}
};

template <class T>
requires std::is_enum_v<T>
struct json::Serialize<T> {
	static json::Value to_json(const T& value) {
		return json::Value(static_cast<int>(value));
	}
};

enum class Hooray {
	Hooray
};

int main() {
	json::Value foo = json::Object {
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
		json::Value obj1 = json::Object {};
		json::Value obj2 = json::Object {};

		obj1["hello"] = 10;
		obj1["world"] = 20;

		obj2["world"] = 20;
		obj2["hello"] = 10;

		debug(obj1); std::cout << std::endl;
		debug(obj2); std::cout << std::endl;

		std::cout << std::boolalpha << "equal? " << (obj1 == obj2) << std::endl;
	}

	{
		json::Value obj(
			{ { "hello", "world!"}, {"sux", 123} }
		);
		debug(obj); std::cout << std::endl;
	}

	{
		auto obj = json::Value::from_str(R"(
{
    "geode": "@PROJECT_VERSION@",
    "id": "geode.loader",
    "version": "@PROJECT_VERSION@@PROJECT_VERSION_SUFFIX@",
    "name": "Geode",
    "developer": "Geode Team",
    "description": "The Geode mod loader",
    "repository": "https://github.com/geode-sdk/geode",
    "resources": {
        "fonts": {
            "mdFont": {
                "path": "fonts/Ubuntu-Regular.ttf",
                "size": 80
            },
            "mdFontB": {
                "path": "fonts/Ubuntu-Bold.ttf",
                "size": 80
            },
            "mdFontI": {
                "path": "fonts/Ubuntu-Italic.ttf",
                "size": 80
            },
            "mdFontBI": {
                "path": "fonts/Ubuntu-BoldItalic.ttf",
                "size": 80
            },
            "mdFontMono": {
                "path": "fonts/UbuntuMono-Regular.ttf",
                "size": 80
            }
        },
        "sprites": [
            "images/*.png"
        ],
        "files": [
            "sounds/*.ogg"
        ],
        "spritesheets": {
            "LogoSheet": [
                "logos/*.png"
            ],
            "APISheet": [
                "*.png"
            ],
            "BlankSheet": [
                "blanks/*.png"
            ]
        }
    }
}
		)");
		debug(obj); endl(std::cout);
		println(obj["resources"]["spritesheets"]["APISheet"][0].as_string());
		println(obj["resources"]["fonts"]["mdFont"]["size"].as_double());
	}

	{
		CoolStruct cool {
			.name = "hello",
			.value = 123
		};
		json::Value obj = json::Object {};
		obj["cool"] = cool;
		debug(obj); endl(std::cout);

		auto another = obj.get<CoolStruct>("cool");
		std::cout << another.name << std::endl;
	}

	{
		Hooray value = Hooray::Hooray;
		json::Object obj;
		obj["hooray"] = value;
		debug(obj); endl(std::cout);
	}

	{
		auto obj = json::parse(R"(
			{
				"hello": "world",
				"nice": null,
				"nested": {
					"objects": ["are", "cool", "\nice \t\ry \buddy, \format me i\f you can \\ \" \\\" "],
					"int": 23,
					"half": 11.5,
					"nested": {
						"again": true
					}
				}
			}
		)");
		println(obj["nested"]["half"].as_double());
		println(obj.dump());
		println(obj.dump(json::NO_INDENTATION));
		println(obj.dump(json::TAB_INDENTATION));

		assert(obj == json::parse(obj.dump()));
		assert(obj == json::parse(obj.dump(json::NO_INDENTATION)));
		assert(obj == json::parse(obj.dump(json::TAB_INDENTATION)));
		assert(obj == json::parse(obj.dump(69)));
	}
	{
		json::Value json;
		json["hello"] = "world";
		println(json.dump());
		assert(json.dump(json::NO_INDENTATION) == R"({"hello":"world"})");
	}
	{
		auto obj = json::parse(R"(
			{
				"key": 5,
				"value": 6,
				"next": 8,
				"hi": 10
			}
		)");

		using UMap = std::unordered_map<std::string, size_t>;
		auto umap = UMap {
			{ "key", 5 },
			{ "value", 6 },
			{ "next", 8 },
			{ "hi", 10 },
		};
		assert(obj.template as<UMap>() == umap);

		using Map = std::map<std::string, size_t>;
		auto map = Map {
			{ "key", 5 },
			{ "value", 6 },
			{ "next", 8 },
			{ "hi", 10 },
		};
		assert(obj.template as<Map>() == map);

		using VMap = std::map<std::string, json::Value>;
		auto vmap = VMap {
			{ "key", 5 },
			{ "value", 6 },
			{ "next", 8 },
			{ "hi", 10 },
		};
		assert(obj.template as<VMap>() == vmap);
	}
	{
		auto arr = json::parse(R"(
			["hi", "mommy", ":3"]
		)");

		using Vec = std::vector<std::string>;
		auto vec = Vec { "hi", "mommy", ":3" };
		assert(arr.template as<Vec>() == vec);

		using Set = std::set<std::string>;
		auto set = Set { "hi", "mommy", ":3" };
		assert(arr.template as<Set>() == set);

		using USet = std::set<std::string>;
		auto uset = USet { "hi", "mommy", ":3" };
		assert(arr.template as<USet>() == uset);
	}
	println("All tests passed :3");
}
