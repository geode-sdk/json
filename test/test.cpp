#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <map>
#include <matjson.hpp>
#include <matjson/reflect.hpp>
#include <matjson/std.hpp>
#include <unordered_map>

struct CoolStruct {
    std::string name;
    int value;

    bool operator==(CoolStruct const&) const = default;
};

using namespace geode;
using namespace matjson;

// template <>
// struct matjson::Serialize<CoolStruct> {
//     static matjson::Value to_json(CoolStruct const& cool) {
//         return matjson::makeObject({{"name", cool.name}, {"value", cool.value}});
//     }

//     static Result<CoolStruct, matjson::GenericError> from_json(matjson::Value const& value) {
//         return CoolStruct{.name = value["name"].as_string(), .value = value["value"].as_int()};
//     }
// };

TEST_CASE("Object basics") {
    Value obj;

    REQUIRE(obj.isObject());

    obj["foo"] = 42;

    REQUIRE(obj.contains("foo"));
    REQUIRE(obj["foo"].isNumber());
    // REQUIRE(obj["foo"].is<int>());
    REQUIRE(!obj["foo"].isObject());

    REQUIRE(obj["foo"] == 42);
    REQUIRE(obj["foo"] == 42.0);
    REQUIRE(obj["foo"].asInt().unwrap() == 42);
    REQUIRE(obj["foo"].asDouble().unwrap() == 42.0);

    auto copy = obj;

    REQUIRE(copy["foo"] == 42);

    copy["foo"] = 30;
    REQUIRE(copy["foo"] == 30);
    REQUIRE(obj["foo"] == 42);
}

TEST_CASE("Struct serialization") {
    CoolStruct foo{.name = "Hello!", .value = GENERATE(take(5, random(-100000, 100000)))};

    Value obj = foo;

    REQUIRE(obj.isObject());
    REQUIRE(obj["name"] == Value(foo.name));
    // wat
    REQUIRE(obj["value"] == foo.value);
    obj["extra"] = 10;

    REQUIRE(foo == obj.as<CoolStruct>().unwrap());

    foo.value = 42;
    obj = foo;
    REQUIRE(obj["value"] == 42);
}

TEST_CASE("String serialization") {
    CoolStruct foo{.name = "wow!\nmultiline", .value = 123};

    matjson::Value obj = foo;

    // key order is guaranteed to be the same
    REQUIRE(obj.dump(matjson::NO_INDENTATION) == "{\"name\":\"wow!\\nmultiline\",\"value\":123}");
    REQUIRE(
        obj.dump(matjson::TAB_INDENTATION) ==
        "{\n\t\"name\": \"wow!\\nmultiline\",\n\t\"value\": 123\n}"
    );
    REQUIRE(obj.dump(1) == "{\n \"name\": \"wow!\\nmultiline\",\n \"value\": 123\n}");
}

