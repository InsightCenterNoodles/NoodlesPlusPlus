#ifndef BUFFERLIST_H
#define BUFFERLIST_H

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_server_interface.h"

namespace noo {


class BufferList : public ComponentListBase<BufferList, BufferID, BufferT> {
public:
    BufferList(ServerT*);
    ~BufferList();
};


class BufferT : public ComponentMixin<BufferT, BufferList, BufferID> {
    BufferData m_data;

public:
    BufferT(IDType, BufferList*, BufferData const&);

    void write_new_to(Writer&);

    void write_delete_to(Writer&);
};


// =============================================================================

class BufferViewList
    : public ComponentListBase<BufferViewList, BufferViewID, BufferViewT> {
public:
    BufferViewList(ServerT*);
    ~BufferViewList();
};


class BufferViewT
    : public ComponentMixin<BufferViewT, BufferViewList, BufferViewID> {
    BufferViewData m_data;

public:
    BufferViewT(IDType, BufferViewList*, BufferViewData const&);

    void write_new_to(Writer&);

    void write_delete_to(Writer&);
};

// =============================================================================

class LightList : public ComponentListBase<LightList, LightID, LightT> {
public:
    LightList(ServerT*);
    ~LightList();
};


class LightT : public ComponentMixin<LightT, LightList, LightID> {
    LightData m_data;

public:
    LightT(IDType, LightList*, LightData const&);

    void write_new_to(Writer&);
    void update(LightUpdateData const&, Writer&);
    void update(LightUpdateData const&);
    void write_delete_to(Writer&);
};

} // namespace noo

#endif // BUFFERLIST_H
