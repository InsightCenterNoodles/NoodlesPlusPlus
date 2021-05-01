#ifndef COMMON_H
#define COMMON_H

#include <span>
#include <string_view>
#include <unordered_map>
#include <variant>

class QString;

namespace noo {

/// Attempt to find and move a type out of a variant. If the type is not active
/// in the variant, returns a default constructed instance. The variant will
/// still consider that type active, afterwards, but in a moved-from state.
template <class T, class... Args>
T steal_or_default(std::variant<Args...>& v) {
    T* t = std::get_if<T>(&v);
    if (!t) { return T(); }
    return std::move(*t);
}

/// Attempt to move a value of of a vector. If the index is out of bounds,
/// return a default constructed instance. The vector will still have a value at
/// that index, but it will be in a moved-from state.
template <class T>
T steal_or_default(std::vector<T>& t, size_t i) {
    if (i < t.size()) { return std::move(t[i]); }
    return T();
}

/// Attempt to move the value out of a given key from a map. If the key does not
/// exist, it will return a default constructed value. The key and value will
/// still remain in the map, but value will be in a moved-from state.
template <class K, class V, class U>
V steal_or_default(std::unordered_map<K, V>& v, U const& key) {
    auto iter = v.find(key);
    if (iter == v.end()) { return V(); }
    return std::move(iter->second);
}

/// Attempt to get a type from a variant. If the type is not active in the
/// variant, returns the default value.
template <class T, class... Args>
T get_or_default(std::variant<Args...> const& v, T const& def = T()) {
    T const* t = std::get_if<T>(&v);
    if (!t) { return def; }
    return *t;
}

/// Attempt to get a value from a vector at a certain index. If the index does
/// not exist, returns the default value.
template <class T>
T get_or_default(std::vector<T> const& t, size_t i, T const& def = T()) {
    if (i < t.size()) { return t[i]; }
    return def;
}

QString to_qstring(std::string_view);

template <class U, class T>
std::span<U> cast_span_to(std::span<T> s) {
    static_assert(
        std::max(sizeof(T), sizeof(U)) % std::min(sizeof(T), sizeof(U)) == 0);
    static_assert(std::is_standard_layout_v<T>);
    static_assert(std::is_standard_layout_v<U>);


    return std::span<U>(reinterpret_cast<U*>(s.data()),
                        s.size_bytes() / sizeof(U));
}

} // namespace noo

#endif // COMMON_H
