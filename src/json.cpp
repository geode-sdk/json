#include <json.hpp>
#include <stdexcept>

// macos does not seem to support charconv.. so do this as a workaround
#ifndef __APPLE__
#include <charconv>
#include <array>
#else
#include <sstream>
#endif

using namespace json;
using namespace std::literals::string_view_literals;

// Implementation for ValueImpl, which actually stores the value
// in a std::variant
#include "impl.hpp"
// Parsing functions
#include "parser.hpp"
// Implementation for Value
#include "value.hpp"
// Implementation for Object, a map like wrapper for std::vector
#include "object.hpp"
