#pragma once

#include "noo_id.h"
#include "noo_include_glm.h"

#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// forwards
namespace noodles {
struct Any;
struct AnyList;
struct AnyMap;
} // namespace noodles

namespace noo {

class AnyVarListRef;
class AnyVarMapRef;

template <class T>
struct PossiblyOwnedView {
    using StrippedT = std::remove_cvref_t<T>;
    std::variant<std::span<T>, std::vector<StrippedT>> storage;

    PossiblyOwnedView() = default;
    PossiblyOwnedView(std::span<T> s) : storage(s) { }
    PossiblyOwnedView(std::vector<StrippedT>&& s) : storage(std::move(s)) { }

    PossiblyOwnedView(PossiblyOwnedView&&) = default;
    PossiblyOwnedView& operator=(PossiblyOwnedView&&) = default;


    PossiblyOwnedView& operator=(std::span<T> s) {
        storage = s;
        return *this;
    }

    PossiblyOwnedView& operator=(std::vector<StrippedT>&& s) {
        storage = std::move(s);
        return *this;
    }

    auto begin() const {
        return std::visit([](auto const& a) { return a.begin(); }, storage);
    }

    auto end() const {
        return std::visit([](auto const& a) { return a.end(); }, storage);
    }

    T const* data() const {
        return std::visit([](auto const& a) { return a.data(); }, storage);
    }

    auto size() const {
        return std::visit([](auto const& a) { return a.size(); }, storage);
    }

    std::span<T> span() const {
        return std::visit([](auto const& a) { return std::span<T>(a); },
                          storage);
    }
};

///
/// \brief The AnyVar class models the noodles Any variable
///
class AnyVarRef {
    noodles::Any const* m_source = nullptr;

public:
    enum class AnyType : uint8_t {
        NONE        = 0,
        Text        = 1,
        Integer     = 2,
        IntegerList = 3,
        Real        = 4,
        RealList    = 5,
        Data        = 6,
        AnyList     = 7,
        AnyMap      = 8,
        AnyID       = 9,
        MIN         = NONE,
        MAX         = AnyID
    };

    AnyVarRef() = default;
    AnyVarRef(noodles::Any const*);

    bool has_int() const;
    bool has_real() const;
    bool has_string() const;
    bool has_list() const;
    bool has_int_list() const;
    bool has_real_list() const;
    bool has_byte_list() const;

    bool is_some_list() const {
        return has_list() or has_real_list() or has_int_list();
    }

    AnyType type() const;

    int64_t                    to_int() const;
    double                     to_real() const;
    std::string_view           to_string() const;
    AnyVarListRef              to_vector() const;
    std::span<std::byte const> to_data() const;
    AnyVarMapRef               to_map() const;
    std::span<double const>    to_real_list() const;
    std::span<int64_t const>   to_int_list() const;
    AnyID                      to_id() const;

    PossiblyOwnedView<double const>  coerce_real_list() const;
    PossiblyOwnedView<int64_t const> coerce_int_list() const;

    /// Dump the Any to a human-friendly string representation.
    std::string dump_string() const;
};

template <class T>
constexpr inline bool is_in_anyref_typelist = false;

template <>
constexpr inline bool is_in_anyref_typelist<int64_t> = true;

template <>
constexpr inline bool is_in_anyref_typelist<double> = true;

template <>
constexpr inline bool is_in_anyref_typelist<std::string_view> = true;

template <>
constexpr inline bool is_in_anyref_typelist<AnyVarListRef> = true;

template <>
constexpr inline bool is_in_anyref_typelist<std::span<double const>> = true;

template <>
constexpr inline bool is_in_anyref_typelist<std::span<int64_t const>> = true;

template <>
constexpr inline bool is_in_anyref_typelist<AnyID> = true;

// =============================================================================

class AnyVarListRef {
    noodles::AnyList const* m_list_source = nullptr;

public:
    // creating an iterator for this is possible but a bit of a pain at the
    // moment.
    AnyVarListRef() = default;
    AnyVarListRef(noodles::AnyList const*);

    AnyVarRef operator[](size_t i) const;

    size_t size() const;

    template <class Function>
    void for_each(Function&& f) const {
        auto s = size();
        for (size_t i = 0; i < s; i++) {
            f(i, this->operator[](i));
        }
    }

    /// Dump the Any to a human-friendly string representation.
    std::string dump_string() const;
};

// =============================================================================

class AnyVarMapRef : public std::unordered_map<std::string, AnyVarRef> {
    // we could do this totally out of class, but we do want lookups, so, here
    // we pack it into the class.
    // there is the lookup functionality, but that demands sorting...which we
    // can't actually guarantee.
public:
    AnyVarMapRef() = default;
    AnyVarMapRef(noodles::AnyMap const*);

    AnyVarRef operator[](std::string_view) const;

    /// Dump the Any to a human-friendly string representation.
    std::string dump_string() const;
};


// =============================================================================

template <class Function>
auto visit(Function&& f, AnyVarRef const& r) {
    switch (r.type()) {
    case AnyVarRef::AnyType::NONE: return f(std::monostate());
    case AnyVarRef::AnyType::Text: return f(r.to_string());
    case AnyVarRef::AnyType::Integer: return f(r.to_int());
    case AnyVarRef::AnyType::IntegerList: return f(r.to_int_list());
    case AnyVarRef::AnyType::Real: return f(r.to_real());
    case AnyVarRef::AnyType::RealList: return f(r.to_real_list());
    case AnyVarRef::AnyType::Data: return f(r.to_data());
    case AnyVarRef::AnyType::AnyList: return f(r.to_vector());
    case AnyVarRef::AnyType::AnyMap: return f(r.to_map());
    case AnyVarRef::AnyType::AnyID: return f(r.to_id());
    }
}

} // namespace noo
