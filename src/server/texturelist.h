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


    void write_new_to(Writer&);
    void update(TextureData const&, Writer&);
    void update(TextureData const&);
    void write_delete_to(Writer&);
};

} // namespace noo


#endif // TEXTURELIST_H
