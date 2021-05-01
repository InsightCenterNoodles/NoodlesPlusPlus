#ifndef NOO_ANY_H
#define NOO_ANY_H

#include "noo_id.h"
#include "noo_include_glm.h"

#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace noo {

class AnyVar;

using AnyVarList = std::vector<AnyVar>;
using AnyVarMap  = std::unordered_map<std::string, AnyVar>;

using AnyVarBase = std::variant<std::monostate, // the null state
                                int64_t,
                                double,
                                std::string,
                                AnyID,
                                std::vector<std::byte>,
                                AnyVarMap,
                                AnyVarList,
                                std::vector<double>,
                                std::vector<int64_t>>;

///
/// \brief The AnyVar class models the noodles Any variable
///
class AnyVar : public AnyVarBase {

public:
    using AnyVarBase::AnyVarBase;

    AnyVar(int i) : AnyVarBase(int64_t(i)) { }
    AnyVar(size_t i) : AnyVarBase(int64_t(i)) { }

    AnyVar(std::string_view v) : AnyVarBase(std::string(v)) { }

    template <class T>
    AnyVar(std::vector<T> const& sp) : AnyVar(std::span<T const>(sp)) { }

    template <class T>
    AnyVar(std::span<T> sp) : AnyVar(std::span<T const>(sp)) { }

    template <class T>
    AnyVar(std::span<T const> sp) {
        if constexpr (std::is_same_v<T, double>) {
            auto& v = emplace<std::vector<double>>();
            v.insert(v.end(), sp.begin(), sp.end());

        } else if constexpr (std::is_same_v<T, int64_t>) {
            auto& v = emplace<std::vector<int64_t>>();
            v.insert(v.end(), sp.begin(), sp.end());
        } else {
            auto& v = emplace<AnyVarList>();
            for (auto const& value : sp) {
                v.emplace_back(value);
            }
        }
    }

    template <class V>
    AnyVar(std::unordered_map<std::string, V> const& map) {
        auto& m = emplace<AnyVarMap>();
        for (auto const& [k, v] : map) {
            m[k] = AnyVar(v);
        }
    }

    AnyVar(glm::vec3 v) {
        auto& ev = emplace<std::vector<double>>(3);

        ev[0] = v.x;
        ev[1] = v.y;
        ev[2] = v.z;
    }

    AnyVar(glm::vec4 v) {
        auto& ev = emplace<std::vector<double>>(4);

        ev[0] = v.x;
        ev[1] = v.y;
        ev[2] = v.z;
        ev[3] = v.w;
    }

    template <class T, class U>
    AnyVar(std::pair<T, U> const& p) {
        auto& v = emplace<AnyVarList>(2);
        v[0]    = AnyVar(p.first);
        v[1]    = AnyVar(p.second);
    }

    template <class T>
    AnyVar& operator=(std::vector<T> const& sp) {
        *this = AnyVar(sp);
        return *this;
    }

    template <class T>
    AnyVar& operator=(std::span<T> sp) {
        *this = AnyVar(sp);
        return *this;
    }

    template <class T>
    AnyVar& operator=(std::span<T const> sp) {
        *this = AnyVar(sp);
        return *this;
    }

    bool has_int() const;
    bool has_real() const;
    bool has_list() const;
    bool has_int_list() const;
    bool has_real_list() const;
    bool has_byte_list() const;

    int64_t             to_int() const;
    double              to_real() const;
    std::string         to_string() const;
    AnyVarList          to_vector() const;
    AnyVarMap           to_map() const;
    std::vector<double> to_real_list() const;

    std::string            steal_string();
    AnyVarList             steal_vector();
    AnyVarMap              steal_map();
    std::vector<std::byte> steal_byte_list();
    std::vector<int64_t>   steal_int_list();
    std::vector<double>    steal_real_list();

    std::vector<double>  coerce_real_list();
    std::vector<int64_t> coerce_int_list();

    /// Dump the Any to a human-friendly string representation.
    std::string dump_string() const;

    template <class Visitor>
    auto visit(Visitor&& vis) {
        return std::visit(vis, *this);
    }

    template <class Visitor>
    auto visit(Visitor&& vis) const {
        return std::visit(vis, *this);
    }
};

