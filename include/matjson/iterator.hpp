#pragma once

#include "linkage.hpp"

#include <cstddef>
#include <iterator>
#include <memory>

namespace matjson {
    class Value;
    class ValueImpl;

    class MAT_JSON_DLL ValueIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Value;
        using difference_type = std::ptrdiff_t;
        using pointer = Value*;
        using reference = Value&;

        ValueIterator();
        ValueIterator(ValueIterator const&);
        ValueIterator(ValueIterator&&) noexcept;
        ValueIterator& operator=(ValueIterator const&);
        ValueIterator& operator=(ValueIterator&&) noexcept;
        ~ValueIterator();

        Value& operator*() const;
        Value* operator->() const;
        ValueIterator& operator++();
        ValueIterator operator++(int);
        bool operator==(ValueIterator const& other) const;
        bool operator!=(ValueIterator const& other) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        ValueIterator(ValueImpl*, bool begin);

        Impl* impl();
        Impl const* impl() const;
        friend Value;
        friend Impl;
    };

    class MAT_JSON_DLL ConstValueIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Value;
        using difference_type = std::ptrdiff_t;
        using pointer = Value const*;
        using reference = Value const&;

        ConstValueIterator();
        ConstValueIterator(ConstValueIterator const&);
        ConstValueIterator(ConstValueIterator&&) noexcept;
        ConstValueIterator& operator=(ConstValueIterator const&);
        ConstValueIterator& operator=(ConstValueIterator&&) noexcept;
        ~ConstValueIterator();

        Value const& operator*() const;
        Value const* operator->() const;
        ConstValueIterator& operator++();
        ConstValueIterator operator++(int);
        bool operator==(ConstValueIterator const& other) const;
        bool operator!=(ConstValueIterator const& other) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        ConstValueIterator(ValueImpl*, bool begin);

        Impl* impl();
        Impl const* impl() const;
        friend Value;
        friend Impl;
    };
}
