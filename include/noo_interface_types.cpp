#include "noo_interface_types.h"

#include "noo_common.h"

#include <QDebug>
#include <QtGlobal>

#include <string>

using namespace std::string_literals;

namespace noo {

QCborValue to_cbor(std::span<double> sp) {
    QCborArray arr;
    for (auto d : sp) {
        arr << d;
    }
    return arr;
}
QCborValue to_cbor(std::span<int64_t> sp) {
    QCborArray arr;
    for (auto d : sp) {
        arr << (qint64)d;
    }
    return arr;
}
QCborValue to_cbor(QVector<int64_t> v) {
    return to_cbor(std::span(v));
}

QCborValue to_cbor(glm::vec3 v) {
    QCborArray arr;
    arr << v.x << v.y << v.z;
    return arr;
}
QCborValue to_cbor(glm::vec4 v) {
    QCborArray arr;
    arr << v.x << v.y << v.z << v.w;
    return arr;
}

QCborValue to_cbor(Selection const& t) {
    return t.to_cbor();
}


// =============================================================================

QVector<double> coerce_to_real_list(QCborValue v) {
    QVector<double> ret;

    if (!v.isArray()) return ret;

    auto arr = v.toArray();

    for (auto a : arr) {
        ret << a.toDouble();
    }

    return ret;
}

QVector<int64_t> coerce_to_int_list(QCborValue v) {
    QVector<int64_t> ret;

    if (!v.isArray()) return ret;

    auto arr = v.toArray();

    for (auto a : arr) {
        ret << a.toInteger();
    }

    return ret;
}

// =============================================================================

static auto const row_str       = QStringLiteral("rows");
static auto const row_range_str = QStringLiteral("row_ranges");

Selection::Selection(QCborMap v) {

    rows = noo::coerce_to_int_list(v[row_str]);

    auto raw_ranges_list = noo::coerce_to_int_list(v[row_range_str]);

    row_ranges.reserve(raw_ranges_list.size() / 2);

    assert(row_ranges.size() * 2 == raw_ranges_list.size());

    std::memcpy(row_ranges.data(),
                raw_ranges_list.data(),
                sizeof(int64_t) * raw_ranges_list.size());
}


QCborValue Selection::to_cbor() const {
    QCborMap map;

    std::span<int64_t> ls((int64_t*)row_ranges.data(), row_ranges.size() * 2);

    map[row_str]       = noo::to_cbor(rows);
    map[row_range_str] = noo::to_cbor(ls);

    return map;
}

// =============================================================================

StringListArg::StringListArg(QCborValue const& a) {
    from_cbor(a, list);
}

Vec3Arg::Vec3Arg(QCborValue const& a) {
    glm::vec3 val;
    if (from_cbor(a, val)) { this->emplace(val); }
}

Vec3ListArg::Vec3ListArg(QCborValue const& a) {
    if (!a.isArray()) return;
    auto arr = a.toArray();

    for (auto la : arr) {
        glm::vec3 val;
        bool      ok = from_cbor(la, val);

        if (ok) { this->push_back(val); }
    }
}

Vec4Arg::Vec4Arg(QCborValue const& a) {
    glm::vec4 val;
    if (from_cbor(a, val)) { this->emplace(val); }
}

IntArg::IntArg(QCborValue const& a) {
    int64_t val;
    if (from_cbor(a, val)) { this->emplace(val); }
}

BoolArg::BoolArg(QCborValue const& a) {
    if (a.isBool()) state = a.toBool();
}

} // namespace noo
