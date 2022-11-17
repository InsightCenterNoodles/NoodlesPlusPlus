#pragma once

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_server_interface.h"
#include <qurl.h>
#include <quuid.h>

namespace noo {


class BufferList : public ComponentListBase<BufferList, BufferID, BufferT> {
public:
    BufferList(ServerT*);
    ~BufferList();
};


class BufferT : public ComponentMixin<BufferT, BufferList, BufferID> {
    BufferData m_data;

    QUuid m_asset_id;
    QUrl  m_asset_url;

public:
    BufferT(IDType, BufferList*, BufferData const&);
    ~BufferT() override;

    void write_new_to(SMsgWriter&);

    void write_delete_to(SMsgWriter&);

    BufferData const& data() const { return m_data; }
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

    auto const& data() const { return m_data; }

    void write_new_to(SMsgWriter&);

    void write_delete_to(SMsgWriter&);
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

    auto const& data() const { return m_data; }

    void write_new_to(SMsgWriter&);
    void update(LightUpdateData const&, SMsgWriter&);
    void update(LightUpdateData const&);
    void write_delete_to(SMsgWriter&);
};

} // namespace noo
