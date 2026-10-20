#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
//
#include <array>
#include <map>
#include <matjson.hpp>
#include <matjson/reflect_all.hpp>
#include <matjson/std.hpp>
#include <unordered_map>

struct CoolStruct {
    std::string name;
    int value;

    bool operator==(CoolStruct const&) const = default;
};

using namespace geode;
using namespace matjson;
using namespace std::string_literals;
using namespace std::string_view_literals;

// template <>
// struct matjson::Serialize<CoolStruct> {
//     static matjson::Value toJson(CoolStruct const& cool) {
//         return matjson::makeObject({{"name", cool.name}, {"value", cool.value}});
//     }

//     static Result<CoolStruct> fromJson(matjson::Value const& value) {
//         CoolStruct res;
//         GEODE_UNWRAP_INTO(res.name, value["name"].asString());
//         GEODE_UNWRAP_INTO(res.value, value["value"].asInt());
//         return Ok(res);
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
    CoolStruct foo{.name = "Hello!", .value = 12345};

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

    REQUIRE(*arr[0].as<std::shared_ptr<int>>().unwrap() == 1);
    REQUIRE(*arr[0].as<std::unique_ptr<int>>().unwrap() == 1);

    arr.push(nullptr);

    REQUIRE(arr[5].as<std::shared_ptr<int>>().unwrap() == nullptr);
    REQUIRE(arr[5].as<std::unique_ptr<int>>().unwrap() == nullptr);

    REQUIRE(Value(std::shared_ptr<int>()).dump() == "null");
    REQUIRE(Value(std::unique_ptr<int>()).dump() == "null");

    std::vector<double> nums = {1.0, 3.4};
    std::span<double> span = nums;

    REQUIRE(Value(nums).dump(0) == "[1.0,3.4]");
    REQUIRE(Value(span).dump(0) == "[1.0,3.4]");
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
    REQUIRE(err.offset == 1);

    err = matjson::parse("{\n\"hello").unwrapErr();

    REQUIRE(err.line == 2);
    REQUIRE(err.column == 7);
    REQUIRE(err.offset == 8);
}

TEST_CASE("ParseError line numbers from stream") {
    std::istringstream stream("{");
    auto err = matjson::parse(stream).unwrapErr();
    REQUIRE(err.line == 1);
    REQUIRE(err.column == 2);
    REQUIRE(err.offset == 1);

    stream = std::istringstream("{\n\"hello");
    err = matjson::parse(stream).unwrapErr();
    REQUIRE(err.line == 2);
    REQUIRE(err.column == 7);
    REQUIRE(err.offset == 8);
}

TEST_CASE("parseAs") {
    auto res = matjson::parseAs<CoolStruct>(R"({"name": "Hello!","value": 123})");

    REQUIRE(res.unwrap() == CoolStruct{.name = "Hello!", .value = 123});

    REQUIRE(matjson::parseAs<int>("123").unwrap() == 123);
}

TEST_CASE("Parse from stream") {
    std::istringstream stream(R"({"name": "Hello!","value": 123})");

    auto res = matjson::parse(stream).unwrap();
    // parsing should consume the whole stream
    REQUIRE(stream.eof());

    REQUIRE(res == CoolStruct{.name = "Hello!", .value = 123});

    stream = std::istringstream("[1,2,3]");
    auto arr = matjson::parse(stream).unwrap();

    REQUIRE(arr == std::vector{1, 2, 3});

    stream = std::istringstream("[1, 2, 3");
    REQUIRE(matjson::parse(stream).isErr());

    stream = std::istringstream("[1, 2!, 3");
    REQUIRE(matjson::parse(stream).isErr());
    REQUIRE(!stream.eof());

    stream = std::istringstream("");
    REQUIRE(matjson::parse(stream).isErr());
    REQUIRE(stream.eof());

    stream = std::istringstream("     ");
    REQUIRE(matjson::parse(stream).isErr());
    REQUIRE(stream.eof());

    stream = std::istringstream("[1, 2, 3]   ");
    REQUIRE(matjson::parse(stream).isOk());

    stream = std::istringstream("[1, 2, 3]  a");
    REQUIRE(matjson::parse(stream).isErr());
    REQUIRE(!stream.eof());

    stream = std::istringstream("[1, 2, 3]  a  b");
    REQUIRE(matjson::parse(stream).isErr());
    REQUIRE(!stream.eof());
}

