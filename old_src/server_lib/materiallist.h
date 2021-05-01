#ifndef MATERIALLIST_H
#define MATERIALLIST_H

#include "../shared/id.h"
#include "componentlistbase.h"
#include "server_interface.h"

namespace noo {

class MaterialList
    : public ComponentListBase<MaterialList, MaterialID, MaterialT> {
public:
    MaterialList(ServerT*);
    ~MaterialList();
};


class MaterialT : public ComponentMixin<MaterialT, MaterialList, MaterialID> {
    MaterialData m_data;

public:
    MaterialT(IDType, MaterialList*, MaterialData const&);

    void write_new_to(Writer&);
    void update(MaterialData const&, Writer&);
    void update(MaterialData const&);
    void write_delete_to(Writer&);
};

} // namespace noo

#endif // MATERIALLIST_H
