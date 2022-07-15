#include "tablelist.h"

#include "noodlesserver.h"
#include "noodlesstate.h"
#include "src/common/serialize.h"
#include "src/generated/interface_tools.h"

namespace noo {

TableList::TableList(ServerT* s) : ComponentListBase(s) { }

TableList::~TableList() { }

// =============================================================================

TableT::TableT(IDType id, TableList* host, TableData const& d)
    : ComponentMixin(id, host), m_data(d) {

    // load signals

    auto doc = host->server()->state()->document();

    for (auto e : { BuiltinMethods::TABLE_SUBSCRIBE,
                    BuiltinMethods::TABLE_INSERT,
                    BuiltinMethods::TABLE_UPDATE,
                    BuiltinMethods::TABLE_REMOVE,
                    BuiltinMethods::TABLE_CLEAR,
                    BuiltinMethods::TABLE_UPDATE_SELECTION }) {
        m_method_list.insert(doc->get_builtin(e));
    }

    for (auto e : { BuiltinSignals::TABLE_SIG_RESET,
                    BuiltinSignals::TABLE_SIG_ROWS_DELETED,
                    BuiltinSignals::TABLE_SIG_DATA_UPDATED,
                    BuiltinSignals::TABLE_SIG_SELECTION_CHANGED }) {
        m_signal_list.insert(doc->get_builtin(e));
    }

    connect(m_data.source.get(),
            &ServerTableDelegate::table_selection_updated,
            this,
            &TableT::on_table_selection_updated);

    connect(m_data.source.get(),
            &ServerTableDelegate::table_row_deleted,
            this,
            &TableT::on_table_row_deleted);

    connect(m_data.source.get(),
            &ServerTableDelegate::table_row_updated,
            this,
            &TableT::on_table_row_updated);

    connect(m_data.source.get(),
            &ServerTableDelegate::table_reset,
            this,
            &TableT::on_table_reset);
}

AttachedMethodList& TableT::att_method_list() {
    return m_method_list;
}
AttachedSignalList& TableT::att_signal_list() {
    return m_signal_list;
}

void TableT::write_new_to(SMsgWriter& w) {

    messages::MsgTableCreate m;
    m.id   = id();
    m.name = opt_string(m_data.name);
    m.meta = opt_string(m_data.meta);

    m.methods_list =
        delegates_to_ids(m_method_list.begin(), m_method_list.end());

    m.signals_list =
        delegates_to_ids(m_signal_list.begin(), m_signal_list.end());

    w.add(m);
}

void TableT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgTableDelete { .id = id() });
}

ServerTableDelegate* TableT::get_source() const {
    return m_data.source.get();
}

static SignalTPtr get_builtin_signal(TableT& n, BuiltinSignals s) {
    return n.hosting_list()->server()->state()->document()->get_builtin(s);
}

template <class... Args>
static void send_table_signal(TableT& n, BuiltinSignals bs, Args&&... args) {
    auto sig = get_builtin_signal(n, bs);

    if (!sig) return;

    auto to_send = convert_to_cbor_array(std::forward<Args>(args)...);

    sig->fire(n.id(), std::move(to_send));
}


void TableT::on_table_selection_updated(Selection const& ref) {
    //    qDebug() << "Table emit" << Q_FUNC_INFO
    //             << QString::fromStdString(ref.to_any().dump_string());

    send_table_signal(
        *this, BuiltinSignals::TABLE_SIG_SELECTION_CHANGED, ref.to_cbor());
}

void TableT::on_table_row_deleted(QCborArray keys) {
    send_table_signal(*this, BuiltinSignals::TABLE_SIG_ROWS_DELETED, keys);
}

void TableT::on_table_row_updated(QCborArray keys, QCborArray rows) {
    send_table_signal(
        *this, BuiltinSignals::TABLE_SIG_DATA_UPDATED, keys, rows);
}

void TableT::on_table_reset() {
    // qDebug() << "Table emit" << Q_FUNC_INFO;

    auto map = make_table_init_data(*this->get_source());

    send_table_signal(*this, BuiltinSignals::TABLE_SIG_RESET, map);
}


QCborMap make_table_init_data(ServerTableDelegate& source) {
    QCborMap return_obj;

    auto all_data = source.get_all_data();

    {
        QCborArray column_info;

        auto all_names = source.get_headers();

        // "TEXT" / "REAL" / "INTEGER" / "ANY"

        for (auto const& name : source.get_headers()) {

            QCborMap map;
            map[QStringLiteral("name")] = name;
            map[QStringLiteral("type")] = "ANY";

            column_info << map;
        }

        return_obj[QStringLiteral("columns")] = column_info;
    }

    return_obj[QStringLiteral("keys")] = all_data.first;
    return_obj[QStringLiteral("data")] = all_data.second;

    {
        auto const& selections = source.get_all_selections();

        QCborArray lv;

        for (auto const& sel : selections) {
            lv << to_cbor(sel);
        }

        return_obj[QStringLiteral("selections")] = std::move(lv);
    }

    return return_obj;
}

} // namespace noo