TEST_CASE("Value::get(..) and Value::get<T>(...)") {
    auto obj = matjson::makeObject({
        {"name", "Hello!"},
        {"value", 123},
        {"arr", std::vector{1, 2, 3, 4}},
        {"nested", matjson::makeObject({{"name", "hello"}, {"value", 123}})},
    });

    REQUIRE(obj.get("name").unwrap().asString().unwrap() == "Hello!");
    REQUIRE(obj.get("value").unwrap().asInt().unwrap() == 123);
    REQUIRE(
        obj.get("nested").unwrap().as<CoolStruct>().unwrap() ==
        CoolStruct{.name = "hello", .value = 123}
    );
    REQUIRE(obj.get(123).isErr());
    REQUIRE(obj["arr"].get(0).unwrap().asInt().unwrap() == 1);
    REQUIRE(obj["arr"].get(123).isErr());

    REQUIRE(obj.get<std::string>("name").unwrap() == "Hello!");
    REQUIRE(obj.get<int>("value").unwrap() == 123);
    REQUIRE(obj.get<CoolStruct>("nested").unwrap() == CoolStruct{.name = "hello", .value = 123});
    REQUIRE(obj.get<int>(123).isErr());
    REQUIRE(obj["arr"].get<int>(0).unwrap() == 1);
    REQUIRE(obj["arr"].get<int>(123).isErr());

    auto const constObj = obj;

    REQUIRE(constObj.get("name").unwrap().asString().unwrap() == "Hello!");
    REQUIRE(constObj.get("value").unwrap().asInt().unwrap() == 123);
    REQUIRE(
        constObj.get("nested").unwrap().as<CoolStruct>().unwrap() ==
        CoolStruct{.name = "hello", .value = 123}
    );
    REQUIRE(constObj.get(123).isErr());
    REQUIRE(constObj["arr"].get(0).unwrap().asInt().unwrap() == 1);
    REQUIRE(constObj["arr"].get(123).isErr());

    REQUIRE(constObj.get<std::string>("name").unwrap() == "Hello!");
    REQUIRE(constObj.get<int>("value").unwrap() == 123);
    REQUIRE(constObj.get<CoolStruct>("nested").unwrap() == CoolStruct{.name = "hello", .value = 123});
    REQUIRE(constObj.get<int>(123).isErr());
    REQUIRE(constObj["arr"].get<int>(0).unwrap() == 1);
    REQUIRE(constObj["arr"].get<int>(123).isErr());
}

TEST_CASE("Leftover characters") {
    REQUIRE(matjson::parse("123").isOk());
    REQUIRE(matjson::parse("123  ").isOk());
    REQUIRE(matjson::parse("123!").isErr());
    REQUIRE(matjson::parse("123  !").isErr());
    REQUIRE(matjson::parse("123\x00"sv).isErr());
    REQUIRE(matjson::parse("123  \x00"sv).isErr());
    REQUIRE(matjson::parse("123@"sv).isErr());
    REQUIRE(matjson::parse("1]"sv).isErr());
    REQUIRE(matjson::parse("{}}"sv).isErr());
}

TEST_CASE("Value::operator= key behavior") {
    matjson::Value obj;
    matjson::Value foo = "hello";

    obj = foo;
    REQUIRE(obj == foo);
    REQUIRE(obj.asString().unwrap() == "hello");

    REQUIRE(!obj.getKey().has_value());
    REQUIRE(!foo.getKey().has_value());

    foo = matjson::makeObject({{"key", "value"}});
    obj = matjson::makeObject({{"a", "b"}});

    auto& key = foo["key"];
    auto& a = obj["a"];

    obj["a"] = foo["key"];

    REQUIRE(key.getKey().value() == "key");
    REQUIRE(a.getKey().value() == "a");

    obj["a"] = std::move(foo["key"]);

    REQUIRE(a.getKey().value() == "a");

    REQUIRE(key.isNull());
    REQUIRE(foo.contains("key"));
    REQUIRE(foo["key"].isNull());
}

