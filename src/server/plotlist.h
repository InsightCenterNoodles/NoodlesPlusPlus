#ifndef NOO_PLOTLIST_H
#define NOO_PLOTLIST_H

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_server_interface.h"
#include "search_helpers.h"

namespace noo {

class PlotList : public ComponentListBase<PlotList, PlotID, PlotT> {
public:
    PlotList(ServerT*);
    ~PlotList();
};

class PlotT : public ComponentMixin<PlotT, PlotList, PlotID> {
    PlotData m_data;

    AttachedMethodList m_method_search;
    AttachedSignalList m_signal_search;

public:
    PlotT(IDType, PlotList*, PlotData const&);

    auto const& data() const { return m_data; }

    AttachedMethodList& att_method_list();
    AttachedSignalList& att_signal_list();

    void write_new_to(SMsgWriter&);
    void update(PlotUpdateData const&, SMsgWriter&);
    void update(PlotUpdateData const&);
    void write_delete_to(SMsgWriter&);
};

} // namespace noo

#endif // NOO_PLOTLIST_H
