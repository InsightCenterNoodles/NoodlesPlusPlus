#include "materiallist.h"

#include "serialize.h"
#include "src/generated/interface_tools.h"
#include "texturelist.h"

namespace noo {

MaterialList::MaterialList(ServerT* s) : ComponentListBase(s) { }

MaterialList::~MaterialList() { }

// =============================================================================

MaterialT::MaterialT(IDType id, MaterialList* host, MaterialData const& d)
    : ComponentMixin(id, host), m_data(d) { }


void MaterialT::write_new_to(Writer& w) {

    auto lid = convert_id(id(), w);

    auto col = convert(m_data.color);

    auto tid = convert_id(m_data.texture, w);

    auto x = noodles::CreateMaterialCreateUpdate(w,
                                                 lid,
                                                 &col,
                                                 m_data.metallic,
                                                 m_data.roughness,
                                                 m_data.use_blending,
                                                 tid);

    w.complete_message(x);
}

void MaterialT::update(MaterialData const& d, Writer& w) {
    m_data = d;

    // same message is used
    write_new_to(w);
}

void MaterialT::update(MaterialData const& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void MaterialT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateMaterialDelete(w, lid);

    w.complete_message(x);
}

} // namespace noo