TEST_CASE("Value isExactlyT") {
    matjson::Value value = 123ull;
    REQUIRE(!value.isExactlyInt());
    REQUIRE(value.isExactlyUInt());
    REQUIRE(!value.isExactlyDouble());
    REQUIRE(!value.isExactlyFloat());

    value = 123.0;
    REQUIRE(!value.isExactlyInt());
    REQUIRE(!value.isExactlyUInt());
    REQUIRE(value.isExactlyDouble());
    REQUIRE(!value.isExactlyFloat());

    value = 123.0f;
    REQUIRE(!value.isExactlyInt());
    REQUIRE(!value.isExactlyUInt());
    REQUIRE(!value.isExactlyDouble());
    REQUIRE(value.isExactlyFloat());

    value = -10;
    REQUIRE(value.isExactlyInt());
    REQUIRE(!value.isExactlyUInt());
    REQUIRE(!value.isExactlyDouble());
    REQUIRE(!value.isExactlyFloat());

    value = "hello";
    REQUIRE(!value.isExactlyInt());
    REQUIRE(!value.isExactlyUInt());
    REQUIRE(!value.isExactlyDouble());
    REQUIRE(!value.isExactlyFloat());

    // parsed ints are stored as unsigned unless they are negative
    value = matjson::parse("123").unwrap();
    REQUIRE(!value.isExactlyInt());
    REQUIRE(value.isExactlyUInt());
    REQUIRE(!value.isExactlyDouble());
    REQUIRE(!value.isExactlyFloat());

    value = matjson::parse("-123").unwrap();
    REQUIRE(value.isExactlyInt());
    REQUIRE(!value.isExactlyUInt());
    REQUIRE(!value.isExactlyDouble());
    REQUIRE(!value.isExactlyFloat());

    // parsed decimals are always stored as double
    value = matjson::parse("0.1").unwrap();
    REQUIRE(!value.isExactlyInt());
    REQUIRE(!value.isExactlyUInt());
    REQUIRE(value.isExactlyDouble());
    REQUIRE(!value.isExactlyFloat());
}

TEST_CASE("Value::erase") {
    matjson::Value obj;
    obj["a"] = 1;
    obj["b"] = 2;
    obj["c"] = 3;

    REQUIRE(obj.erase("b"));
    REQUIRE(obj.dump(0) == "{\"a\":1,\"c\":3}");

    REQUIRE(!obj.erase("b"));
    REQUIRE(obj.dump(0) == "{\"a\":1,\"c\":3}");

    REQUIRE(obj.erase("a"));
    REQUIRE(obj.dump(0) == "{\"c\":3}");

    REQUIRE(obj.erase("c"));
    REQUIRE(obj.dump(0) == "{}");

    obj = matjson::parse(R"({"a":1,"b":2,"c":3,"d":4})").unwrap();

    REQUIRE(obj.erase("b"));
    REQUIRE(obj.dump(0) == R"({"a":1,"c":3,"d":4})");
}

TEST_CASE("Trailing commas") {
    auto const opts = ParseOpts{.trailingCommas = true};
    auto src1 = "[1,\n 20,\n 3,\n ]";

    REQUIRE(matjson::parse(src1).isErr());
    auto value = matjson::parse(src1, opts).unwrap();

    REQUIRE(value[0] == 1);
    REQUIRE(value[1] == 20);
    REQUIRE(value[2] == 3);
    REQUIRE(value.dump(0) == "[1,20,3]");

    auto src2 = R"({"hello": true,})";
    REQUIRE(matjson::parse(src2).isErr());
    value = matjson::parse(src2, opts).unwrap();

    REQUIRE(value["hello"] == true);
    REQUIRE(value.dump(0) == R"({"hello":true})");

    auto src3 = R"([1, [20, 3,], 4])";
    REQUIRE(matjson::parse(src3).isErr());
    value = matjson::parse(src3, opts).unwrap();

    REQUIRE(value.dump(0) == "[1,[20,3],4]");

    auto src4 = "[1, [2,],, 3]";
    REQUIRE(matjson::parse(src4).isErr());
    REQUIRE(matjson::parse(src4, opts).isErr());

    auto src5 = "[1, [2,], 3,,]";
    REQUIRE(matjson::parse(src5).isErr());
    REQUIRE(matjson::parse(src5, opts).isErr());
}

