#include "noo_interface_types.h"

#include "noo_common.h"

#include <QDebug>

namespace noo {

Selection::Selection(AnyVar&& v) {
    auto raw_obj = v.steal_map();

    rows = steal_or_default(raw_obj, "rows").coerce_int_list();

    auto raw_ranges_list =
        steal_or_default(raw_obj, "row_ranges").coerce_int_list();

    row_ranges.reserve(raw_ranges_list.size() / 2);

    assert(row_ranges.size() * 2 == raw_ranges_list.size());

    std::memcpy(row_ranges.data(),
                raw_ranges_list.data(),
                sizeof(int64_t) * raw_ranges_list.size());
}


AnyVar Selection::to_any() const {
    AnyVar ret;

    auto& map = ret.emplace<AnyVarMap>();

    std::span<int64_t> ls((int64_t*)row_ranges.data(), row_ranges.size() * 2);

    map["selected_rows"]       = rows;
    map["selected_row_ranges"] = ls;

    return ret;
}


SelectionRef::SelectionRef(Selection const& s)
    : rows(s.rows), row_ranges(s.row_ranges) { }

SelectionRef::SelectionRef(AnyVarRef const& s) {
    auto raw_obj = s.to_map();

    rows       = steal_or_default(raw_obj, "rows").coerce_int_list();
    raw_ranges = steal_or_default(raw_obj, "row_ranges").coerce_int_list();

    // turn the contiguous span into a pair span

    row_ranges = cast_span_to<Pair const>(raw_ranges.span());
}

AnyVar SelectionRef::to_any() const {
    AnyVar ret;

    auto& map = ret.emplace<AnyVarMap>();

    map["selected_rows"]       = rows.span();
    map["selected_row_ranges"] = cast_span_to<int64_t const>(row_ranges);

    return ret;
}

Selection SelectionRef::to_selection() const {
    Selection s;
    s.rows       = span_to_vector(rows.span());
    s.row_ranges = span_to_vector(row_ranges);

    return s;
}

//==============================================================================

StringListArg::StringListArg(AnyVarRef const& a) {
    auto l = a.to_vector();

    l.for_each([&](auto, AnyVarRef const& ref) {
        list.emplace_back(std::string(ref.to_string()));
    });
}

Vec3Arg::Vec3Arg(AnyVarRef const& a) {
    auto l = a.to_real_list();

    if (l.size() < 3) { return; }

    this->emplace(l[0], l[1], l[2]);
}

Vec4Arg::Vec4Arg(AnyVarRef const& a) {
    auto l = a.to_real_list();

    if (l.size() < 4) { return; }

    this->emplace(l[0], l[1], l[2], l[3]);
}

BoolArg::BoolArg(AnyVarRef const& a) {
    if (!a.has_int()) return;

    auto l = a.to_int();

    state = l;
}

} // namespace noo
