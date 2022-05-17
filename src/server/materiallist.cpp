#include "materiallist.h"

#include "src/common/serialize.h"
#include "src/generated/interface_tools.h"
#include "texturelist.h"

namespace noo {

MaterialList::MaterialList(ServerT* s) : ComponentListBase(s) { }

MaterialList::~MaterialList() { }

// =============================================================================

MaterialT::MaterialT(IDType id, MaterialList* host, MaterialData const& d)
    : ComponentMixin(id, host), m_data(d) { }

std::optional<messages::TextureRef>
convert(std::optional<TextureRef> const& tr) {
    if (!tr) return std::nullopt;

    messages::TextureRef ret;
    ret.texture            = tr->source->id();
    ret.transform          = tr->transform;
    ret.texture_coord_slot = tr->texture_coord_slot;

    return ret;
}

void convert(PBRInfo const& src, messages::PBRInfo& dest) {
    dest.base_color          = src.base_color;
    dest.base_color_texture  = convert(src.base_color_texture);
    dest.metallic            = src.metallic;
    dest.roughness           = src.roughness;
    dest.metal_rough_texture = convert(src.metal_rough_texture);
}

void MaterialT::write_new_to(SMsgWriter& w) {

    messages::MsgMaterialCreate m;

    m.id   = id();
    m.name = opt_string(m_data.name);

    convert(m_data.pbr_info, m.pbr_info);

    m.normal_texture = convert(m_data.normal_texture);

    m.occlusion_texture = convert(m_data.occlusion_texture);

    if (m_data.occlusion_texture_factor) {
        m.occlusion_texture_factor = m_data.occlusion_texture_factor.value();
    }

    m.emissive_texture = convert(m_data.emissive_texture);

    if (m_data.emissive_factor) {
        m.emissive_factor = m_data.emissive_factor.value();
    }

    if (m_data.use_alpha) { m.use_alpha = m_data.use_alpha.value(); }

    if (m_data.alpha_cutoff) { m.alpha_cutoff = m_data.alpha_cutoff.value(); }

    if (m_data.double_sided) { m.double_sided = m_data.double_sided.value(); }

    w.add(m);
}

void MaterialT::update(MaterialData const& d, SMsgWriter& w) {
    m_data = d;

    // same message is used
    write_new_to(w);
}

void MaterialT::update(MaterialData const& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void MaterialT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgMaterialDelete { .id = id() });
}

} // namespace noo