TEST_CASE("Comments") {
    auto const opts = ParseOpts{.comments = true};
    auto src1 = "//at the very start!\n[]";

    REQUIRE(matjson::parse(src1).isErr());
    auto value = matjson::parse(src1, opts).unwrap();
    REQUIRE(value.isArray());

    auto src2 = "[]//into eof";
    REQUIRE(matjson::parse(src2).isErr());
    value = matjson::parse(src2, opts).unwrap();

    REQUIRE(value.isArray());
    REQUIRE(value.dump(0) == "[]");

    auto src3 = "//just a comment";
    REQUIRE(matjson::parse(src3).isErr());
    REQUIRE(matjson::parse(src3, opts).isErr());

    auto src4 = "[//hello!]";
    REQUIRE(matjson::parse(src4).isErr());
    REQUIRE(matjson::parse(src4, opts).isErr());

    auto src5 = "[/!]";
    auto res = matjson::parse(src5, opts);
    auto resErr = res.unwrapErr();
    REQUIRE(resErr.message == "expected comment");
    REQUIRE(resErr.offset == 2);
    REQUIRE(resErr.line == 1);
    REQUIRE(resErr.column == 3);

    auto src6 = R"("in a string // is fine!")";
    value = matjson::parse(src6).unwrap();
    REQUIRE(value == "in a string // is fine!");
    value = matjson::parse(src6, opts).unwrap();
    REQUIRE(value == "in a string // is fine!");

    auto src7 =
        "/*hello! this is a // stupid\n comment i dont like this style ***/[1, /* well it works "
        "here */ 2]";
    REQUIRE(matjson::parse(src7).isErr());
    value = matjson::parse(src7, opts).unwrap();
    REQUIRE(value.isArray());
    REQUIRE(value.dump(0) == "[1,2]");

    auto src8 = "[1,/**/2]";
    REQUIRE(matjson::parse(src8).isErr());
    value = matjson::parse(src8, opts).unwrap();
    REQUIRE(value.isArray());
    REQUIRE(value.dump(0) == "[1,2]");

    auto src9 = "[1, 2]/* hello";
    resErr = matjson::parse(src9, opts).unwrapErr();
    REQUIRE(resErr.message == "expected end of comment");
}

TEST_CASE("Value after move") {
    SUBCASE("Moving out the entire object") {
        matjson::Value value = matjson::parse("{\"hello\": \"world\"}").unwrap();
        matjson::Value newOne(std::move(value));
        REQUIRE(value.isNull());
        REQUIRE(newOne.isObject());
        REQUIRE(newOne["hello"] == "world");

        REQUIRE(value.dump(0) == "null");
        REQUIRE(newOne.dump(0) == "{\"hello\":\"world\"}");
    }

    SUBCASE("Moving out part of an object") {
        matjson::Value value =
            matjson::parse(R"({"hello": "world", "sub": {"value": true}, "bye": 10})").unwrap();
        matjson::Value newOne(std::move(value["sub"]));
        REQUIRE(newOne.isObject());
        REQUIRE(newOne["value"] == true);

        REQUIRE(value.isObject());
        REQUIRE(value.contains("sub"));
        REQUIRE(value["sub"] == nullptr);
        // its not the dummy null value
        REQUIRE(value["sub"].getKey().has_value());
        REQUIRE(value["hello"] == "world");
        REQUIRE(value["bye"] == 10);

        REQUIRE(value.dump(0) == R"({"hello":"world","sub":null,"bye":10})");
    }

    SUBCASE("Moving out and assigning in its place") {
        matjson::Value value =
            matjson::parse(R"({"hello": "world", "sub": {"value": true}, "bye": 10})").unwrap();
        matjson::Value newOne(std::move(value["sub"]));

        auto& newSub = (value["sub"] = matjson::Value("hello"));

        REQUIRE(newOne["value"] == true);
        REQUIRE(newSub.getKey().value() == "sub");
        REQUIRE(value["sub"] == "hello");
    }

    SUBCASE("Moving out and copying it somewhere else") {
        matjson::Value value =
            matjson::parse(R"({"hello": "world", "sub": {"value": true}, "bye": 10})").unwrap();
        matjson::Value newOne(std::move(value["sub"]));

        matjson::Value value2 = matjson::parse(R"({"hello": "world"})").unwrap();
        value2["test"] = newOne;

        REQUIRE(!value.contains("test"));
        REQUIRE(value2.contains("test"));
        REQUIRE(!value2.contains("sub"));
        REQUIRE(value2["test"]["value"] == true);
        REQUIRE(value["sub"] == nullptr);
    }

    SUBCASE("Moving out and moving it somewhere else") {
        matjson::Value value =
            matjson::parse(R"({"hello": "world", "sub": {"value": true}, "bye": 10})").unwrap();
        matjson::Value newOne(std::move(value["sub"]));

        matjson::Value value2 = matjson::parse(R"({"hello": "world"})").unwrap();
        value2["test"] = std::move(newOne);

        REQUIRE(!value.contains("test"));
        REQUIRE(value2.contains("test"));
        REQUIRE(!value2.contains("sub"));
        REQUIRE(value2["test"]["value"] == true);
        REQUIRE(value["sub"] == nullptr);
    }

    SUBCASE("Self move-assignment") {
        matjson::Value value = matjson::parse(R"({"a": 123})").unwrap();

        value = std::move(value);
        REQUIRE(value.dump(0) == R"({"a":123})");
        REQUIRE(value["a"] == 123);
    }
}

