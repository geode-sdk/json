# MATjson

JSON library made for [Geode](https://github.com/geode-sdk/json). Main feature is preserving insertion order, along with the use of [Geode's Result library](https://github.com/geode-sdk/result).

## Example

```cpp
#include <matjson.hpp>

auto result = matjson::parse(R"(
    {
        "hello": "world",
        "foo": {
            "bar": 2000
        }
    }
)");
if (!result) {
    println("Failed to parse json: {}", result.unwrapErr());
}
matjson::Value object = result.unwrap();

object["hello"].asString().unwrap(); // world
object["foo"]["bar"].asInt().unwrap(); // 2000
object.isObject(); // true
object.isString(); // false
```

# Index

* [Usage](#usage)
    * [Parsing JSON](#parsing-json)
    * [Dumping JSON](#dumping-json)
    * [Accessing object properties](#accessing-object-properties)
    * [Serializing custom types](#serializing-custom-types)
    * [Objects and arrays](#objects-and-arrays)
    * [Reflection](#reflection)
    * [Support for standard library types](#support-for-standard-library-types)

# Usage

The main class of the library is `matjson::Value`, which can represent any JSON value. You can access its type by using the `type()` method, or one of the `isString()` methods.
```cpp
matjson::Value value = ...;

matjson::Type type = value.type();
if (type == matjson::Type::Object) {
    // ...
}
// same as
if (value.isObject()) {
    // ...
}
```

Anything not documented here can be found in the [matjson.hpp](https://github.com/geode-sdk/json/blob/main/include/matjson.hpp) header, which is also well documented.

## Parsing JSON

To parse JSON into a `matjson::Value`, you can simply use the `matjson::parse` method

```cpp
Result<matjson::Value, matjson::ParseError> matjson::parse(std::string_view str);
Result<matjson::Value, matjson::ParseError> matjson::parse(std::istream& stream);
```
```cpp
GEODE_UNWRAP_INTO(matjson::Value value, matjson::parse("[1, 2, 3, 4]"));

std::ifstream file("file.json");
GEODE_UNWRAP_INTO(matjson::Value value2, matjson::parse(file));
```

## Dumping JSON

To convert a `matjson::Value` back to a JSON string, use `dump()`
```cpp
/// Dumps the JSON value to a string, with a given indentation.
/// If the given indentation is matjson::NO_INDENTATION, the json is compacted.
/// If the given indentation is matjson::TAB_INDENTATION, the json is indented with tabs.
/// @param indentationSize The number of spaces to use for indentation
/// @return The JSON string
/// @note Due to limitations in the JSON format, NaN and Infinity float values get converted to null.
///       This behavior is the same as many other JSON libraries.
std::string Value::dump(int indentation = 4);
```
```cpp
matjson::Value object = matjson::makeObject({
    {"x", 10}
});

std::string jsonStr = object.dump();
// {
//     "x": 10
// }

std::string compact = object.dump(matjson::NO_INDENTATION);
// {"x":10}
```
`Value::dump` can also control indentation, see [Dumping to string](#dumping-to-string)

## Accessing object properties

There are many different ways to access inner properties
```cpp
Result<Value&> Value::get(std::string_view key);
Result<Value&> Value::get(std::size_t index);
// These are special!
Value& Value::operator[](std::string_view key);
Value& Value::operator[](std::size_t index);
```
As noted, `operator[]` is special because:
* If the `matjson::Value` is not const, `operator[](std::string_view key)` will insert into the object, akin to `std::map`. \
(This does not apply if the value is an array)
* Otherwise, trying to access missing values will return a special `null` JSON value. This value cannot be mutated.
```cpp
matjson::Value object = matjson::parse(R"(
    {
        "hello": "world",
        "foo": {
            "bar": 2000
            "numbers": [123, false, "not a number actually"]
        }
    }
)").unwrap();

object["hello"]; // matjson::Value("world")
object.get("hello"); // Ok(matjson::Value("world"))

object["foo"]["bar"].asInt(); // Ok(2000)

object.contains("sup"); // false

// will insert "sup" into object as long as its not const.
// behaves like std::map::operator[]
object["sup"];
object["nya"] = 123;

// not the case for .get
object.get("something else"); // Err("key not found")
```

## Serializing custom types

It's possible to (de)serialize your own types into json, by specializing `matjson::Serialize<T>`.
```cpp
#include <matjson.hpp>

struct User {
    std::string name;
    int age;
};

template <>
struct matjson::Serialize<User> {
    static geode::Result<User> fromJson(const matjson::Value& value) {
        GEODE_UNWRAP_INTO(std::string name, value["name"].asString());
        GEODE_UNWRAP_INTO(int age, value["age"].asInt());
        return geode::Ok(User { name, age });
    }
    static matjson::Value toJson(const User& user) {
        return matjson::makeObject({
            { "name", user.name },
            { "age", user.age }
        });
    }
};

int main() {
    User user = { "mat", 123 };

    // gets implicitly converted into json
    matjson::Value value = user;

    value.dump(matjson::NO_INDENTATION); // {"name":"mat","age":123}

    value["name"] = "hello";
    // you have to use the templated methods for accessing custom types!
    // it will *not* implicitly convert to User
    User user2 = value.as<User>().unwrap();
    user2.name; // "hello"
}
```

Serialization is available for many standard library containers in `#include <matjson/std.hpp>`, as mentioned in [Support for standard library types](#support-for-standard-library-types).

There is also experimental support for [reflection](#reflection).

## Objects and arrays

Objects and arrays have special methods, since they can be iterated and mutated.
```cpp
// Does nothing is Value is not an object
void Value::set(std::string_view key, Value value);
// Does nothing if Value is not an array
void Value::push(Value value);
// Clears all entries from an array or object
void Value::clear();
// Does nothing if Value is not an object
bool Value::erase(std::string_view key);
// Returns 0 if Value is not an object or array
std::size_t Value::size() const;
```
To make objects you can use the `matjson::makeObject` function, and for arrays you can use the `Value::Value(std::vector<matjson::Value>)` constructor.
```cpp
matjson::Value obj = matjson::makeObject({
    {"hello", "world"},
    {"number", "123"}
});

for (auto& [key, value] : obj) {
    // key is std::string
    // value is matjson::Value
}

// just iterates through the values
for (auto& value : obj) {
    // value is matjson::Value
}

obj.set("foo", true);

matjson::Value arr({ 1, 2, "hello", true });

for (auto& value : obj) {
    // values
}

arr.push(nullptr);
arr.dump(matjson::NO_INDENTATION); // [1,2,"hello",true,null]

// If you need direct access to the vector, for some reason
std::vector<matjson::Value>& vec = arr.asArray().unwrap();
```

## Reflection

The library now has experimental support for reflection using the [qlibs/reflect](https://github.com/qlibs/reflect) library, which only supports [aggregate types](https://en.cppreference.com/w/cpp/language/aggregate_initialization).

Including the optional header will define JSON serialization for any type it can, making them able to be used with matjson instantly

```cpp
#include <matjson/reflect.hpp>

struct Stats {
    int hunger;
    int health;
};

struct User {
    std::string name;
    Stats stats;
    bool registered;
};

int main() {
    User user = User {
        .name = "Joe",
        .stats = Stats {
            .hunger = 0,
            .health = 100
        },
        .registered = true
    };

    matjson::Value value = user;

    value.dump();
    // {
    //     "name": "Joe",
    //     "stats": {
    //         "hunger": 0,
    //         "health": 0
    //     },
    //     "registered": true
    // }

    User u2 = value.as<User>().unwrap();
}
```

## Support for standard library types

There is built in support for serializing std containers by including an optional header:

Supported classes (given T is serializable):
* `std::vector<T>`
* `std::span<T>`
* `std::map<std::string, T>`
* `std::unordered_map<std::string, T>`
* `std::set<T>`
* `std::unordered_set<T>`
* `std::optional<T>` - `null` is prioritized as `std::nullopt`
* `std::shared_ptr<T>` - same as above
* `std::unique_ptr<T>` - same as above

```cpp
#include <matjson/std.hpp>

std::vector<int> nums = { 1, 2, 3 };
std::map<std::string, std::string> map = {
    { "hello", "foo" }
};

matjson::Value jsonNums = nums;
matjson::Value jsonMap = map;
```
