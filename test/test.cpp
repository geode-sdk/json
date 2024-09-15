#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <matjson.hpp>
#include <matjson/stl_serialize.hpp>
#include <map>
#include <unordered_map>
#include <array>

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
			.value = static_cast<int>(value["value"].as_integer())
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
    REQUIRE(obj["foo"].as_integer() == 42);
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

    obj["crazy"] = true;
    obj.set("awesome", "maybe");

    int i = 0;
    std::array values = {"zzz", "aaa", "cool", "crazy", "awesome"};
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
        },
        "empty": {},
        "empty_arr": []
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

TEST_CASE("STL serialization") {
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

TEST_CASE("UTF-8 strings") {
    auto obj = matjson::parse("{\"hello\": \"Ol\xC3\xA1!\"}");
    REQUIRE(obj["hello"].as_string() == "Ol\xC3\xA1!");
}

TEST_CASE("Mutate object") {
    matjson::Value obj;

    obj.set("hello", 123);
    REQUIRE(obj.dump(matjson::NO_INDENTATION) == "{\"hello\":123}");

    obj["hello!"] = 1234;
    REQUIRE(obj.dump(matjson::NO_INDENTATION) == "{\"hello\":123,\"hello!\":1234}");

    obj.set("hello!", 4);
    REQUIRE(obj.dump(matjson::NO_INDENTATION) == "{\"hello\":123,\"hello!\":4}");
    
    obj.erase("hello!");
    REQUIRE(obj.dump(matjson::NO_INDENTATION) == "{\"hello\":123}");
}

TEST_CASE("Parse unit values") {
    REQUIRE(matjson::parse("123").as_integer() == 123);
    REQUIRE(matjson::parse("-123").as_integer() == -123);
    REQUIRE(matjson::parse("123\n").as_integer() == 123);
    REQUIRE(matjson::parse("   123  ").as_integer() == 123);
    REQUIRE(matjson::parse("123  ").as_integer() == 123);
    REQUIRE(matjson::parse("   123").as_integer() == 123);

    REQUIRE(matjson::parse("123").as_uinteger() == 123);
    REQUIRE(matjson::parse("123\n").as_uinteger() == 123);
    REQUIRE(matjson::parse("   123  ").as_uinteger() == 123);
    REQUIRE(matjson::parse("123  ").as_uinteger() == 123);
    REQUIRE(matjson::parse("   123").as_uinteger() == 123);

    REQUIRE(matjson::parse("0.0").as_double() == 0.0);
    REQUIRE(matjson::parse("0.05").as_double() == 0.05);
    REQUIRE(matjson::parse("123").as_double() == 123.0);
    REQUIRE(matjson::parse("123.0").as_double() == 123.0);
    REQUIRE(matjson::parse("123.123").as_double() == 123.123);
    REQUIRE(matjson::parse("-123.123").as_double() == -123.123);

    REQUIRE(matjson::parse("true").as_bool() == true);
    REQUIRE(matjson::parse("  true").as_bool() == true);
    REQUIRE(matjson::parse("true  ").as_bool() == true);
    
    REQUIRE(matjson::parse("false").as_bool() == false);
    REQUIRE(matjson::parse("false   ").as_bool() == false);
    REQUIRE(matjson::parse("   false").as_bool() == false);
    
    REQUIRE(matjson::parse("\"hello\"").as_string() == "hello");
    REQUIRE(matjson::parse("\"hello\"   ").as_string() == "hello");
    REQUIRE(matjson::parse("  \"hello\"").as_string() == "hello");

    REQUIRE(matjson::parse("null").is_null());
    REQUIRE(matjson::parse("[]").is_array());
    REQUIRE(matjson::parse("{}").is_object());

    REQUIRE_THROWS(matjson::parse(""));
    REQUIRE_THROWS(matjson::parse("  "));
    REQUIRE_THROWS(matjson::parse("invalid"));
}

TEST_CASE("Invalid json") {
    REQUIRE_THROWS(matjson::parse("{"));
    REQUIRE_THROWS(matjson::parse("}"));
    REQUIRE_THROWS(matjson::parse("[10, 10,]"));
    REQUIRE_THROWS(matjson::parse("{\"hello\"}"));
    REQUIRE_THROWS(matjson::parse("{123: 123}"));
    REQUIRE_THROWS(matjson::parse("[null, 10, \"]"));

    // Very invalid
    using namespace std::string_view_literals;
    using Catch::Matchers::Message;
    REQUIRE_THROWS_MATCHES(matjson::parse("[\"hi\x00the\"]"sv), std::runtime_error, Message("invalid string"));
}

TEST_CASE("Invalid dump") {
    matjson::Value obj;
    using namespace std::string_literals;
    // no throw
    obj.dump();

    // json cant represent nan or infinity, sadly

    obj["Hi"] = NAN;
    REQUIRE_THROWS(obj.dump());

    obj.as_object().clear();
    obj.dump();
    
    obj["wow"] = INFINITY;
    REQUIRE_THROWS(obj.dump());
}

TEST_CASE("Number precision") {
    matjson::Value obj = 0.1;
    REQUIRE(obj.dump() == "0.1");

    obj = 123;
    REQUIRE(obj.dump() == "123");

    obj = 123.23;
    REQUIRE(obj.dump() == "123.23");

    // internally these are stored as double, but that
    // should have enough precision for these big numbers.
    // maybe in the future consider storing them as integers
    obj = 123456789;
    REQUIRE(obj.dump() == "123456789");

    obj = 1234567895017;
    REQUIRE(obj.dump() == "1234567895017");

    obj = 1234567895017.234;
    REQUIRE(obj.dump() == "1234567895017.234");
}

TEST_CASE("Rvalue as_array() return") {
    auto get_json = [] {
        return matjson::parse("[1,2,3,4]");
    };
    // `auto& arr = get_json().as_array();` should fail to compile, however i can't test that
    auto const& arr = get_json().as_array();
    REQUIRE(arr.size() == 4);
}

TEST_CASE("Parsing unicode characters") {
    auto obj = matjson::parse(R"(
        {
            "hello": "\u00D3l\u00E1!",
            "cool": "😎",
            "pair": "\uD83D\uDE00"
        }
    )");

    REQUIRE(obj["hello"].as_string() == "Ólá!");
    REQUIRE(obj["cool"].as_string() == "😎");
    REQUIRE(obj["pair"].as_string() == "😀");
}

TEST_CASE("Special characters") {
    auto obj = matjson::parse(R"(
        {
            "control": "\b\f\n\r\t\u0012 "
        }
    )");

    REQUIRE(obj["control"].as_string() == "\b\f\n\r\t\x12 ");
}

TEST_CASE("Very big numbers") {
    matjson::Value obj = 1ll << 61;
    REQUIRE(obj.as_integer() == 1ll << 61);
    REQUIRE(obj.as_uinteger() == 1ull << 61);

    obj = (1ull << 63) + 1;
    REQUIRE(obj.as_integer() == (1ll << 63) + 1); // actually negative!
    REQUIRE(obj.as_uinteger() == (1ull << 63) + 1); // correct one
}