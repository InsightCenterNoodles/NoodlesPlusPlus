#ifndef TEXTURELIST_H
#define TEXTURELIST_H

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_server_interface.h"

namespace noo {

class TextureList : public ComponentListBase<TextureList, TextureID, TextureT> {
public:
    TextureList(ServerT*);
    ~TextureList();
};

class TextureT : public ComponentMixin<TextureT, TextureList, TextureID> {
    TextureData m_data;

public:
    TextureT(IDType, TextureList*, TextureData const&);


    void write_new_to(SMsgWriter&);
    void write_delete_to(SMsgWriter&);
};

// =============================================================================

class ImageList : public ComponentListBase<ImageList, ImageID, ImageT> {
public:
    ImageList(ServerT*);
    ~ImageList();
};

class ImageT : public ComponentMixin<ImageT, ImageList, ImageID> {
    ImageData m_data;

public:
    ImageT(IDType, ImageList*, ImageData const&);


    void write_new_to(SMsgWriter&);
    void write_delete_to(SMsgWriter&);
};

// =============================================================================

class SamplerList : public ComponentListBase<SamplerList, SamplerID, SamplerT> {
public:
    SamplerList(ServerT*);
    ~SamplerList();
};

class SamplerT : public ComponentMixin<SamplerT, SamplerList, SamplerID> {
    SamplerData m_data;

public:
    SamplerT(IDType, SamplerList*, SamplerData const&);


    void write_new_to(SMsgWriter&);
    void write_delete_to(SMsgWriter&);
};

} // namespace noo


#endif // TEXTURELIST_H
