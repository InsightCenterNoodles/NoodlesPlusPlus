#include "plotlist.h"

#include "noodlesserver.h"
#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"
#include "src/generated/noodles_generated.h"
#include "src/server/tablelist.h"

namespace noo {

PlotList::PlotList(ServerT* s) : ComponentListBase(s) { }
PlotList::~PlotList() = default;

PlotT::PlotT(IDType id, PlotList* host, PlotData const& d)
    : ComponentMixin(id, host), m_data(d) { }

struct PlotTWriteSelect {
    bool def    = false;
    bool table  = false;
    bool method = false;
    bool signal = false;
};

template <class T>
using OffsetList = std::vector<flatbuffers::Offset<T>>;

void PlotT::write_common(Writer& w, PlotTWriteSelect const& s) {

    auto lid = convert_id(id(), w);

    noodles::PlotType plot_type = noodles::PlotType::NONE;
    std::optional<flatbuffers::Offset<void>> update_definition;

    std::optional<flatbuffers::Offset<noodles::TableID>> update_table;
    std::optional<OffsetList<noodles::MethodID>>         update_methods_list;
    std::optional<OffsetList<noodles::SignalID>>         update_signals_list;

    if (s.def) {
        update_definition = VMATCH(
            m_data.definition,
            VCASE(SimplePlotDef const& s) {
                plot_type     = noodles::PlotType::SimplePlot;
                auto eightbit = s.definition.toUtf8();
                return noodles::CreateSimplePlotDirect(w, eightbit.data())
                    .Union();
            },
            VCASE(URLPlotDef const& s) {
                plot_type     = noodles::PlotType::URLPlot;
                auto eightbit = s.url.toString().toUtf8();
                return noodles::CreateURLPlotDirect(w, eightbit.data()).Union();
            });
    }

    if (s.table) { update_table = convert_id(m_data.table_link, w); }

    if (s.method) { update_methods_list = make_id_list(m_data.method_list, w); }
    if (s.signal) { update_signals_list = make_id_list(m_data.signal_list, w); }


    auto x = noodles::CreatePlotCreateUpdateDirect(
        w,
        lid,
        update_table ? *update_table : 0,
        plot_type,
        update_definition ? *update_definition : 0,
        update_methods_list ? &update_methods_list.value() : nullptr,
        update_signals_list ? &update_signals_list.value() : nullptr);

    w.complete_message(x);
}

void PlotT::write_new_to(Writer& w) {

    PlotTWriteSelect s {
        .def    = true,
        .table  = true,
        .method = true,
        .signal = true,
    };

    write_common(w, s);
}

void PlotT::update(PlotUpdateData const& data, Writer& w) {
    PlotTWriteSelect s {
        .def    = data.definition.has_value(),
        .table  = data.table_link.has_value(),
        .method = data.method_list.has_value(),
        .signal = data.signal_list.has_value(),
    };

    if (s.def) m_data.definition = data.definition.value();
    if (s.table) m_data.table_link = data.table_link.value();
    if (s.method) m_data.method_list = data.method_list.value();
    if (s.signal) m_data.signal_list = data.signal_list.value();


    write_common(w, s);
}

void PlotT::update(PlotUpdateData const& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void PlotT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreatePlotDelete(w, lid);

    w.complete_message(x);
}

} // namespace noo
