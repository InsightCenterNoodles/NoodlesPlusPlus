#include "bufferlist.h"

#include "src/common/serialize.h"
#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"

#include <magic_enum.hpp>

namespace noo {

BufferList::BufferList(ServerT* s) : ComponentListBase(s) { }
BufferList::~BufferList() = default;

BufferT::BufferT(IDType id, BufferList* host, BufferData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void BufferT::write_new_to(SMsgWriter& w) {
    messages::MsgBufferCreate create;
    create.id = id();

    if (m_data.name.size()) { create.name = m_data.name; }

    VMATCH(
        m_data.source,
        VCASE(BufferInlineSource const& source) {
            create.inline_bytes = source.data;
            create.size         = source.data.size();
        },
        VCASE(BufferURLSource const& source) {
            create.uri_bytes = source.url_source;
            create.size      = source.source_byte_size;
        });

    w.add(create);
}

void BufferT::write_delete_to(SMsgWriter& w) {
    messages::MsgBufferDelete deletion;
    deletion.id = id();
    w.add(deletion);
}

// =============================================================================

BufferViewList::BufferViewList(ServerT* s) : ComponentListBase(s) { }
BufferViewList::~BufferViewList() = default;

BufferViewT::BufferViewT(IDType                id,
                         BufferViewList*       host,
                         BufferViewData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void BufferViewT::write_new_to(SMsgWriter& w) {
    messages::MsgBufferViewCreate create {
        .id            = id(),
        .source_buffer = m_data.source_buffer->id(),
        .type   = QString::fromLocal8Bit(magic_enum::enum_name(m_data.type)),
        .offset = m_data.offset,
        .length = m_data.length,
    };

    w.add(create);
}

void BufferViewT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgBufferViewDelete { .id = id() });
}

// =============================================================================


LightList::LightList(ServerT* s) : ComponentListBase(s) { }
LightList::~LightList() = default;

LightT::LightT(IDType id, LightList* host, LightData const& d)
    : ComponentMixin<LightT, LightList, LightID>(id, host), m_data(d) { }

void LightT::write_new_to(SMsgWriter& w) {
    messages::MsgLightCreate m {
        .id        = id(),
        .name      = m_data.name.size() ? m_data.name : QString(),
        .color     = m_data.color,
        .intensity = m_data.intensity,
    };

    VMATCH(
        m_data.type,
        VCASE(PointLight const& pl) {
            m.point = messages::PointLight {
                .range = pl.range,
            };
        },
        VCASE(SpotLight const& sl) {
            m.spot = messages::SpotLight {
                .range                = sl.range,
                .inner_cone_angle_rad = sl.inner_cone_angle_rad,
                .outer_cone_angle_rad = sl.outer_cone_angle_rad,
            };
        },
        VCASE(DirectionLight const& dl) {
            m.directional = messages::DirectionalLight {
                .range = dl.range,
            };
        });

    w.add(m);
}

void LightT::update(LightUpdateData const& d, SMsgWriter& w) {
    if (d.color) m_data.color = *d.color;
    if (d.intensity) m_data.intensity = *d.intensity;

    messages::MsgLightUpdate m {
        .color     = m_data.color,
        .intensity = m_data.intensity,
    };

    w.add(m);
}

void LightT::update(LightUpdateData const& d) {
    auto w = m_parent_list->new_bcast();

    update(d, *w);
}
void LightT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgLightDelete { .id = id() });
}

} // namespace noo