TEST_CASE("Dummy value") {
    SUBCASE("Does not get a key set on it") {
        matjson::Value object;
        auto& dummy = std::as_const(object)["no_exist"];
        REQUIRE(dummy.isNull());
        REQUIRE(!dummy.getKey().has_value());

        object["foo"] = std::move(dummy);
        REQUIRE(object.contains("foo"));
        REQUIRE(object.get("foo").isOk());

        // dummy value should stay unaffected
        REQUIRE(dummy.isNull());
        REQUIRE(!dummy.getKey().has_value());

        auto& dummy2 = std::as_const(object)["no_exist"];
        REQUIRE(dummy2.isNull());
        REQUIRE(!dummy2.getKey().has_value());
    }
}

TEST_CASE("Float dump precision") {
    matjson::Value value = 0.1f;
    REQUIRE(value.dump(0) == "0.1");

    value = 0.1;
    REQUIRE(value.dump(0) == "0.1");

    value = 1000.0f;
    REQUIRE(value.dump(0) == "1000.0");

    value = 1000.0;
    REQUIRE(value.dump(0) == "1000.0");

    value = 1e30f;
    REQUIRE(value.dump(0) == "1e+30");

    value = 1e30;
    REQUIRE(value.dump(0) == "1e+30");
}

TEST_CASE("Comparisons") {
    SUBCASE("Object equality checks keys") {
        auto a = matjson::makeObject({{"a", 1}});
        auto b = matjson::makeObject({{"b", 1}});
        CHECK_FALSE(a == b);
        CHECK(a == matjson::makeObject({{"a", 1}}));
        CHECK_FALSE(a == matjson::makeObject({{"a", 2}}));
    }

    SUBCASE("Object ordering is irreflexive and consistent") {
        auto a = matjson::makeObject({{"a", 1}});
        auto b = matjson::makeObject({{"b", 1}});
        CHECK_FALSE(a < a);
        CHECK((a < b) != (b < a));
        CHECK((a > b) == (b < a));
    }

    SUBCASE("Null ordering is irreflexive") {
        matjson::Value n1 = nullptr, n2 = nullptr;
        CHECK_FALSE(n1 < n2);
        CHECK_FALSE(n1 > n2);
        CHECK(n1 == n2);
    }
}

TEST_CASE("Max array/object nesting") {
    SUBCASE("Parser rejects excessive nesting instead of crashing") {
        std::string deep(200000, '[');
        deep += std::string(200000, ']');
        auto res = matjson::parse(deep);
        CHECK(res.isErr());
    }

    SUBCASE("Parser accepts nesting up to maxDepth") {
        matjson::ParseOpts opts{.maxDepth = 3};
        CHECK(matjson::parse("[[[1]]]", opts).isOk());
        CHECK(matjson::parse("[[[[1]]]]", opts).isErr());
    }

    SUBCASE("Parser depth limit resets across sibling containers") {
        matjson::ParseOpts opts{.maxDepth = 2};
        // many siblings at depth 2 must not accumulate depth
        CHECK(matjson::parse("[[1],[2],[3],[4],[5],[6]]", opts).isOk());
        CHECK(matjson::parse("[[1],[2],[3, [10]],[4],[5],[6]]", opts).isErr());
    }
}

