#include "texturelist.h"

#include "bufferlist.h"
#include "noodlesserver.h"
#include "src/common/serialize.h"
#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"

#include <magic_enum.hpp>

namespace noo {

TextureList::TextureList(ServerT* s) : ComponentListBase(s) { }

TextureList::~TextureList() { }

// =============================================================================

TextureT::TextureT(IDType id, TextureList* host, TextureData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void TextureT::write_new_to(SMsgWriter& w) {

    messages::MsgTextureCreate m {
        .id   = id(),
        .name = m_data.name,
    };
    if (m_data.sampler) { m.sampler = m_data.sampler->id(); }
    if (m_data.image) { m.image = m_data.image->id(); }

    w.add(m);
}

void TextureT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgTextureDelete { .id = id() });
}

// =============================================================================


ImageList::ImageList(ServerT* s) : ComponentListBase(s) { }

ImageList::~ImageList() { }

ImageT::ImageT(IDType id, ImageList* host, ImageData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void ImageT::write_new_to(SMsgWriter& w) {

    messages::MsgImageCreate m {
        .id   = id(),
        .name = opt_string(m_data.name),
    };

    VMATCH(
        m_data.source,
        VCASE(QUrl const& url) { m.uri_source = url; },
        VCASE(BufferViewTPtr const& ptr) { m.buffer_source = ptr->id(); })

    w.add(m);
}

void ImageT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgImageDelete { .id = id() });
}

// =============================================================================


SamplerList::SamplerList(ServerT* s) : ComponentListBase(s) { }

SamplerList::~SamplerList() { }

SamplerT::SamplerT(IDType id, SamplerList* host, SamplerData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void SamplerT::write_new_to(SMsgWriter& w) {

    messages::MsgSamplerCreate m {
        .id         = id(),
        .name       = opt_string(m_data.name),
        .mag_filter = to_qstring(magic_enum::enum_name(m_data.mag_filter)),
        .min_filter = to_qstring(magic_enum::enum_name(m_data.min_filter)),
        .wrap_s     = to_qstring(magic_enum::enum_name(m_data.wrap_s)),
        .wrap_t     = to_qstring(magic_enum::enum_name(m_data.wrap_t)),
    };

    w.add(m);
}

void SamplerT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgSamplerDelete { .id = id() });
}

} // namespace noo
