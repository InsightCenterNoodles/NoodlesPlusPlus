#include "tablelist.h"

#include "../shared/interface_tools.h"
#include "noodlesserver.h"
#include "noodlesstate.h"
#include "serialize.h"

namespace noo {

TableList::TableList(ServerT* s) : ComponentListBase(s) { }

TableList::~TableList() { }

// =============================================================================

TableT::TableT(IDType id, TableList* host, TableData const& d)
    : ComponentMixin(id, host), m_data(d) {

    // load signals

    auto doc = host->server()->state()->document();

    for (auto e : { BuiltinMethods::TABLE_SUBSCRIBE,
                    BuiltinMethods::TABLE_GET_COLUMNS,
                    BuiltinMethods::TABLE_GET_NUM_ROWS,
                    BuiltinMethods::TABLE_GET_ROW,
                    BuiltinMethods::TABLE_GET_BLOCK,
                    BuiltinMethods::TABLE_REQUEST_ROW_INSERT,
                    BuiltinMethods::TABLE_REQUEST_ROW_UPDATE,
                    BuiltinMethods::TABLE_REQUEST_ROW_APPEND,
                    BuiltinMethods::TABLE_REQUEST_DELETION,
                    BuiltinMethods::TABLE_GET_SELECTION,
                    BuiltinMethods::TABLE_REQUEST_SET_SELECTION }) {
        m_method_list.insert(doc->get_builtin(e));
    }

    for (auto e : { BuiltinSignals::TABLE_SIG_RESET,
                    BuiltinSignals::TABLE_SIG_ROWS_ADDED,
                    BuiltinSignals::TABLE_SIG_ROWS_DELETED,
                    BuiltinSignals::TABLE_SIG_DATA_UPDATED,
                    BuiltinSignals::TABLE_SIG_SELECTION_CHANGED }) {
        m_signal_list.insert(doc->get_builtin(e));
    }

    connect(m_data.source.get(),
            &TableSource::table_selection_updated,
            this,
            &TableT::on_table_selection_updated);

    connect(m_data.source.get(),
            &TableSource::table_row_added,
            this,
            &TableT::on_table_row_added);

    connect(m_data.source.get(),
            &TableSource::table_row_deleted,
            this,
            &TableT::on_table_row_deleted);

    connect(m_data.source.get(),
            &TableSource::table_row_updated,
            this,
            &TableT::on_table_row_updated);

    connect(m_data.source.get(),
            &TableSource::sig_reset,
            this,
            &TableT::on_sig_reset);
}

AttachedMethodList& TableT::att_method_list() {
    return m_method_list;
}
AttachedSignalList& TableT::att_signal_list() {
    return m_signal_list;
}

void TableT::write_new_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto update_methods_list =
        make_id_list(m_method_list.begin(), m_method_list.end(), w);

    auto update_signals_list =
        make_id_list(m_signal_list.begin(), m_signal_list.end(), w);


    auto x = noodles::CreateTableCreateUpdateDirect(w,
                                                    lid,
                                                    m_data.name.c_str(),
                                                    m_data.meta.c_str(),
                                                    &update_methods_list,
                                                    &update_signals_list);

    w.complete_message(x);
}

void TableT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateTableDelete(w, lid);

    w.complete_message(x);
}

TableSource* TableT::get_source() const {
    return m_data.source.get();
}

static SignalTPtr get_builtin_signal(TableT& n, BuiltinSignals s) {
    return n.hosting_list()->server()->state()->document()->get_builtin(s);
}

template <class... Args>
static void send_table_signal(TableT& n, BuiltinSignals bs, Args&&... args) {
    auto sig = get_builtin_signal(n, bs);

    if (!sig) return;

    auto to_send = marshall_to_any(std::forward<Args>(args)...);

    sig->fire(n.id(), std::move(to_send));
}


void TableT::on_table_selection_updated(std::string id) {
    send_table_signal(*this, BuiltinSignals::TABLE_SIG_SELECTION_CHANGED, id);
}

void TableT::on_table_row_added(int64_t at, int64_t count) {
    send_table_signal(*this, BuiltinSignals::TABLE_SIG_ROWS_ADDED, at, count);
}

void TableT::on_table_row_deleted(Selection s) {
    send_table_signal(
        *this, BuiltinSignals::TABLE_SIG_ROWS_DELETED, s.to_any());
}

void TableT::on_table_row_updated(Selection s) {
    send_table_signal(
        *this, BuiltinSignals::TABLE_SIG_DATA_UPDATED, s.to_any());
}

void TableT::on_sig_reset() {
    send_table_signal(*this, BuiltinSignals::TABLE_SIG_RESET);
}

} // namespace noo