TEST_CASE("Keep insertion order") {
    matjson::Value obj = matjson::makeObject({{"zzz", "hi"}, {"aaa", 123}, {"cool", true}});

    obj["crazy"] = true;
    obj.set("awesome", "maybe");

    int i = 0;
    std::array values = {"zzz", "aaa", "cool", "crazy", "awesome"};
    for (auto [key, _] : obj) {
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
    auto obj = matjson::parse(COMPLEX_INPUT).unwrap();

    // risky test! requires we dump arrays the same way
    REQUIRE(obj.dump(4) == COMPLEX_INPUT);

    REQUIRE(obj["nested"]["again"] != true);
    REQUIRE(obj["nested"]["nested"]["again"] == true);
    REQUIRE(obj["nested"]["half"] == 11.5);
    REQUIRE(obj["nice"] == nullptr);
}

TEST_CASE("Dump/parse round trip") {
    auto obj = matjson::parse(COMPLEX_INPUT).unwrap();

    REQUIRE(obj == matjson::parse(obj.dump()).unwrap());
    REQUIRE(obj == matjson::parse(obj.dump(matjson::NO_INDENTATION)).unwrap());
    REQUIRE(obj == matjson::parse(obj.dump(matjson::TAB_INDENTATION)).unwrap());
    REQUIRE(obj == matjson::parse(obj.dump(69)).unwrap());
}

TEST_CASE("STL serialization") {
    auto obj = matjson::parse(R"(
        {
            "key": 5,
            "value": 6,
            "next": 8,
            "hi": 10
        }
    )")
                   .unwrap();

    using UMap = std::unordered_map<std::string, size_t>;
    auto umap = UMap{
        {"key", 5},
        {"value", 6},
        {"next", 8},
        {"hi", 10},
    };
    REQUIRE(obj.as<UMap>().unwrap() == umap);

    using Map = std::map<std::string, size_t>;
    auto map = Map{
        {"key", 5},
        {"value", 6},
        {"next", 8},
        {"hi", 10},
    };
    REQUIRE(obj.as<Map>().unwrap() == map);

    using VMap = std::map<std::string, matjson::Value>;
    auto vmap = VMap{
        {"key", 5},
        {"value", 6},
        {"next", 8},
        {"hi", 10},
    };
    REQUIRE(obj.as<VMap>().unwrap() == vmap);

    auto arr = matjson::parse("[1,2,3,4,5]").unwrap();

    REQUIRE(arr.as<std::vector<int>>().unwrap() == std::vector{1, 2, 3, 4, 5});
    REQUIRE(arr.as<std::set<int>>().unwrap() == std::set{1, 2, 3, 4, 5});
    REQUIRE(arr.as<std::unordered_set<int>>().unwrap() == std::unordered_set{1, 2, 3, 4, 5});

    REQUIRE(arr[0].as<std::optional<int>>().unwrap().has_value());
    REQUIRE(!arr[123].as<std::optional<int>>().unwrap().has_value());
}

TEST_CASE("UTF-8 strings") {
    auto obj = matjson::parse("{\"hello\": \"Ol\xC3\xA1!\"}").unwrap();
    REQUIRE(obj["hello"].asString().unwrap() == "Ol\xC3\xA1!");
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
    REQUIRE(matjson::parse("123").unwrap().asInt().unwrap() == 123);
    REQUIRE(matjson::parse("-123").unwrap().asInt().unwrap() == -123);
    REQUIRE(matjson::parse("123\n").unwrap().asInt().unwrap() == 123);
    REQUIRE(matjson::parse("   123  ").unwrap().asInt().unwrap() == 123);
    REQUIRE(matjson::parse("123  ").unwrap().asInt().unwrap() == 123);
    REQUIRE(matjson::parse("   123").unwrap().asInt().unwrap() == 123);

    REQUIRE(matjson::parse("0.0").unwrap().asDouble().unwrap() == 0.0);
    REQUIRE(matjson::parse("0.05").unwrap().asDouble().unwrap() == 0.05);
    REQUIRE(matjson::parse("123").unwrap().asDouble().unwrap() == 123.0);
    REQUIRE(matjson::parse("123.0").unwrap().asDouble().unwrap() == 123.0);
    REQUIRE(matjson::parse("123.123").unwrap().asDouble().unwrap() == 123.123);
    REQUIRE(matjson::parse("-123.123").unwrap().asDouble().unwrap() == -123.123);

    REQUIRE(matjson::parse("true").unwrap().asBool().unwrap() == true);
    REQUIRE(matjson::parse("  true").unwrap().asBool().unwrap() == true);
    REQUIRE(matjson::parse("true  ").unwrap().asBool().unwrap() == true);

    REQUIRE(matjson::parse("false").unwrap().asBool().unwrap() == false);
    REQUIRE(matjson::parse("false   ").unwrap().asBool().unwrap() == false);
    REQUIRE(matjson::parse("   false").unwrap().asBool().unwrap() == false);

    REQUIRE(matjson::parse("\"hello\"").unwrap().asString().unwrap() == "hello");
    REQUIRE(matjson::parse("\"hello\"   ").unwrap().asString().unwrap() == "hello");
    REQUIRE(matjson::parse("  \"hello\"").unwrap().asString().unwrap() == "hello");

    REQUIRE(matjson::parse("null").unwrap().isNull());
    REQUIRE(matjson::parse("[]").unwrap().isArray());
    REQUIRE(matjson::parse("{}").unwrap().isObject());

    REQUIRE_THROWS(matjson::parse("").unwrap());
    REQUIRE_THROWS(matjson::parse("  ").unwrap());
    REQUIRE_THROWS(matjson::parse("invalid").unwrap());
}

TEST_CASE("Invalid json") {
    REQUIRE_THROWS(matjson::parse("{").unwrap());
    REQUIRE_THROWS(matjson::parse("}").unwrap());
    REQUIRE_THROWS(matjson::parse("[10, 10,]").unwrap());
    REQUIRE_THROWS(matjson::parse("{\"hello\"}").unwrap());
    REQUIRE_THROWS(matjson::parse("{123: 123}").unwrap());
    REQUIRE_THROWS(matjson::parse("[null, 10, \"]").unwrap());

    // Very invalid
    using namespace std::string_view_literals;
    REQUIRE(matjson::parse("[\"hi\x00the\"]"sv).isErrAnd([](auto const& err) {
        return err.message == "invalid string";
    }));
}

TEST_CASE("Dump with inf and nan") {
    matjson::Value obj;
    using namespace std::string_literals;

    // json cant represent nan or infinity, sadly

    obj["Hi"] = NAN;
    obj["wow"] = INFINITY;
    obj["wow2"] = -INFINITY;
    REQUIRE(obj.dump(NO_INDENTATION) == "{\"Hi\":null,\"wow\":null,\"wow2\":null}"s);
}

TEST_CASE("Number precision") {
    matjson::Value obj = 0.1;
    REQUIRE(obj.dump() == "0.1");

    obj = 123;
    REQUIRE(obj.dump() == "123");

    obj = 123.23;
    REQUIRE(obj.dump() == "123.23");

    obj = 123456789;
    REQUIRE(obj.dump() == "123456789");

    obj = 1234567895017;
    REQUIRE(obj.dump() == "1234567895017");

    obj = 1234567895017.234;
    REQUIRE(obj.dump() == "1234567895017.234");
}

#if 0

TEST_CASE("Rvalue asArray() return") {
    auto get_json = [] {
        return matjson::parse("[1,2,3,4]").unwrap();
    };
    // `auto& arr = get_json().as_array();` should fail to compile, however i can't test that
    auto const& arr = get_json().asArray();
    REQUIRE(arr.size() == 4);
}

#endif

TEST_CASE("Parsing unicode characters") {
    auto obj = matjson::parse(R"(
        {
            "hello": "\u00D3l\u00E1!",
            "cool": "😎",
            "pair": "\uD83D\uDE00"
        }
    )")
                   .unwrap();

    REQUIRE(obj["hello"].asString().unwrap() == "Ólá!");
    REQUIRE(obj["cool"].asString().unwrap() == "😎");
    REQUIRE(obj["pair"].asString().unwrap() == "😀");
}

TEST_CASE("Special characters") {
    auto obj = matjson::parse(R"(
        {
            "control": "\b\f\n\r\t\u0012 "
        }
    )")
                   .unwrap();

    REQUIRE(obj["control"].asString().unwrap() == "\b\f\n\r\t\x12 ");
}

TEST_CASE("Implicit ctors") {
    matjson::Value value;

    value["a"] = 123;
    value["a"] = 123.0;
    value["a"] = true;
    value["a"] = false;
    value["a"] = nullptr;
    value["a"] = "Hello!";
    std::string foo;
    value["a"] = foo;
    char const* bar = "hi";
    value["a"] = bar;
    std::string_view baz = "c";
    value["a"] = baz;

    struct Test {
        operator std::string() {
            return "hi";
        }
    } t;

    value["a"] = t;

    value["a"] = CoolStruct{
        .name = "Hello!",
        .value = 123,
    };
    CoolStruct b{};
    value["a"] = b;
}

TEST_CASE("ParseError line numbers") {
    auto err = matjson::parse("{").unwrapErr();
    REQUIRE(err.line == 1);
    REQUIRE(err.column == 2);

    err = matjson::parse("{\n\"hello").unwrapErr();

    REQUIRE(err.line == 2);
    REQUIRE(err.column == 7);
}
