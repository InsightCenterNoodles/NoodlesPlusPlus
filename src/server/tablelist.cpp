#include "tablelist.h"

#include "noodlesserver.h"
#include "noodlesstate.h"
#include "serialize.h"
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
            &TableSource::table_selection_updated,
            this,
            &TableT::on_table_selection_updated);

    connect(m_data.source.get(),
            &TableSource::table_row_deleted,
            this,
            &TableT::on_table_row_deleted);

    connect(m_data.source.get(),
            &TableSource::table_row_updated,
            this,
            &TableT::on_table_row_updated);

    connect(m_data.source.get(),
            &TableSource::table_reset,
            this,
            &TableT::on_table_reset);
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


void TableT::on_table_selection_updated(std::string         name,
                                        SelectionRef const& ref) {
    send_table_signal(
        *this, BuiltinSignals::TABLE_SIG_SELECTION_CHANGED, name, ref.to_any());
    qDebug() << "Table emit" << Q_FUNC_INFO
             << QString::fromStdString(ref.to_any().dump_string());
}

void TableT::on_table_row_deleted(TableQueryPtr q) {
    // TODO clean up
    std::vector<int64_t> keys;
    keys.resize(q->num_rows);

    q->get_keys_to(keys);

    AnyVar v = std::move(keys);

    send_table_signal(*this, BuiltinSignals::TABLE_SIG_ROWS_DELETED, v);

    qDebug() << "Table emit" << Q_FUNC_INFO
             << QString::fromStdString(v.dump_string());
}

void TableT::on_table_row_updated(TableQueryPtr q) {
    AnyVar kv;
    AnyVar cols;

    {
        std::vector<int64_t> keys;
        keys.resize(q->num_rows);

        q->get_keys_to(keys);

        kv = std::move(keys);
    }

    {
        AnyVarList l;
        l.reserve(q->num_cols);

        for (size_t i = 0; i < q->num_cols; i++) {
            AnyVar this_c;

            if (q->is_column_string(i)) {
                AnyVarList avl(q->num_rows);

                for (size_t row_i = 0; row_i < avl.size(); row_i++) {
                    std::string_view value_view;

                    q->get_cell_to(i, row_i, value_view);

                    avl[i] = std::string(value_view);
                }

                this_c = std::move(avl);

            } else {
                std::vector<double> d(q->num_rows);

                q->get_reals_to(i, d);

                this_c = std::move(d);
            }

            l.emplace_back(std::move(this_c));
        }

        cols = std::move(l);
    }

    send_table_signal(*this, BuiltinSignals::TABLE_SIG_DATA_UPDATED, kv, cols);

    qDebug() << "Table emit" << Q_FUNC_INFO
             << QString::fromStdString(kv.dump_string())
             << QString::fromStdString(cols.dump_string());
}

void TableT::on_table_reset() {
    send_table_signal(*this, BuiltinSignals::TABLE_SIG_RESET);
    qDebug() << "Table emit" << Q_FUNC_INFO;
}

} // namespace noo
