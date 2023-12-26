#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_all.hpp>
#include <matjson.hpp>
#include <matjson/stl_serialize.hpp>
#include <map>
#include <unordered_map>
#include <vector>

struct CoolStruct {
	std::string name;
	int value;

    bool operator==(const CoolStruct&) const = default;
};

template <>
struct matjson::Serialize<CoolStruct> {
	static matjson::Value to_json(const CoolStruct& cool) {
		return matjson::Object {
			{ "name", cool.name },
			{ "value", cool.value }
		};
	}
	static CoolStruct from_json(const matjson::Value& value) {
		return CoolStruct {
			.name = value["name"].as_string(),
			.value = value["value"].as_int()
		};
	}
};

TEST_CASE("Object basics") {
    matjson::Value obj;

    REQUIRE(obj.is_object());
    REQUIRE(obj.is<matjson::Object>());

    obj["foo"] = 42;

    REQUIRE(obj.contains("foo"));
    REQUIRE(obj.as_object().contains("foo"));
    REQUIRE(obj["foo"].is_number());
    REQUIRE(obj["foo"].is<int>());
    REQUIRE(!obj["foo"].is_object());

    REQUIRE(obj["foo"] == 42);
    REQUIRE(obj["foo"] == 42.0);
    REQUIRE(obj["foo"].as_int() == 42);
    REQUIRE(obj["foo"].as_double() == 42.0);

    auto copy = obj;

    REQUIRE(copy["foo"] == 42);

    copy["foo"] = 30;
    REQUIRE(copy["foo"] == 30);
    REQUIRE(obj["foo"] == 42);
}

TEST_CASE("Struct serialization") {
    CoolStruct foo {
        .name = "Hello!",
        .value = GENERATE(take(5, random(-100000, 100000)))
    };

    matjson::Value obj = foo;

    REQUIRE(obj.is_object());
    REQUIRE(obj["name"] == foo.name);
    REQUIRE(obj["value"] == foo.value);
    obj["extra"] = 10;

    REQUIRE(foo == obj.as<CoolStruct>());

    foo.value = 42;
    obj = foo;
    REQUIRE(obj["value"] == 42);
}

TEST_CASE("String serialization") {
    CoolStruct foo {
        .name = "wow!\nmultiline",
        .value = 123
    };

    matjson::Value obj = foo;

    // key order is guaranteed to be the same
    REQUIRE(obj.dump(matjson::NO_INDENTATION) == "{\"name\":\"wow!\\nmultiline\",\"value\":123}");
    REQUIRE(obj.dump(matjson::TAB_INDENTATION) == "{\n\t\"name\": \"wow!\\nmultiline\",\n\t\"value\": 123\n}");
    REQUIRE(obj.dump(1) == "{\n \"name\": \"wow!\\nmultiline\",\n \"value\": 123\n}");
}

TEST_CASE("Keep insertion order") {
    matjson::Value obj(
        {
            {"zzz", "hi"},
            {"aaa", 123},
            {"cool", true}
        }
    );

    int i = 0;
    std::array values = {"zzz", "aaa", "cool"};
    for (auto [key, _] : obj.as_object()) {
        REQUIRE(key == values[i]);
        ++i;
    }
}

static std::string_view COMPLEX_INPUT = R"({
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
})";

TEST_CASE("Parse") {
    auto obj = matjson::parse(COMPLEX_INPUT);

    // risky test! requires we dump arrays the same way
    REQUIRE(obj.dump(4) == COMPLEX_INPUT);

    REQUIRE(obj["nested"]["again"] != true);
    REQUIRE(obj["nested"]["nested"]["again"] == true);
    REQUIRE(obj["nested"]["half"] == 11.5);
    REQUIRE(obj["nice"] == nullptr);
}

TEST_CASE("Dump/parse round trip") {
    auto obj = matjson::parse(COMPLEX_INPUT);

    REQUIRE(obj == matjson::parse(obj.dump()));
    REQUIRE(obj == matjson::parse(obj.dump(matjson::NO_INDENTATION)));
    REQUIRE(obj == matjson::parse(obj.dump(matjson::TAB_INDENTATION)));
    REQUIRE(obj == matjson::parse(obj.dump(69)));
}

TEST_CASE("Test custom Object class") {
    auto obj = matjson::parse(R"(
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
    REQUIRE(obj.as<UMap>() == umap);

    using Map = std::map<std::string, size_t>;
    auto map = Map {
        { "key", 5 },
        { "value", 6 },
        { "next", 8 },
        { "hi", 10 },
    };
    REQUIRE(obj.as<Map>() == map);

    using VMap = std::map<std::string, matjson::Value>;
    auto vmap = VMap {
        { "key", 5 },
        { "value", 6 },
        { "next", 8 },
        { "hi", 10 },
    };
    REQUIRE(obj.as<VMap>() == vmap);
}

TEST_CASE("Test UTF-8 strings") {
    auto obj = matjson::parse("{\"hello\": \"Ol\xC3\xA1!\"}");
    REQUIRE(obj["hello"].as_string() == "Ol\xC3\xA1!");
}