#pragma once

#include <variant>

namespace noo {

// due to bugs in the GCC stdlib, we have a special cast for visit here
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90943

template <typename... Ts>
constexpr auto variant_cast(std::variant<Ts...>& v) -> std::variant<Ts...>& {
    return v;
}
template <typename... Ts>
constexpr auto variant_cast(std::variant<Ts...> const& v)
    -> std::variant<Ts...> const& {
    return v;
}
template <typename... Ts>
constexpr auto variant_cast(std::variant<Ts...>&& v) -> std::variant<Ts...>&& {
    return std::move(v);
}
template <typename... Ts>
constexpr auto variant_cast(std::variant<Ts...> const&& v)
    -> std::variant<Ts...> const&& {
    return std::move(v);
}

template <typename Visitor, typename... Variants>
constexpr decltype(auto) visit(Visitor&& vis, Variants&&... vars) {
    return std::visit(std::forward<Visitor>(vis),
                      variant_cast(std::forward<Variants>(vars))...);
}
} // namespace noo

// From the Dacite source

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

///
/// To use:
/// \code
/// std::visit(
///     overloaded{
///         [](TypeA a) { return 1; },
///         [](TypeB b) { return 2; },
///     f);
/// \endcode
///
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/// QOL macros for the overload tool
#define VCASE(C) [&](C)
#define VMATCH(V, ...) noo::visit(overloaded { __VA_ARGS__ }, V);
#define VMATCH_W(C, V, ...) C(overloaded { __VA_ARGS__ }, V);

/// To use macros:
/// \code
///
/// VMATCH(t,
///     VCASE(int& a) {
///         return a;
///     },
///     VCASE(std::string& s) -> int {
///         return s.size();
///     }
/// )
///
/// \endcode
