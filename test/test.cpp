#include <iostream>
#include <matjson3.hpp>

int main() {
    auto json = matjson::parse("{\"hi\": 123}").unwrap();

    auto& x = json["wow"]["crazy"];
    matjson::Value test = 123.1;
    test = std::move(x);
    std::cout << (json["hello"]["world"]["lol"].dump().unwrap()) << std::endl;
}