/// Take a list of arguments of any type, and try to convert them into an
/// AnyList.
template <class... Args>
AnyVarList marshall_to_any(Args&&... args) {
    AnyVarList ret;

    ret.reserve(sizeof...(args));

    (ret.emplace_back(std::move(args)), ...);

    return ret;
}

// =============================================================================

/// Convert an Any var to a byte
inline void any_to_var(AnyVar& v, std::byte& s) {
    s = static_cast<std::byte>(v.to_int());
}

/// Convert an Any var to an int
inline void any_to_var(AnyVar& v, int64_t& s) {
    s = v.to_int();
}

/// Convert an Any var to a double
inline void any_to_var(AnyVar& v, double& s) {
    s = v.to_real();
}

/// Convert an Any var to a string
inline void any_to_var(AnyVar& v, std::string& s) {
    s = v.steal_string();
}

/// Convert an Any var (if it is a list type) to a fixed array
template <class T, size_t N>
void any_to_var(AnyVar& v, std::array<T, N>& s) {
    if (v.has_list()) {
        auto lv = v.steal_vector();

        auto bound = std::min(s.size(), lv.size());

        for (size_t i = 0; i < bound; i++) {
            any_to_var(lv[i], s[i]);
        }
    } else if (v.has_real_list()) {
        auto lv = v.steal_real_list();

        auto bound = std::min(s.size(), lv.size());

        for (size_t i = 0; i < bound; i++) {
            s[i] = lv[i];
        }
    } else if (v.has_int_list()) {
        auto lv = v.steal_int_list();

        auto bound = std::min(s.size(), lv.size());

        for (size_t i = 0; i < bound; i++) {
            s[i] = lv[i];
        }
    }
}

/// Convert an Any var (if it has a list) to a byte array
template <size_t N>
void any_to_var(AnyVar& v, std::array<std::byte, N>& s) {
    if (v.has_list()) {
        auto lv = v.steal_vector();

        auto bound = std::min(s.size(), lv.size());

        for (size_t i = 0; i < bound; i++) {
            any_to_var(lv[i], s[i]);
        }
    } else if (v.has_byte_list()) {
        auto lv = v.steal_byte_list();

        auto bound = std::min(s.size(), lv.size());

        for (size_t i = 0; i < bound; i++) {
            s[i] = lv[i];
        }
    }
}

/// Convert an Any var (if it has a list) to an array
template <class T>
void any_to_var(AnyVar& v, std::vector<T>& s) {
    if constexpr (std::is_same_v<T, double>) {
        s = v.coerce_real_list();
    } else if constexpr (std::is_same_v<T, int64_t>) {
        s = v.coerce_int_list();
    } else {
        if (v.has_list()) {
            auto lv = v.steal_vector();
            s.resize(lv.size());

            for (size_t i = 0; i < lv.size(); i++) {
                any_to_var(lv[i], s[i]);
            }
        }
    }
}

/// Convert an Any var (if it has a list) to a byte array
inline void any_to_var(AnyVar& v, std::vector<std::byte>& s) {
    if (v.has_list()) {
        auto lv = v.steal_vector();
        s.resize(lv.size());

        for (size_t i = 0; i < lv.size(); i++) {
            any_to_var(lv[i], s[i]);
        }
    } else if (v.has_byte_list()) {
        auto lv = v.steal_byte_list();

        s = std::move(lv);
    }
}

/// Convert an Any var (if it has a map) to a typed map
template <class K, class V>
void any_to_var(AnyVar& v, std::unordered_map<K, V>& s) {
    auto lm = v.steal_map();

    for (auto& [k, v] : lm) {
        any_to_var(v, s.try_emplace(k));
    }
}

/// Convert an any var (assuming it is a list) to a glm::vec3
inline void any_to_var(AnyVar& v, glm::vec3& s) {
    std::array<double, 3> lv;
    any_to_var(v, lv);
    s = glm::vec3(lv[0], lv[1], lv[2]);
}

inline AnyVar to_any(glm::vec3 const& v) {
    auto span = std::span(glm::value_ptr(v), 3);
    return AnyVar(span);
}

} // namespace noo

#endif // NOO_ANY_H
