#pragma once

#include <algorithm>
#include <map>
#include <matjson.hpp>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>

namespace matjson {
    // allow converting parsing JSON directly to STL containers for convenience

    template <class T>
    struct Serialize<std::optional<T>> {
        static geode::Result<std::optional<T>> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<T>(); }
        {
            if (!value.isNull()) {
                return value.template as<T>().map([](T&& v) -> std::optional<T> {
                    return std::move(v);
                });
            }
            return geode::Ok(std::nullopt);
        }

        static Value toJson(std::optional<T> const& value)
            requires requires(T const& value) { Value(value); }
        {
            if (value.has_value()) {
                return Value(value.value());
            }
            return Value(nullptr);
        }
    };

    template <class T, class A>
    struct Serialize<std::vector<T, A>> {
        using Vector = std::vector<T, A>;

        static geode::Result<Vector> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<T>(); }
        {
            if (!value.isArray()) return geode::Err("not an array");
            Vector res;
            res.reserve(value.size());
            for (auto const& entry : value) {
                GEODE_UNWRAP_INTO(auto vv, entry.template as<T>());
                res.emplace_back(std::move(vv));
            }
            return geode::Ok(res);
        }

        static Value toJson(Vector const& value)
            requires requires(T const& value) { Value(value); }
        {
            std::vector<Value> res;
            res.reserve(value.size());
            std::transform(value.begin(), value.end(), std::back_inserter(res), [](T const& value) -> Value {
                return Value(value);
            });
            return res;
        }
    };

    template <class T, size_t Extent>
    struct Serialize<std::span<T, Extent>> {
        static Value toJson(std::span<T const, Extent> value)
            requires requires(T const& value) { Value(value); }
        {
            std::vector<Value> res;
            res.reserve(value.size());
            std::transform(value.begin(), value.end(), std::back_inserter(res), [](T const& value) -> Value {
                return Value(value);
            });
            return res;
        }
    };

    template <class T, class Hash, class KeyEqual, class Alloc>
    struct Serialize<std::unordered_set<T, Hash, KeyEqual, Alloc>> {
        using Set = std::unordered_set<T, Hash, KeyEqual, Alloc>;

        static geode::Result<Set> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<std::decay_t<T>>(); }
        {
            if (!value.isArray()) return geode::Err("not an array");
            Set res;
            res.reserve(value.size());
            for (auto const& entry : value) {
                GEODE_UNWRAP_INTO(auto vv, entry.template as<T>());
                res.insert(std::move(vv));
            }
            return geode::Ok(res);
        }

        static Value toJson(Set const& value)
            requires requires(T const& value) { Value(value); }
        {
            std::vector<Value> res;
            res.reserve(value.size());
            std::transform(value.begin(), value.end(), std::back_inserter(res), [](T const& value) -> Value {
                return Value(value);
            });
            return res;
        }
    };

    template <class T, class Compare, class Alloc>
    struct Serialize<std::set<T, Compare, Alloc>> {
        using Set = std::set<T, Compare, Alloc>;

        static geode::Result<Set> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<std::decay_t<T>>(); }
        {
            if (!value.isArray()) return geode::Err("not an array");
            Set res;
            for (auto const& entry : value) {
                GEODE_UNWRAP_INTO(auto vv, entry.template as<T>());
                res.insert(std::move(vv));
            }
            return geode::Ok(res);
        }

        static Value toJson(Set const& value)
            requires requires(T const& value) { Value(value); }
        {
            std::vector<Value> res;
            res.reserve(value.size());
            std::transform(value.begin(), value.end(), std::back_inserter(res), [](T const& value) -> Value {
                return Value(value);
            });
            return res;
        }
    };

    template <class T, class Compare, class Alloc>
    struct Serialize<std::map<std::string, T, Compare, Alloc>> {
        using Map = std::map<std::string, T, Compare, Alloc>;

        static geode::Result<Map> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<std::decay_t<T>>(); }
        {
            if (!value.isObject()) return geode::Err("not an object");
            Map res;
            for (auto const& [k, v] : value) {
                GEODE_UNWRAP_INTO(auto vv, v.template as<std::decay_t<T>>());
                res.insert({k, vv});
            }
            return geode::Ok(res);
        }

        static Value toJson(Map const& value)
            requires requires(T const& value) { Value(value); }
        {
            Value res;
            for (auto const& [k, v] : value) {
                res.set(k, Value(v));
            }
            return res;
        }
    };

    template <class T, class Hash, class KeyEqual, class Alloc>
    struct Serialize<std::unordered_map<std::string, T, Hash, KeyEqual, Alloc>> {
        using Map = std::unordered_map<std::string, T, Hash, KeyEqual, Alloc>;

        static geode::Result<Map> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<std::decay_t<T>>(); }
        {
            if (!value.isObject()) return geode::Err("not an object");
            Map res;
            for (auto const& [k, v] : value) {
                GEODE_UNWRAP_INTO(auto vv, v.template as<std::decay_t<T>>());
                res.insert({k, vv});
            }
            return geode::Ok(res);
        }

        static Value toJson(Map const& value)
            requires requires(T const& value) { Value(value); }
        {
            Value res;
            for (auto const& [k, v] : value) {
                res.set(k, Value(v));
            }
            return res;
        }
    };

    template <class T>
    struct Serialize<std::shared_ptr<T>> {
        static geode::Result<std::shared_ptr<T>> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<T>(); }
        {
            if (!value.isNull()) {
                return value.template as<T>().map([](T&& v) {
                    return std::make_shared<T>(std::move(v));
                });
            }
            return geode::Ok(nullptr);
        }

        static Value toJson(std::shared_ptr<T> const& value)
            requires requires(T const& value) { Value(value); }
        {
            if (value) {
                return Value(*value.get());
            }
            return Value(nullptr);
        }
    };

    template <class T>
    struct Serialize<std::unique_ptr<T>> {
        static geode::Result<std::unique_ptr<T>> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<T>(); }
        {
            if (!value.isNull()) {
                return value.template as<T>().map([](T&& v) {
                    return std::make_unique<T>(std::move(v));
                });
            }
            return geode::Ok(nullptr);
        }

        static Value toJson(std::unique_ptr<T> const& value)
            requires requires(T const& value) { Value(value); }
        {
            if (value) {
                return Value(*value.get());
            }
            return Value(nullptr);
        }
    };

    template <class T, size_t N>
    struct Serialize<std::array<T, N>> {
        static geode::Result<std::array<T, N>> fromJson(Value const& value)
            requires requires(Value const& value) { value.template as<T>(); }
        {
            if (!value.isArray()) return geode::Err("not an array");
            if (value.size() != N) return geode::Err("array must have size " + std::to_string(N));
            std::array<T, N> res{};
            for (size_t i = 0; i < N; ++i) {
                GEODE_UNWRAP_INTO(res[i], value[i].template as<T>());
            }
            return geode::Ok(res);
        }

        static matjson::Value toJson(std::array<T, N> const& value)
            requires requires(T const& value) { Value(value); }
        {
            std::vector<matjson::Value> res;
            res.reserve(N);
            std::transform(value.begin(), value.end(), std::back_inserter(res), [](T const& value) -> Value {
                return Value(value);
            });
            return res;
        }
    };
}
