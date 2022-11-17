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

void convert(PBRInfo const& src, std::optional<messages::PBRInfo>& dest_opt) {
    auto& dest               = dest_opt.emplace();
    dest.base_color          = src.base_color;
    dest.base_color_texture  = convert(src.base_color_texture);
    dest.metallic            = src.metallic;
    dest.roughness           = src.roughness;
    dest.metal_rough_texture = convert(src.metal_rough_texture);
}

template <class Message, class UP>
void update_common(MaterialT* obj, UP const& data, Message& m) {

    m.id = obj->id();

    if (data.pbr_info) { convert(*data.pbr_info, m.pbr_info); }

    if (data.normal_texture) {
        m.normal_texture = convert(data.normal_texture);
    }

    if (data.occlusion_texture) {
        m.occlusion_texture = convert(data.occlusion_texture);
    }

    if (data.occlusion_texture_factor) {
        m.occlusion_texture_factor = data.occlusion_texture_factor.value();
    }

    if (data.emissive_texture) {
        m.emissive_texture = convert(data.emissive_texture);
    }

    if (data.emissive_factor) {
        m.emissive_factor = data.emissive_factor.value();
    }

    if (data.use_alpha) { m.use_alpha = data.use_alpha.value(); }

    if (data.alpha_cutoff) { m.alpha_cutoff = data.alpha_cutoff.value(); }

    if (data.double_sided) { m.double_sided = data.double_sided.value(); }
}

void MaterialT::write_new_to(SMsgWriter& w) {

    messages::MsgMaterialCreate m;

    m.name = opt_string(m_data.name);

    update_common(this, m_data, m);

    w.add(m);
}

#define CHECK_UPDATE(FLD)                                                      \
    {                                                                          \
        if (data.FLD) { m_data.FLD = std::move(*data.FLD); }                   \
    }

void MaterialT::update(MaterialUpdateData const& data, SMsgWriter& w) {
    messages::MsgMaterialUpdate m;

    update_common(this, data, m);

    CHECK_UPDATE(pbr_info);
    CHECK_UPDATE(normal_texture);

    CHECK_UPDATE(occlusion_texture);
    CHECK_UPDATE(occlusion_texture_factor);

    CHECK_UPDATE(emissive_texture);
    CHECK_UPDATE(emissive_factor);

    CHECK_UPDATE(use_alpha);
    CHECK_UPDATE(alpha_cutoff);

    CHECK_UPDATE(double_sided);

    // same message is used
    write_new_to(w);
}

void MaterialT::update(MaterialUpdateData const& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void MaterialT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgMaterialDelete { .id = id() });
}

} // namespace noo
