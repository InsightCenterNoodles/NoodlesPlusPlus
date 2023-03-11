#include "plotlist.h"

#include "noodlesserver.h"
#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"
#include "src/server/tablelist.h"

namespace noo {

PlotList::PlotList(ServerT* s) : ComponentListBase(s) { }
PlotList::~PlotList() = default;

PlotT::PlotT(IDType id, PlotList* host, PlotData const& d)
    : ComponentMixin(id, host), m_data(d) {
    if (m_data.method_list) m_method_search = *m_data.method_list;
    if (m_data.signal_list) m_signal_search = *m_data.signal_list;
}

template <class Message, class UP>
void write_common(PlotT* plt, UP const& data, Message& m) {

    m.id = plt->id();

    if (data.table_link) { m.table = (*data.table_link)->id(); }

    if (data.method_list) {
        m.methods_list = delegates_to_ids(data.method_list.value());
    }

    if (data.signal_list) {
        m.signals_list = delegates_to_ids(data.signal_list.value());
    }

    if (data.definition) {
        VMATCH(
            *data.definition,
            VCASE(QString const& s) { m.simple_plot = s; },
            VCASE(QUrl const& s) { m.url_plot = s.toString(); });
    }
}

void PlotT::write_new_to(SMsgWriter& w) {

    messages::MsgPlotCreate m;
    m.name = opt_string(m_data.name);

    write_common(this, m_data, m);

    w.add(m);
}

#define CHECK_UPDATE(FLD)                                                      \
    {                                                                          \
        if (data.FLD) { m_data.FLD = std::move(*data.FLD); }                   \
    }

void PlotT::update(PlotUpdateData const& data, SMsgWriter& w) {

    messages::MsgPlotUpdate m;

    write_common(this, m_data, m);

    w.add(m);

    CHECK_UPDATE(definition)
    CHECK_UPDATE(table_link)
    CHECK_UPDATE(method_list)
    CHECK_UPDATE(signal_list)

    if (data.method_list) { m_method_search = *m_data.method_list; }
    if (data.signal_list) { m_signal_search = *m_data.signal_list; }
}

void PlotT::update(PlotUpdateData const& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void PlotT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgPlotDelete { .id = id() });
}

AttachedMethodList& PlotT::att_method_list() {
    return m_method_search;
}
AttachedSignalList& PlotT::att_signal_list() {
    return m_signal_search;
}

} // namespace noo
