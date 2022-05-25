#include "bufferlist.h"

#include "src/common/serialize.h"
#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"
#include "src/server/noodlesserver.h"
#include "src/server/noodlesstate.h"

#include <magic_enum.hpp>

#include <cstdint>
#include <variant>

namespace noo {

BufferList::BufferList(ServerT* s) : ComponentListBase(s) { }
BufferList::~BufferList() = default;

static constexpr uint64_t inline_threshold = 2 << 16;

static bool is_oversized(QByteArray const& array) {
    return array.size() > inline_threshold;
}

BufferT::BufferT(IDType id, BufferList* host, BufferData const& d)
    : ComponentMixin(id, host), m_data(d) {

    // if this is bytes we control, and they are large enough, register them for
    // download

    auto inline_src = std::get_if<BufferInlineSource>(&m_data.source);

    if (inline_src) {
        if (is_oversized(inline_src->data)) {
            auto* state = host->server()->state();

            auto doc = state->document();

            std::tie(m_asset_id, m_asset_url) =
                doc->storage().register_asset(inline_src->data);
        }
    }
}

BufferT::~BufferT() {
    if (!m_asset_id.isNull()) {
        auto* state = this->hosting_list()->server()->state();

        auto doc = state->document();

        doc->storage().destroy_asset(m_asset_id);
    }
}

void BufferT::write_new_to(SMsgWriter& w) {
    messages::MsgBufferCreate create;
    create.id = id();

    if (m_data.name.size()) { create.name = m_data.name; }

    VMATCH(
        m_data.source,
        VCASE(BufferInlineSource const& source) {
            if (m_asset_url.isValid()) {
                create.uri_bytes = m_asset_url;
                create.size      = source.data.size();
            } else {
                create.inline_bytes = source.data;
                create.size         = source.data.size();
            }
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
    auto tname = magic_enum::enum_name(m_data.type);

    messages::MsgBufferViewCreate create {
        .id            = id(),
        .source_buffer = m_data.source_buffer->id(),
        .type          = QString::fromUtf8(tname.data(), tname.size()),
        .offset        = m_data.offset,
        .length        = m_data.length,
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
