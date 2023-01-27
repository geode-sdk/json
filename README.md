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
        "foo" : {
            "bar": 2000
        }
    }
)");

object["hello"].as_string(); // world
object["foo"]["bar"].as_int(); // 2000
object.is_object(); // true
object.is_string(); // false
```