TEST_CASE("Duplicate key parsing") {
    matjson::Value value = matjson::parse(R"({"a": 10, "a": 123})").unwrap();
    CHECK(value.size() == 1);
    // keep only the last value, similar to other libraries (python, js, rust)
    CHECK(value["a"] == 123);
    CHECK(value.dump(0) == R"({"a":123})");
}

TEST_CASE("Duplicate keys keep the position of the first occurrence") {
    auto v = matjson::parse(R"({"a":1,"b":2,"a":3})").unwrap();
    CHECK(v.dump(0) == R"({"a":3,"b":2})");
}

TEST_CASE("Plain nulls") {
    SUBCASE("Behave like any other null") {
        matjson::Value n = nullptr;
        CHECK(n.isNull());
        CHECK(n.type() == matjson::Type::Null);
        CHECK(!n.getKey().has_value());
        CHECK(n.dump(0) == "null");
        CHECK(n.size() == 0);
        CHECK(n == nullptr);
        CHECK(n == matjson::parse("null").unwrap());
        CHECK_FALSE(n == matjson::Value(0));
        CHECK_FALSE(n == matjson::Value::object());
        CHECK(n.asBool().isErr());
        CHECK(n.asString().isErr());
        CHECK(n.get("a").isErr());
        CHECK(n.get(0).isErr());
        CHECK(n.begin() == n.end());
    }

    SUBCASE("Ordering is the same as for nodes with a payload") {
        matjson::Value n = nullptr;
        matjson::Value parsedNull = matjson::parse("null").unwrap();
        for (auto const& other :
             {matjson::Value(1),
              matjson::Value("s"),
              matjson::Value(true),
              matjson::Value::array(),
              matjson::Value::object()}) {
            CHECK((other < n) == (other < parsedNull));
            CHECK((n < other) == (parsedNull < other));
            CHECK((n > other) == (parsedNull > other));
            CHECK((other > n) == (other > parsedNull));
        }
        CHECK_FALSE(n < parsedNull);
        CHECK_FALSE(parsedNull < n);
        CHECK_FALSE(n > parsedNull);
    }

    SUBCASE("Copying and assigning") {
        matjson::Value n = nullptr;
        matjson::Value copy = n;
        CHECK(copy.isNull());

        matjson::Value v = 5;
        v = n;
        CHECK(v.isNull());
        v = 7;
        CHECK(v == 7);
        v = std::move(n);
        CHECK(v.isNull());
    }

    SUBCASE("Inside containers") {
        matjson::Value arr = matjson::Value::array();
        arr.push(nullptr);
        arr.push(1);
        arr.push(nullptr);
        CHECK(arr.dump(0) == "[null,1,null]");
        CHECK(arr == matjson::parse("[null, 1, null]").unwrap());

        matjson::Value obj = matjson::makeObject({{"a", nullptr}, {"b", 1}});
        CHECK(obj.dump(0) == R"({"a":null,"b":1})");
        // an entry is still reachable by key, and still knows it
        CHECK(obj.contains("a"));
        CHECK(obj["a"].isNull());
        CHECK(obj["a"].getKey().value() == "a");
        for (auto const& [key, value] : obj) {
            CHECK((key == "a" || key == "b"));
            (void)value;
        }
    }
}

