#include "meshlist.h"

#include "bufferlist.h"
#include "materiallist.h"
#include "noo_common.h"
#include "noodlesserver.h"
#include "src/common/serialize.h"
#include "src/generated/interface_tools.h"

#include <QDebug>

namespace noo {

MeshList::MeshList(ServerT* s) : ComponentListBase(s) { }
MeshList::~MeshList() = default;

// =============================================================================

MeshT::MeshT(IDType id, MeshList* host, MeshData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void MeshT::write_new_to(SMsgWriter& w) {

    messages::MsgGeometryCreate m;
    m.id   = id();
    m.name = m_data.name;

    for (auto const& patch : m_data.patches) {

        messages::GeometryPatch geom_patch;

        geom_patch.vertex_count = patch.vertex_count;
        geom_patch.type         = patch.type;

        if (patch.material) geom_patch.material = patch.material->id();

        if (patch.indices) {
            auto const& src = *patch.indices;
            auto&       ind = geom_patch.indices.emplace();
            ind.view        = src.view->id();
            ind.format      = src.format;
            ind.stride      = src.stride;
            ind.offset      = src.offset;
            ind.count       = src.count;
        }

        for (auto const& attrib : patch.attributes) {

            messages::Attribute new_attrib;

            new_attrib.view     = attrib.view->id();
            new_attrib.semantic = attrib.semantic;

            if (attrib.channel) new_attrib.channel = attrib.channel;
            if (attrib.offset) new_attrib.offset = attrib.offset;
            if (attrib.stride) new_attrib.stride = attrib.stride;

            new_attrib.format = attrib.format;

            if (attrib.maximum_value.size()) {
                new_attrib.maximum_value = attrib.maximum_value;
            }
            if (attrib.minimum_value.size()) {
                new_attrib.minimum_value = attrib.minimum_value;
            }

            new_attrib.normalized = attrib.normalized;

            geom_patch.attributes << new_attrib;
        }

        m.patches << geom_patch;
    }

    w.add(m);
}

void MeshT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgGeometryCreate { .id = id() });
}

} // namespace noo
