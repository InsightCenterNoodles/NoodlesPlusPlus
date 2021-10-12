#ifndef NOO_PLOTLIST_H
#define NOO_PLOTLIST_H

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_server_interface.h"

namespace noo {

class PlotList : public ComponentListBase<PlotList, PlotID, PlotT> {
public:
    PlotList(ServerT*);
    ~PlotList();
};

struct PlotTWriteSelect;

class PlotT : public ComponentMixin<PlotT, PlotList, PlotID> {
    PlotData m_data;

    void write_common(Writer&, PlotTWriteSelect const&);

public:
    PlotT(IDType, PlotList*, PlotData const&);

    void write_new_to(Writer&);
    void update(PlotUpdateData const&, Writer&);
    void update(PlotUpdateData const&);
    void write_delete_to(Writer&);
};

} // namespace noo

#endif // NOO_PLOTLIST_H