TEST_CASE("Moved-from values") {
    SUBCASE("Standalone value becomes a plain null") {
        matjson::Value a = "hello";
        matjson::Value b = std::move(a);
        CHECK(b == "hello");
        CHECK(a.isNull());
        CHECK(!a.getKey().has_value());
        CHECK(a.dump(0) == "null");
        // and can be reused
        a = 12;
        CHECK(a == 12);
        a = std::move(b);
        CHECK(a == "hello");
        CHECK(b.isNull());
    }

    SUBCASE("Array elements survive reallocation") {
        std::vector<matjson::Value> vec;
        for (int i = 0; i < 1000; ++i) {
            vec.emplace_back(i);
            vec.emplace_back(std::to_string(i));
        }
        REQUIRE(vec.size() == 2000);
        for (int i = 0; i < 1000; ++i) {
            CHECK(vec[i * 2] == i);
            CHECK(vec[i * 2 + 1] == std::to_string(i));
        }
    }

    SUBCASE("Moved-from object entry keeps its key") {
        auto obj = matjson::parse(R"({"a": 1, "b": "two", "c": [3]})").unwrap();
        matjson::Value taken = std::move(obj["b"]);
        CHECK(taken == "two");
        CHECK(obj["b"].isNull());
        CHECK(obj["b"].getKey().value() == "b");

        std::vector<std::string> keys;
        for (auto const& [key, value] : obj) {
            keys.push_back(key);
        }
        CHECK(keys == std::vector<std::string>{"a", "b", "c"});
        CHECK(obj.dump(0) == R"({"a":1,"b":null,"c":[3]})");
    }

    SUBCASE("Assigning into an entry keeps the entry's key") {
        auto obj = matjson::parse(R"({"a": 1, "b": 2})").unwrap();
        matjson::Value n = nullptr;
        obj["a"] = n; // null copied over an entry
        CHECK(obj["a"].isNull());
        CHECK(obj["a"].getKey().value() == "a");

        obj["b"] = std::move(n); // plain null moved over an entry
        CHECK(obj["b"].isNull());
        CHECK(obj["b"].getKey().value() == "b");

        obj["a"] = 3; // and back to a real value
        CHECK(obj["a"] == 3);
        CHECK(obj["a"].getKey().value() == "a");
        CHECK(obj.dump(0) == R"({"a":3,"b":null})");
    }

    SUBCASE("Moving a keyed value over a standalone value clears the key") {
        auto obj = matjson::parse(R"({"a": 1})").unwrap();
        matjson::Value standalone = nullptr;
        standalone = std::move(obj["a"]);
        CHECK(standalone == 1);
        CHECK(!standalone.getKey().has_value());
        CHECK(obj["a"].getKey().value() == "a");
    }

    SUBCASE("Moving a value out of the container that is being assigned to") {
        matjson::Value v = matjson::parse(R"({"child": {"x": [1, 2, 3]}})").unwrap();
        v = std::move(v["child"]);
        CHECK(v.dump(0) == R"({"x":[1,2,3]})");

        matjson::Value w = matjson::parse(R"({"child": {"x": [1, 2, 3]}})").unwrap();
        w = w["child"];
        CHECK(w.dump(0) == R"({"x":[1,2,3]})");
    }

    SUBCASE("Setting with set() and operator[]") {
        matjson::Value obj;
        obj.set("n", nullptr);
        obj.set("s", "str");
        obj["m"] = nullptr;
        CHECK(obj.size() == 3);
        CHECK(obj.dump(0) == R"({"n":null,"s":"str","m":null})");
        CHECK(obj["n"].getKey().value() == "n");
        CHECK(obj["m"].getKey().value() == "m");

        obj.set("s", nullptr); // overwrite keeps position and key
        CHECK(obj.dump(0) == R"({"n":null,"s":null,"m":null})");
        CHECK(obj["s"].getKey().value() == "s");

        // existing key through operator[] returns the same entry
        auto& ref = obj["s"];
        CHECK(&ref == &obj["s"]);
        CHECK(obj.size() == 3);
    }

    SUBCASE("Dummy null is never modified") {
        matjson::Value obj;
        auto& dummy = std::as_const(obj)["missing"];
        matjson::Value moved = std::move(dummy);
        CHECK(moved.isNull());
        CHECK(dummy.isNull());
        CHECK(!dummy.getKey().has_value());

        matjson::Value number = 5;
        auto& writable = number["missing"]; // non-const access to a non-object gives the dummy too
        writable = 5; // assigning into the dummy is a no-op
        CHECK(std::as_const(obj)["missing"].isNull());
        CHECK(std::as_const(number)["missing"].isNull());
    }
}
