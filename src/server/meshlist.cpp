#include "meshlist.h"

#include "bufferlist.h"
#include "noodlesserver.h"
#include "serialize.h"
#include "src/generated/interface_tools.h"
#include "src/generated/noodles_generated.h"

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

    auto name_h = w->CreateString(m_data.name);

    std::vector<flatbuffers::Offset<noodles::GeometryPatch>> fbb_patches;

    for (auto const& patch : m_data.patches) {
        std::vector<flatbuffers::Offset<noodles::Attribute>> fbb_attrib;

        for (auto const& attrib : patch.attributes) {

            auto const& view = attrib.view;
            view.get()->

                auto fbb_attrib = noodles::CreateAttribute(w,

            )
        }
    }

    auto bb = convert(m_data.bounding_box);

    auto x = noodles::CreateGeometryCreate(
        w,
        lid,
        &bb,
        conditional_comp_ref(m_data.positions, w),
        conditional_comp_ref(m_data.normals, w),
        conditional_comp_ref(m_data.textures, w),
        conditional_comp_ref(m_data.colors, w),
        conditional_comp_ref(m_data.lines, w),
        conditional_comp_ref(m_data.triangles, w));


    w.complete_message(x);
}

void MeshT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateGeometryDelete(w, lid);

    w.complete_message(x);
}

} // namespace noo
