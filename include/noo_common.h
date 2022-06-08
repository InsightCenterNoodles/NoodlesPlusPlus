#pragma once

#include <span>
#include <string_view>
#include <unordered_map>
#include <variant>

#include <QString>

namespace noo {

// common strings as defined by the spec

namespace names {

#define NOO_STRING(t, d) inline QString t##d = QStringLiteral(#d);

NOO_STRING(mthd_, tbl_subscribe);
NOO_STRING(mthd_, tbl_insert);
NOO_STRING(mthd_, tbl_update);
NOO_STRING(mthd_, tbl_remove);
NOO_STRING(mthd_, tbl_clear);
NOO_STRING(mthd_, tbl_update_selection);

NOO_STRING(sig_, tbl_reset);
NOO_STRING(sig_, tbl_updated);
NOO_STRING(sig_, tbl_rows_removed);
NOO_STRING(sig_, tbl_selection_updated);

NOO_STRING(mthd_, activate);
NOO_STRING(mthd_, get_activation_choices);

NOO_STRING(mthd_, get_option_choices);
NOO_STRING(mthd_, get_current_option);
NOO_STRING(mthd_, set_current_option);

NOO_STRING(mthd_, set_position);
NOO_STRING(mthd_, set_rotation);
NOO_STRING(mthd_, set_scale);

NOO_STRING(mthd_, select_region);
NOO_STRING(mthd_, select_sphere);
NOO_STRING(mthd_, select_half_plane);
NOO_STRING(mthd_, select_hull);

NOO_STRING(mthd_, probe_at);

NOO_STRING(mthd_, signal_attention);

NOO_STRING(mthd_, noo_client_view);

NOO_STRING(tag_, noo_user_hidden);


NOO_STRING(hint_, any);
NOO_STRING(hint_, text);
NOO_STRING(hint_, integer);
NOO_STRING(hint_, integerlist);
NOO_STRING(hint_, real);
NOO_STRING(hint_, reallist);
NOO_STRING(hint_, data);
NOO_STRING(hint_, list);
NOO_STRING(hint_, map);
NOO_STRING(hint_, anyid);
NOO_STRING(hint_, objectid);
NOO_STRING(hint_, tableid);
NOO_STRING(hint_, signalid);
NOO_STRING(hint_, methodid);
NOO_STRING(hint_, materialid);
NOO_STRING(hint_, geometryid);
NOO_STRING(hint_, lightid);
NOO_STRING(hint_, textureid);
NOO_STRING(hint_, bufferid);
NOO_STRING(hint_, plotid);

#undef NOO_STRING
} // namespace names


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


/// Attempt to get a value from a span at a certain index. If the index does
/// not exist, returns the default value.
template <class T>
T get_or_default(std::span<T> const& t, size_t i, T const& def = T()) {
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


template <class Iter1, class Iter2>
size_t copy(Iter1 src_begin, Iter1 src_end, Iter2 dest_begin, Iter2 dest_end) {
    auto size1 = std::distance(src_begin, src_end);
    auto size2 = std::distance(dest_begin, dest_end);

    auto to_copy = std::min(size1, size2);

    std::copy_n(src_begin, to_copy, dest_begin);

    return to_copy;
}

template <class C1, class C2>
size_t copy_range(C1 const& src, C2& dest) {
    auto size1 = std::size(src);
    auto size2 = std::size(dest);

    auto to_copy = std::min(size1, size2);

    std::copy_n(std::begin(src), to_copy, std::begin(dest));

    return to_copy;
}

template <class T>
auto span_to_vector(std::span<T> sp) {
    using R = std::remove_cvref_t<T>;
    return std::vector<R>(sp.begin(), sp.end());
}

///
/// \brief By the standard, no checks are made for a subspan offset or count (if
/// not max). The span returned here will always be defined, and may have less
/// items than requested.
///
template <class T>
auto safe_subspan(std::span<T> sp,
                  size_t       offset,
                  size_t       count = std::dynamic_extent) {
    if (offset >= sp.size()) return std::span<T>();

    size_t available = sp.size() - offset;

    count = (count <= available ? count : available);

    return sp.subspan(offset, count);
}

// =============================================================================

enum ErrorCodes {
    // defined by the spec
    PARSE_ERROR      = -32700,
    INVALID_REQUEST  = -32600,
    METHOD_NOT_FOUND = -32601,
    INVALID_PARAMS   = -32602,
    INTERNAL_ERROR   = -32603,


    // defined by this library
    TABLE_REJECT_INSERT = -40000,
    TABLE_REJECT_UPDATE = -40001,
    TABLE_REJECT_REMOVE = -40002,
    TABLE_REJECT_CLEAR  = -40003,

    TABLE_REJECT_SELECTION_UPDATE = -40100
};


enum class Format : uint8_t {
    U8,
    U16,
    U32,

    U8VEC4,

    U16VEC2,

    VEC2,
    VEC3,
    VEC4,

    MAT3,
    MAT4,
};

enum class PrimitiveType : uint8_t {
    POINTS,
    LINES,
    LINE_LOOP,
    LINE_STRIP,
    TRIANGLES,
    TRIANGLE_STRIP,
    TRIANGLE_FAN // Not recommended, some hardware support is lacking
};

enum class AttributeSemantic : uint8_t {
    POSITION, // for the moment, must be a vec3.
    NORMAL,   // for the moment, must be a vec3.
    TANGENT,  // for the moment, must be a vec3.
    TEXTURE,  // for the moment, is either a vec2, or normalized u16vec2
    COLOR,    // normalized u8vec4, or vec4
};

} // namespace noo
