#ifndef INTERFACE_TYPES_H
#define INTERFACE_TYPES_H

#include "noo_any.h"
#include "noo_anyref.h"
#include "noo_id.h"
#include "noo_include_glm.h"

#include <glm/gtc/type_ptr.hpp>

#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QColor>

#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>


namespace noo {

// convert to a CBOR

QVector<double>  coerce_to_real_list(QCborValue);
QVector<int64_t> coerce_to_int_list(QCborValue);


// Convert from a CBOR

template <class...>
constexpr std::false_type always_false {};

template <class T>
QCborValue convert_to_cbor(T);

template <class T>
bool convert_from_cbor(QCborValue v, T& t) {
    if constexpr (std::is_constructible_v<T, QCborMap>) {
        t = T(v.toMap());
        return true;
    } else {
        static_assert(always_false<T>);
    }
}

inline bool convert_from_cbor(QCborValue v, QCborArray& array) {
    array = v.toArray();
    return true;
}

inline bool convert_from_cbor(QCborValue v, QString& s) {
    s = v.toString();
    return true;
}

inline bool convert_from_cbor(QCborValue v, glm::vec4& s) {
    auto arr = v.toArray();
    s.x      = arr[0].toDouble(1);
    s.y      = arr[1].toDouble(1);
    s.z      = arr[2].toDouble(1);
    s.w      = arr[3].toDouble(1);
    return true;
}

inline bool convert_from_cbor(QCborValue v, QColor& s) {
    glm::vec4 col;
    convert_from_cbor(v, col);
    s = QColor(col.r, col.g, col.b, col.a);
    return true;
}

template <class T>
bool convert_from_cbor(QCborValue v, QVector<T>& out) {
    out.clear();
    auto arr = v.toArray();
    for (auto const& arr_v : arr) {
        T item;
        if (!convert_from_cbor(arr_v, item)) return false;
        out.push_back(std::move(item));
    }
}

template <class T>
bool convert_from_cbor(QCborValue v, std::vector<T>& out) {
    out.clear();
    auto arr = v.toArray();
    for (auto const& arr_v : arr) {
        T item;
        if (!convert_from_cbor(arr_v, item)) return false;
        out.push_back(std::move(item));
    }
}

template <class T>
bool convert_from_cbor(QCborValue v, std::optional<T>& out) {
    out.reset();
    if (v.isUndefined()) { return true; }
    T& lt = out.emplace();
    return convert_from_cbor(v, lt);
}

struct CborDecoder {
    QCborMap map;

    CborDecoder() = default;
    CborDecoder(QCborMap const& m) : map(m) { }

    template <class T>
    bool operator()(QString n, T& t) {
        using namespace noo;
        return convert_from_cbor(map[n], t);
    }

    template <class T>
    bool conditional(QString n, T& t) {
        using namespace noo;
        if (map.contains(n)) { return convert_from_cbor(map[n], t); }
        return true;
    }
};

// =============================================================================

///
/// \brief The Selection struct models a noodles SelectionObject
///
struct Selection {
    using Pair = std::pair<int64_t, int64_t>;
    static_assert(sizeof(Pair) == 2 * sizeof(int64_t));

    std::vector<int64_t> rows;
    std::vector<Pair>    row_ranges;

    Selection() = default;
    Selection(QCborMap);

    QCborValue to_any() const;
};

// =============================================================================

//@{
/// Ask if a type is a possible member of a variant at compile time
template <class T, class Var>
struct is_in_variant;

template <class T, class... Args>
struct is_in_variant<T, std::variant<Args...>>
    : std::bool_constant<(std::is_same_v<T, Args> || ...)> { };
//@}

//@{
/// Ask if a type is a std::vector of some kind
template <class T>
struct is_vector : std::bool_constant<false> { };

template <class T>
struct is_vector<std::vector<T>> : std::bool_constant<true> { };
//@}

//@{
/// Ask if a type is a std::span of some kind
template <class T>
struct is_span : std::bool_constant<false> { };

template <class T>
struct is_span<std::span<T>> : std::bool_constant<true> { };
//@}

// =============================================================================
// These utils help with creating methods

///
/// \brief The RealListArg struct is used in methods to coerce the Any type
/// to turn a real-list, a list of any-reals or list of any-ints into a list of
/// reals.
///
struct AnyListArg {
    QCborArray list;

    AnyListArg() = default;
    AnyListArg(QCborValue const& a) : list(a.toArray()) { }
};

///
/// \brief The RealListArg struct is used in methods to coerce the Any type
/// to turn a real-list, a list of any-reals or list of any-ints into a list of
/// reals.
///
struct RealListArg {
    QVector<double> list;

    RealListArg() = default;
    RealListArg(QCborValue const& a) : list(coerce_to_real_list(a)) { }
};

///
/// \brief The IntListArg struct is used in methods to coerce an any type into a
/// list of integers.
///
struct IntListArg {
    QVector<int64_t> list;

    IntListArg() = default;
    IntListArg(QCborValue const& a) : list(coerce_to_int_list(a)) { }
};

///
/// \brief The StringListArg struct is used in methods to coerce an any into a
/// list of strings
///
struct StringListArg {
    QStringList list;

    StringListArg() = default;
    StringListArg(QCborValue const& a);
};


struct Vec3Arg : std::optional<glm::vec3> {
    Vec3Arg() = default;
    Vec3Arg(QCborValue const&);
};

struct Vec3ListArg : std::vector<glm::vec3> {
    Vec3ListArg() = default;
    Vec3ListArg(QCborValue const&);
};

struct Vec4Arg : std::optional<glm::vec4> {
    Vec4Arg() = default;
    Vec4Arg(QCborValue const&);
};

struct IntArg : std::optional<int64_t> {
    IntArg() = default;
    IntArg(QCborValue const&);
};

struct BoolArg {
    char state = -1;

    BoolArg() = default;
    BoolArg(QCborValue const&);

    operator bool() const { return state >= 0; }

    bool operator*() const { return state == 1; }
};

struct BoundingBox {
    glm::vec3 aabb_min;
    glm::vec3 aabb_max;
};


} // namespace noo

#endif // INTERFACE_TYPES_H
