#ifndef INTERFACE_TYPES_H
#define INTERFACE_TYPES_H

#include "noo_any.h"
#include "noo_anyref.h"
#include "noo_id.h"
#include "noo_include_glm.h"

#include <glm/gtc/type_ptr.hpp>

#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>


namespace noo {

///
/// \brief The Selection struct models a noodles SelectionObject
///
struct Selection {
    using Pair = std::pair<int64_t, int64_t>;
    static_assert(sizeof(Pair) == 2 * sizeof(int64_t));

    std::vector<int64_t> rows;
    std::vector<Pair>    row_ranges;

    Selection() = default;
    Selection(AnyVar&& v);

    AnyVar to_any() const;
};

///
/// \brief The SelectionRef struct models a noodles SelectionObject, but uses
/// spans to reference data.
///
struct SelectionRef {
    using Pair = std::pair<int64_t, int64_t>;

    noo::PossiblyOwnedView<int64_t const> rows;
    noo::PossiblyOwnedView<int64_t const> raw_ranges;
    std::span<Pair const>                 row_ranges;

    SelectionRef() = default;
    SelectionRef(Selection const&);
    SelectionRef(AnyVarRef const&);

    AnyVar to_any() const;
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


///
/// \brief The RealListArg struct is used in methods to coerce the Any type
/// to turn a real-list, a list of any-reals or list of any-ints into a list of
/// reals.
///
struct RealListArg {
    std::vector<double> list;

    RealListArg() = default;
    RealListArg(AnyVar&& a) : list(a.coerce_real_list()) { }
};

///
/// \brief The IntListArg struct is used in methods to coerce an any type into a
/// list of integers.
///
struct IntListArg {
    std::vector<int64_t> list;

    IntListArg() = default;
    IntListArg(AnyVar&& a) : list(a.coerce_int_list()) { }
};

///
/// \brief The StringListArg struct is used in methods to coerce an any into a
/// list of strings
///
struct StringListArg {
    std::vector<std::string> list;

    StringListArg() = default;
    StringListArg(AnyVar&& a);
};


struct Vec3Arg : std::optional<glm::vec3> {
    Vec3Arg() = default;
    Vec3Arg(AnyVar&&);
};

struct Vec4Arg : std::optional<glm::vec4> {
    Vec4Arg() = default;
    Vec4Arg(AnyVar&&);
};

struct BoolArg {
    char state = -1;

    BoolArg() = default;
    BoolArg(AnyVar&&);

    operator bool() const { return state >= 0; }

    bool operator*() const { return state == 1; }
};


} // namespace noo

#endif // INTERFACE_TYPES_H
