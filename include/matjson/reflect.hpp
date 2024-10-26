#pragma once

#include "external/reflect"

#include <matjson3.hpp>

template <class T>
    requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
struct matjson::Serialize<T> {
    static geode::Result<T, std::string_view> fromJson(matjson::Value const& json) {
        T value;
        std::optional<std::string_view> error;
        reflect::for_each<T>([&](auto N) {
            if (error) return;
            auto& field = reflect::get<N>(value);

            auto result =
                json[reflect::member_name<N>(value)].template as<std::remove_cvref_t<decltype(field)>>(
                );
            if (result) {
                field = result.unwrap();
            }
            else {
                error = "field missing";
            }
        });
        if (error) return geode::Err(*error);
        return geode::Ok(value);
    }

    static matjson::Value toJson(T const& value) {
        matjson::Value json;
        reflect::for_each<T>([&](auto N) {
            auto const& field = reflect::get<N>(value);
            json[reflect::member_name<N>(value)] = matjson::Value(field);
        });
        return json;
    }
};
