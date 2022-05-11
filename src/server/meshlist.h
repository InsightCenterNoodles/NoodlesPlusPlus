#ifndef MESHLIST_H
#define MESHLIST_H

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_server_interface.h"

namespace noo {

class MeshList : public ComponentListBase<MeshList, GeometryID, MeshT> {
public:
    MeshList(ServerT*);
    ~MeshList();
};


class MeshT : public ComponentMixin<MeshT, MeshList, GeometryID> {
    MeshData m_data;

public:
    MeshT(IDType, MeshList*, MeshData const&);

    void write_new_to(Writer&);
    void write_delete_to(Writer&);
};

} // namespace noo

#endif // MESHLIST_H
