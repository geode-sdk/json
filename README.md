# MATjson

json library i made quickly for geode. preserves insertion order for objects

- this was made to be easy to compile
- numbers are internally stored as double
- not made to handle extremely nested objects

## Example

```cpp
#include <json.hpp>

auto object = json::parse(R"(
    {
        "hello": "world",
        "foo": {
            "bar": 2000
        }
    }
)");

object["hello"].as_string(); // world
object["foo"]["bar"].as_int(); // 2000
object.is_object(); // true
object.is_string(); // false
```

# Usage

## Loading from a file
For the sake of simplicty, the library can only parse from a string, so you will have to load the whole file in memory yourself.

```cpp
std::string file_contents = ...

auto object = json::parse(file_contents);
```

## Saving to a file
The library can also only directly output a string.
```cpp
json::Value object = ...

std::string file_contents = object.dump();
write_to_file("epic.json", file_contents);
```
`Value::dump` can also control identation, see [Dumping to string](#dumping-to-string)

## Accessing values
There are many different ways to access inner values, depending on your needs:
```cpp
auto object = json::parse(R"(
    {
        "hello": "world",
        "foo": {
            "bar": 2000
            "numbers": [123, false, "not a number actually"]
        }
    }
)");

object["hello"].as_string(); // "world"
object["hello"].as<std::string>(); // "world"
object.get<std::string>("hello"); // "world"

object["foo"]["bar"].as_int(); // 2000
object["foo"]["bar"].as<int>(); // 2000
object["foo"].get<int>("bar"); // 2000

object["foo"]["numbers"][0].as_int(); // 123
object["foo"]["numbers"][0].as<int>(); // 123
object["foo"]["numbers"].get<int>(0); // 123

object.try_get("foo"); // std::optional<json::Value>(...)
object.try_get("sup"); // std::nullopt
object.contains("sup"); // false

// will insert "sup" into object as long as its not const.
// behaves like std::map::operator[]
object["sup"];
object["nya"] = 123;

object["hello"].is_string(); // true
object["hello"].is<std::string>(); // true
```

## Custom types
It is possible (de)serialize your own types into json, by specializing `json::Serialize<T>`.
```cpp
struct User {
    std::string name;
    int age;
}

template <>
struct json::Serialize<User> {
    static User from_json(const json::Value& value) {
        return User {
            .name = value["name"].as_string(),
            .age = value["age"].as_int(),
        };
    }
    static json::Value to_json(const User& user) {
        return json::Object {
            { "name", user.name },
            { "age", user.age }
        };
    }
    // You can also implement this method:
    // > static bool is_json(const json::Value& value);
    // It is only used if you do value.is<User>();
};

User user = { "mat", 123 };
json::Value value = user; // gets converted into json
value.dump(json::NO_INDENTATION); // {"name":"mat","age":123}
value["name"] = "hello";
// you have to use the templated methods for accessing custom types!
auto user2 = value.as<User>();
user2.name; // "hello"
```

## Dumping to string
To save a json value to a file or to pretty print it you can use the `json::Value::dump` method.
```cpp
// from the previous section
json::Value value = User { "mat", 123 };

// by default dumps with 4 spaces
value.dump();
// {
//     "name": "mat",
//     "age": 123    
// }

value.dump(1); // 1 space identation
// {
//  "name": "mat",
//  "age": 123    
// }

// will create a compact json with no extra whitepace
value.dump(json::NO_INDENTATION);
value.dump(0); // same as above
// {"name":"mat","age":123}

value.dump(json::TAB_INDENTATION); // will indent with tabs
// like imagine '{\n\t"name": "mat",\n\t"age": 123\n}'
```

## misc
```cpp
// default constructs to a json object, for convenience
json::Value value;

// null is nullptr
value["hello"] = nullptr;
value["hello"].is_null(); // true
```

## ok thats like everything
just look at the main header for other things its not that hard to read the code