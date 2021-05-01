#include "noo_any.h"

#include "noo_common.h"

namespace noo {


bool AnyVar::has_int() const {
    return std::holds_alternative<int64_t>(*this);
}
bool AnyVar::has_real() const {
    return std::holds_alternative<double>(*this);
}
bool AnyVar::has_list() const {
    return std::holds_alternative<AnyVarList>(*this);
}
bool AnyVar::has_int_list() const {
    return std::holds_alternative<std::vector<int64_t>>(*this);
}
bool AnyVar::has_real_list() const {
    return std::holds_alternative<std::vector<double>>(*this);
}
bool AnyVar::has_byte_list() const {
    return std::holds_alternative<std::vector<std::byte>>(*this);
}

int64_t AnyVar::to_int() const {
    return get_or_default<int64_t>(*this);
}
double AnyVar::to_real() const {
    return get_or_default<double>(*this);
}
std::string AnyVar::to_string() const {
    return get_or_default<std::string>(*this);
}
AnyVarList AnyVar::to_vector() const {
    return get_or_default<AnyVarList>(*this);
}
AnyVarMap AnyVar::to_map() const {
    return get_or_default<AnyVarMap>(*this);
}
std::vector<double> AnyVar::to_real_list() const {
    return get_or_default<std::vector<double>>(*this);
}

std::string AnyVar::steal_string() {
    return steal_or_default<std::string>(*this);
}
AnyVarList AnyVar::steal_vector() {
    return steal_or_default<AnyVarList>(*this);
}
AnyVarMap AnyVar::steal_map() {
    return steal_or_default<AnyVarMap>(*this);
}
std::vector<std::byte> AnyVar::steal_byte_list() {
    return steal_or_default<std::vector<std::byte>>(*this);
}
std::vector<int64_t> AnyVar::steal_int_list() {
    return steal_or_default<std::vector<int64_t>>(*this);
}
std::vector<double> AnyVar::steal_real_list() {
    return steal_or_default<std::vector<double>>(*this);
}

std::vector<double> AnyVar::coerce_real_list() {
    if (has_list()) {

        auto list = steal_vector();

        std::vector<double> ret;
        ret.reserve(list.size());

        for (auto const& v : list) {
            if (v.has_int()) {
                ret.push_back(v.to_int());
            } else if (v.has_real()) {
                ret.push_back(v.to_real());
            }
        }

        return ret;
    }

    if (has_real_list()) {
        // should zero copy here
        return steal_real_list();
    }

    return {};
}

std::vector<int64_t> AnyVar::coerce_int_list() {
    if (has_list()) {

        auto list = steal_vector();

        std::vector<int64_t> ret;
        ret.reserve(list.size());

        for (auto const& v : list) {
            if (v.has_int()) {
                ret.push_back(v.to_int());
            } else if (v.has_real()) {
                ret.push_back(v.to_real());
            }
        }

        return ret;
    }

    if (has_int_list()) {
        // should zero copy here
        return steal_int_list();
    }

    return {};
}

namespace {

std::string to_string(AnyVar const& v);


std::string to_string_part(std::monostate) {
    return "NULL";
}

std::string to_string_part(int64_t i) {
    return std::to_string(i);
}

std::string to_string_part(double i) {
    return std::to_string(i);
}

std::string to_string_part(std::string const& i) {
    return i;
}

template <class Tag>
std::string to_string_part(ID<Tag> id) {
    return id.to_string();
}

std::string to_string_part(AnyID const& i) {
    return std::visit([](auto const& ptr) { return to_string_part(ptr); }, i);
}

std::string to_string_part(std::vector<std::byte> const&) {
    return "DATA";
}

std::string to_string_part(std::vector<double> const& v) {
    std::string ret;

    for (double d : v) {
        ret += std::to_string(d) + ", ";
    }

    return ret;
}

std::string to_string_part(std::vector<int64_t> const& v) {
    std::string ret;

    for (auto d : v) {
        ret += std::to_string(d) + ", ";
    }

    return ret;
}


std::string to_string_part(AnyVarMap const& map) {
    std::string ret = "{";

    for (auto const& [k, v] : map) {
        ret += k + ": " + to_string(v) + ",";
    }

    ret += "}";

    return ret;
}

std::string to_string_part(AnyVarList const& list) {
    std::string ret = "[";

    for (auto v : list) {
        ret += to_string(v) + ",";
    }

    ret += "]";

    return ret;
}


std::string to_string(AnyVar const& v) {
    return v.visit([](auto const& thing) { return to_string_part(thing); });
}


} // namespace

std::string AnyVar::dump_string() const {
    return ::noo::to_string(*this);
}

} // namespace noo
