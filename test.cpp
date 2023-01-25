#include <json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

std::string read_file(std::string filepath) {
    std::ifstream is { filepath, std::ios::binary };
    std::stringstream ss;
    ss << is.rdbuf();
    is.close();
    return std::move(ss).str();
}

int main() {
    json::Value foo = json::ValueObject {
        {"hello", 69}
    };

    foo.debug(); std::cout << std::endl;

    foo["world"] = 31;

    foo.debug(); std::cout << std::endl;

    json::Value bar;

    bar = foo;

    bar["hello"] = 10;

    foo.debug(); std::cout << std::endl;
    bar.debug(); std::cout << std::endl;
}