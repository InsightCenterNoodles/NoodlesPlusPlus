#include "noo_anyref.h"

#include "src/generated/interface_tools.h"
#include "src/generated/noodles_generated.h"

namespace noo {

static_assert((int)noodles::AnyType::NONE == (int)AnyVarRef::AnyType::NONE);
static_assert((int)noodles::AnyType::Text == (int)AnyVarRef::AnyType::Text);
static_assert((int)noodles::AnyType::Integer ==
              (int)AnyVarRef::AnyType::Integer);
static_assert((int)noodles::AnyType::IntegerList ==
              (int)AnyVarRef::AnyType::IntegerList);
static_assert((int)noodles::AnyType::Real == (int)AnyVarRef::AnyType::Real);
static_assert((int)noodles::AnyType::RealList ==
              (int)AnyVarRef::AnyType::RealList);
static_assert((int)noodles::AnyType::Data == (int)AnyVarRef::AnyType::Data);
static_assert((int)noodles::AnyType::AnyList ==
              (int)AnyVarRef::AnyType::AnyList);
static_assert((int)noodles::AnyType::AnyMap == (int)AnyVarRef::AnyType::AnyMap);
static_assert((int)noodles::AnyType::AnyID == (int)AnyVarRef::AnyType::AnyID);
static_assert((int)noodles::AnyType::MIN == (int)AnyVarRef::AnyType::MIN);
static_assert((int)noodles::AnyType::MAX == (int)AnyVarRef::AnyType::MAX);

// =============================================================================

AnyVarRef::AnyVarRef(noodles::Any const* c) : m_source(c) { }

bool AnyVarRef::has_int() const {
    if (!m_source) return false;
    return m_source->any_type() == noodles::AnyType::Integer;
}

bool AnyVarRef::has_real() const {
    if (!m_source) return false;
    return m_source->any_type() == noodles::AnyType::Real;
}

bool AnyVarRef::has_string() const {
    if (!m_source) return false;
    return m_source->any_type() == noodles::AnyType::Text;
}

bool AnyVarRef::has_list() const {
    if (!m_source) return false;
    return m_source->any_type() == noodles::AnyType::AnyList;
}

bool AnyVarRef::has_int_list() const {
    if (!m_source) return false;
    return m_source->any_type() == noodles::AnyType::IntegerList;
}

bool AnyVarRef::has_real_list() const {
    if (!m_source) return false;
    return m_source->any_type() == noodles::AnyType::RealList;
}

bool AnyVarRef::has_byte_list() const {
    if (!m_source) return false;
    return m_source->any_type() == noodles::AnyType::Data;
}


AnyVarRef::AnyType AnyVarRef::type() const {
    if (!m_source) return AnyType::NONE;
    return (AnyType)m_source->any_type();
}


int64_t AnyVarRef::to_int() const {
    if (!m_source) return 0;
    auto* v = m_source->any_as_Integer();
    if (v) return v->integer();
    return 0;
}

double AnyVarRef::to_real() const {
    if (!m_source) return 0;
    auto* v = m_source->any_as_Real();
    if (v) return v->real();
    return 0;
}

std::span<std::byte const> AnyVarRef::to_data() const {
    if (!m_source) return {};
    auto* v = m_source->any_as_Data();
    if (!v) return {};

    return { (std::byte const*)v->data()->data(), v->data()->size() };
}

std::string_view AnyVarRef::to_string() const {
    if (!m_source) return {};
    auto* v = m_source->any_as_Text();
    if (v) return v->text()->string_view();
    return {};
}

AnyVarListRef AnyVarRef::to_vector() const {
    if (!m_source) return {};
    auto const* v = m_source->any_as_AnyList();
    if (v) return AnyVarListRef(v);
    return {};
}

AnyVarMapRef AnyVarRef::to_map() const {
    if (!m_source) return {};
    auto const* v = m_source->any_as_AnyMap();
    if (v) return AnyVarMapRef(v);
    return {};
}

std::span<int64_t const> AnyVarRef::to_int_list() const {
    if (!m_source) return {};
    auto const* v = m_source->any_as_IntegerList();
    if (!v) return {};

    return { v->integers()->data(), v->integers()->size() };
}

std::span<double const> AnyVarRef::to_real_list() const {
    if (!m_source) return {};
    auto const* v = m_source->any_as_RealList();
    if (!v) return {};

    return { v->reals()->data(), v->reals()->size() };
}

AnyID AnyVarRef::to_id() const {
    if (!m_source) return {};

    auto const* v = m_source->any_as_AnyID();

    AnyID ret;

    read_to(v, ret);

    return ret;
}

PossiblyOwnedView<double const> AnyVarRef::coerce_real_list() const {
    if (!m_source) return {};

    if (has_list()) {

        auto const& list = *m_source->any_as_AnyList()->list();

        std::vector<double> ret;
        ret.reserve(list.size());

        for (size_t i = 0; i < list.size(); i++) {
            auto v = list[i];

            switch (v->any_type()) {
            case noodles::AnyType::Integer:
                ret.push_back(v->any_as_Integer()->integer());
                break;
            case noodles::AnyType::Real:
                ret.push_back(v->any_as_Real()->real());
                break;
            default: break;
            }
        }

        return ret;
    }

    if (has_real_list()) { return to_real_list(); }

    return {};
}

PossiblyOwnedView<int64_t const> AnyVarRef::coerce_int_list() const {
    if (!m_source) return {};

    if (has_list()) {

        auto const& list = *m_source->any_as_AnyList()->list();

        std::vector<int64_t> ret;
        ret.reserve(list.size());

        for (size_t i = 0; i < list.size(); i++) {
            auto v = list[i];

            switch (v->any_type()) {
            case noodles::AnyType::Integer:
                ret.push_back(v->any_as_Integer()->integer());
                break;
            case noodles::AnyType::Real:
                ret.push_back(v->any_as_Real()->real());
                break;
            default: break;
            }
        }

        return ret;
    }

    if (has_int_list()) { return to_int_list(); }

    return {};
}

// =============================================================================

namespace {


std::string to_string(AnyVarRef const& v);


std::string to_string_part(std::monostate) {
    return "NULL";
}

std::string to_string_part(int64_t i) {
    return std::to_string(i);
}

std::string to_string_part(double i) {
    return std::to_string(i);
}

std::string to_string_part(std::string_view i) {
    return std::string(i);
}

template <class Tag>
std::string to_string_part(ID<Tag> id) {
    return id.to_string();
}

std::string to_string_part(AnyID const& i) {
    return std::visit([](auto const& ptr) { return to_string_part(ptr); }, i);
}

std::string to_string_part(std::span<std::byte const> const&) {
    return "DATA";
}

std::string to_string_part(std::span<double const> const& v) {
    std::string ret;

    for (double d : v) {
        ret += std::to_string(d) + ", ";
    }

    return ret;
}

std::string to_string_part(std::span<int64_t const> const& v) {
    std::string ret;

    for (auto d : v) {
        ret += std::to_string(d) + ", ";
    }

    return ret;
}


std::string to_string_part(AnyVarMapRef const& map) {
    std::string ret = "{";

    for (auto const& [k, v] : map) {
        ret += k + ": " + to_string(v) + ",";
    }

    ret += "}";

    return ret;
}

std::string to_string_part(AnyVarListRef const& list) {
    std::string ret = "[";

    list.for_each([&ret](auto, auto r) { ret += to_string(r) + ","; });


    ret += "]";

    return ret;
}


std::string to_string(AnyVarRef const& v) {
    return visit([](auto const& thing) { return to_string_part(thing); }, v);
}

} // namespace

std::string AnyVarRef::dump_string() const {
    return ::noo::to_string(*this);
}

// =============================================================================

AnyVarListRef::AnyVarListRef(noodles::AnyList const* s) : m_list_source(s) { }

AnyVarRef AnyVarListRef::operator[](size_t i) const {
    if (!m_list_source) return {};

    return AnyVarRef(m_list_source->list()->Get(i));
}

size_t AnyVarListRef::size() const {
    if (!m_list_source) return 0;

    return m_list_source->list()->size();
}

// =============================================================================


AnyVarMapRef::AnyVarMapRef(noodles::AnyMap const* s) {
    if (!s) return;
    if (!s->entries()) return;

    for (auto v : *s->entries()) {
        auto key   = v->name();
        auto value = v->value();

        this->try_emplace(key->str(), AnyVarRef(value));
    }
}

} // namespace noo
