#pragma once
#include <nlohmann/json.hpp>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

// nlohmann's json::value(key, default) throws type_error when the key exists
// with a type different from the requested one, and only falls back to the
// default when the key is missing. World docs are user-authored JSON edited in
// MudAdmin, so every doc lookup must fall back instead of crashing the server.
// Use SafeValue(json, key, default) anywhere the JSON may contain
// admin-authored content. A present-but-wrong-typed field is an authoring
// error, so it warns while still falling back.
namespace detail {

template<typename T>
using SafeValueType = std::conditional_t<
    std::is_array_v<std::remove_cv_t<std::remove_reference_t<T>>>,
    std::string,
    std::decay_t<T>>;

} // namespace detail

template<typename T>
auto SafeValue(const nlohmann::json& j, std::string_view key, T&& defaultValue)
    -> detail::SafeValueType<T> {
    using V = detail::SafeValueType<T>;
    const auto it = j.find(std::string(key));
    if (it == j.end() || it->is_null()) {
        return V(std::forward<T>(defaultValue));
    }
    try {
        return it->get<V>();
    } catch (const nlohmann::json::exception&) {
        std::fprintf(stderr, "[SafeJson] field '%s' has unexpected type (%s), using default\n",
                     std::string(key).c_str(), it->type_name());
        return V(std::forward<T>(defaultValue));
    }
}
