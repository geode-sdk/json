#pragma once

#include "linkage.hpp"
#include "matjson.hpp"

#include <cstddef>
#include <iterator>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace matjson {
    namespace detail {
        using ArrayIter = std::vector<Value>::iterator;
        using MapIter = std::vector<std::pair<std::string, Value>>::iterator;

        using ConstArrayIter = std::vector<Value>::const_iterator;
        using ConstMapIter = std::vector<std::pair<std::string, Value>>::const_iterator;
    }

    class ValueIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Value;
        using difference_type = std::ptrdiff_t;
        using pointer = Value*;
        using reference = Value&;

        ValueIterator() = default;
        ~ValueIterator() = default;

        ValueIterator(ValueIterator const&) = default;
        ValueIterator(ValueIterator&&) noexcept = default;
        ValueIterator& operator=(ValueIterator const&) = default;
        ValueIterator& operator=(ValueIterator&&) noexcept = default;

        Value& operator*() const {
            if (auto ptr = std::get_if<0>(&data)) {
                return **ptr;
            }
            else {
                auto& x = **std::get_if<1>(&data);
                return x.second;
            }
        }

        Value* operator->() const {
            return &**this;
        }

        ValueIterator& operator++() {
            if (auto ptr = std::get_if<0>(&data)) {
                ++*ptr;
            }
            else {
                ++*std::get_if<1>(&data);
            }
            return *this;
        }

        ValueIterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(ValueIterator const& other) const {
            if (data.index() != other.data.index()) return false;
            if (auto ptr = std::get_if<0>(&data)) return *ptr == *std::get_if<0>(&other.data);
            return *std::get_if<1>(&data) == *std::get_if<1>(&other.data);
        }

        bool operator!=(ValueIterator const& other) const {
            return !(*this == other);
        }

    private:
        std::variant<detail::ArrayIter, detail::MapIter> data = detail::ArrayIter{};

        MAT_JSON_DLL ValueIterator(ValueImpl*, bool begin);
        friend Value;
    };

    class ConstValueIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Value;
        using difference_type = std::ptrdiff_t;
        using pointer = Value const*;
        using reference = Value const&;

        ConstValueIterator() = default;
        ~ConstValueIterator() = default;

        ConstValueIterator(ConstValueIterator const&) = default;
        ConstValueIterator(ConstValueIterator&&) noexcept = default;
        ConstValueIterator& operator=(ConstValueIterator const&) = default;
        ConstValueIterator& operator=(ConstValueIterator&&) noexcept = default;

        Value const& operator*() const {
            if (auto ptr = std::get_if<0>(&data)) {
                return **ptr;
            }
            else {
                auto& x = **std::get_if<1>(&data);
                return x.second;
            }
        }

        Value const* operator->() const {
            return &**this;
        }

        ConstValueIterator& operator++() {
            if (auto ptr = std::get_if<0>(&data)) {
                ++*ptr;
            }
            else {
                ++*std::get_if<1>(&data);
            }
            return *this;
        }

        ConstValueIterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(ConstValueIterator const& other) const {
            if (data.index() != other.data.index()) return false;
            if (auto ptr = std::get_if<0>(&data)) return *ptr == *std::get_if<0>(&other.data);
            return *std::get_if<1>(&data) == *std::get_if<1>(&other.data);
        }

        bool operator!=(ConstValueIterator const& other) const {
            return !(*this == other);
        }

    private:
        std::variant<detail::ConstArrayIter, detail::ConstMapIter> data = detail::ConstArrayIter{};

        MAT_JSON_DLL ConstValueIterator(ValueImpl*, bool begin);
        friend Value;
    };
}
