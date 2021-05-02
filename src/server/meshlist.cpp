#include "meshlist.h"

#include "bufferlist.h"
#include "noodlesserver.h"
#include "serialize.h"
#include "src/generated/interface_tools.h"
#include "src/generated/noodles_server_generated.h"

#include <QDebug>
namespace noo {

MeshList::MeshList(ServerT* s) : ComponentListBase(s) { }
MeshList::~MeshList() = default;

// =============================================================================

MeshT::MeshT(IDType id, MeshList* host, MeshData const& d)
    : ComponentMixin(id, host), m_data(d) { }

static flatbuffers::Offset<noodles::ComponentRef>
setup_comp_ref(ComponentRef const& r, Writer& w) {
    auto bid = convert_id(r.buffer, w);

    // qDebug() << Q_FUNC_INFO << bid.id_slot() << r.start << r.size <<
    // r.stride;

    return noodles::CreateComponentRef(w, bid, r.start, r.size, r.stride);
}

static flatbuffers::Offset<noodles::ComponentRef>
conditional_comp_ref(std::optional<ComponentRef> const& r, Writer& w) {
    if (r) return setup_comp_ref(*r, w);
    return 0;
}

void MeshT::write_new_to(Writer& w) {

    auto lid = convert_id(id(), w);

    auto lem = convert(m_data.extent_min);
    auto lea = convert(m_data.extent_max);

    auto x = noodles::CreateGeometryCreate(
        w,
        lid,
        &lem,
        &lea,
        conditional_comp_ref(m_data.positions, w),
        conditional_comp_ref(m_data.normals, w),
        conditional_comp_ref(m_data.textures, w),
        conditional_comp_ref(m_data.colors, w),
        conditional_comp_ref(m_data.lines, w),
        conditional_comp_ref(m_data.triangles, w));


    w.complete_message(x);
}

void MeshT::update(MeshData const& data, Writer& w) {
    m_data = data;

    write_new_to(w);
}

void MeshT::update(MeshData const& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void MeshT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateGeometryDelete(w, lid);

    w.complete_message(x);
}

} // namespace noo
