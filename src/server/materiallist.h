#pragma once

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_server_interface.h"

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

    MaterialData const& data() const { return m_data; }

    void write_new_to(SMsgWriter&);
    void update(MaterialUpdateData const&, SMsgWriter&);
    void update(MaterialUpdateData const&);
    void write_delete_to(SMsgWriter&);
};

} // namespace noo
