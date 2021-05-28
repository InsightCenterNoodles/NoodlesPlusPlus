#include "texturelist.h"

#include "bufferlist.h"
#include "noodlesserver.h"
#include "serialize.h"
#include "src/generated/interface_tools.h"

namespace noo {

TextureList::TextureList(ServerT* s) : ComponentListBase(s) { }

TextureList::~TextureList() { }

// =============================================================================

TextureT::TextureT(IDType id, TextureList* host, TextureData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void TextureT::write_new_to(Writer& w) {
    auto tid = convert_id(id(), w);

    auto bid = convert_id(m_data.buffer, w);

    auto buffer = noodles::CreateBufferRef(w, bid, m_data.start, m_data.size);

    auto x = noodles::CreateTextureCreateUpdate(w, tid, buffer);

    w.complete_message(x);
}

void TextureT::update(TextureData const& data, Writer& w) {
    m_data = data;

    // we use the same message here
    write_new_to(w);
}

void TextureT::update(TextureData const& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void TextureT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);
    auto x   = noodles::CreateTextureDelete(w, lid);
    w.complete_message(x);
}

} // namespace noo
