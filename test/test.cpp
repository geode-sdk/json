#include <iostream>
#include <matjson3.hpp>

int main() {
    auto result = matjson::parse("[1,2,3,\"pen\\nis\"]");
    if (result.isErr()) {
        auto err = result.unwrapErr();
        std::cerr << "Error: " << err << std::endl;
    }
    else {
        auto value = result.unwrap();
        // std::cout << "Value: " << value << std::endl;

        std::cout << "it worked!\n";
        for (auto val : value) {
            std::cout << "Value type: " << (int)(val.type()) << std::endl;
        }
        std::cout << value.dump().unwrap() << std::endl;
    }

    auto json = matjson::parse("{\"hi\": 123}").unwrap();
    auto& [key, val] = json["hi"];
    std::cout << key << std::endl;
    val = matjson::Value("wow");
    std::cout << json.dump(0).unwrap() << std::endl;
}
