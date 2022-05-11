#include "bufferlist.h"

#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"

namespace noo {

BufferList::BufferList(ServerT* s) : ComponentListBase(s) { }
BufferList::~BufferList() = default;

BufferT::BufferT(IDType id, BufferList* host, BufferData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void BufferT::write_new_to(Writer& w) {
    auto lid = convert_id(id(), w);

    VMATCH(
        m_data.source,
        VCASE(BufferOwningSource const& source) {
            auto byte_handle = w->CreateVectorScalarCast<int8_t>(
                source.to_move.data(), source.to_move.size());

            auto x = noodles::CreateBufferCreateDirect(
                w,
                lid,
                m_data.name.size() ? m_data.name.c_str() : nullptr,
                source.to_move.size(),
                noodles::BufferSource::InlineSource,
                byte_handle.Union());

            w.complete_message(x);
        },
        VCASE(BufferURLSource const& source) {
            auto url_string = source.url_source.toString().toStdString();
            auto size       = source.source_byte_size;

            auto s = noodles::CreateURLSourceDirect(w, url_string.c_str());

            auto x = noodles::CreateBufferCreateDirect(
                w,
                lid,
                m_data.name.size() ? m_data.name.c_str() : nullptr,
                size,
                noodles::BufferSource::URLSource,
                s.Union());

            w.complete_message(x);
        });
}

void BufferT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateBufferDelete(w, lid);

    w.complete_message(x);
}

// =============================================================================

BufferViewList::BufferViewList(ServerT* s) : ComponentListBase(s) { }
BufferViewList::~BufferViewList() = default;

BufferViewT::BufferViewT(IDType                id,
                         BufferViewList*       host,
                         BufferViewData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void BufferViewT::write_new_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto nh = m_data.name.size() ? w->CreateString(m_data.name)
                                 : flatbuffers::Offset<flatbuffers::String>();

    auto x = noodles::CreateBufferViewCreate(
        w,
        lid,
        nh,
        convert_id(m_data.source_buffer->id(), w),
        noodles::ViewType(m_data.type),
        m_data.offset,
        m_data.length);

    w.complete_message(x);
}

void BufferViewT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateBufferViewDelete(w, lid);

    w.complete_message(x);
}

// =============================================================================


LightList::LightList(ServerT* s) : ComponentListBase(s) { }
LightList::~LightList() = default;

LightT::LightT(IDType id, LightList* host, LightData const& d)
    : ComponentMixin<LightT, LightList, LightID>(id, host), m_data(d) { }

void LightT::write_new_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto ncol = convert(m_data.color);

    noodles::LightType light_enum;

    auto ltype = VMATCH(
        m_data.type,
        VCASE(PointLight const& pl) {
            auto l     = noodles::CreatePointLight(w, pl.range);
            light_enum = noodles::LightType::PointLight;
            return l.Union();
        },
        VCASE(SpotLight const& sl) {
            auto l = noodles::CreateSpotLight(
                w, sl.range, sl.inner_cone_angle_rad, sl.outer_cone_angle_rad);
            light_enum = noodles::LightType::SpotLight;
            return l.Union();
        },
        VCASE(DirectionLight const& dl) {
            auto l     = noodles::CreateDirectionLight(w, dl.range);
            light_enum = noodles::LightType::DirectionLight;
            return l.Union();
        });

    auto x = noodles::CreateLightCreateUpdateDirect(
        w,
        lid,
        m_data.name.size() ? m_data.name.data() : nullptr,
        &ncol,
        m_data.intensity,
        light_enum,
        ltype);

    w.complete_message(x);
}

void LightT::update(LightUpdateData const& d, Writer& w) {

    auto lid = convert_id(id(), w);

    if (d.color) { m_data.color = *d.color; }
    if (d.intensity) { m_data.intensity = *d.intensity; }

    auto ncol = convert(m_data.color);

    auto x = noodles::CreateLightCreateUpdate(
        w, lid, 0, d.color ? &ncol : nullptr, m_data.intensity);

    w.complete_message(x);
}

void LightT::update(LightUpdateData const& d) {
    auto w = m_parent_list->new_bcast();

    update(d, *w);
}
void LightT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateLightDelete(w, lid);

    w.complete_message(x);
}

} // namespace noo
