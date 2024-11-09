#include <fmt/core.h>
#include <fstream>
#include <matjson/reflect.hpp>
#include <matjson3.hpp>

using namespace geode;

struct Bar {
    int x = 10;
};

struct Foo {
    std::string name;
    int age;
    double height;
    Bar bar;
};

Result<void, std::string> fancyMain(int argc, char const* argv[]) {
#if 0
    {
        Foo bar{"John", 25, 1.75};
        matjson::Value json = bar;
        fmt::println("{}", GEODE_UNWRAP(json.dump()));
    }

    auto const json = GEODE_UNWRAP(matjson::parse("{\"hi\": 123.51}"));

    auto& x = json["wow"]["crazy"];
    matjson::Value test = 123.1;
    test = std::move(x);
    fmt::println("{}", json["hello"]["world"]["lol"]);
    fmt::println("{}", json);
    fmt::println("{}", GEODE_UNWRAP(json["hi"].asInt()));
    fmt::println("{}", GEODE_UNWRAP(json["hi"].asDouble()));

    auto mjson = json;
    mjson.set("hi", 123.5);
    mjson.set("oworld", "uwu");
    fmt::println("{}", mjson);
    mjson = 123;

    mjson = GEODE_UNWRAP(matjson::parse("{\"big number\": 123123123123123123}"));
    fmt::println("{}", mjson);
    fmt::println("{}", GEODE_UNWRAP(mjson["big number"].asInt()));
    fmt::println("{}", GEODE_UNWRAP(mjson["big number"].asUInt()));
    fmt::println("{}", GEODE_UNWRAP(mjson["big number"].asDouble()));

    if (argc > 1) {
        std::fstream file(argv[1]);

        auto const json = GEODE_UNWRAP(matjson::parse(file));

        fmt::println("{}", json.dump(matjson::NO_INDENTATION).unwrap());

        for (auto const& [key, _] : json) {
            fmt::println("Key: {}", key);
        }

        for (auto const& values : json) {
            fmt::println("{}", GEODE_UNWRAP(values.dump()));
        }

        auto result = json.as<Foo>();
        if (result) {
            fmt::println("it worked!");
        }
        else {
            fmt::println("it failed: {}", result.unwrapErr());
        }
    }
#endif
    GEODE_UNWRAP(matjson::parse(""));
    return Ok();
}

int main(int argc, char const* argv[]) {
    auto result = fancyMain(argc, argv);
    if (result) {
        return 0;
    }
    fmt::println(stderr, "main failed: {}", result.unwrapErr());
    return 1;
}