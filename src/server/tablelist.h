#ifndef TABLELIST_H
#define TABLELIST_H

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_server_interface.h"
#include "search_helpers.h"

#include <QByteArray>

#include <unordered_set>

namespace noo {

class TableList : public ComponentListBase<TableList, TableID, TableT> {
public:
    TableList(ServerT*);
    ~TableList();
};


class TableT : public ComponentMixin<TableT, TableList, TableID> {
    Q_OBJECT

    TableData m_data;

    AttachedMethodList m_method_list;
    AttachedSignalList m_signal_list;

public:
    TableT(IDType, TableList*, TableData const&);

    auto const& data() const { return m_data; }

    AttachedMethodList& att_method_list();
    AttachedSignalList& att_signal_list();

    void write_new_to(SMsgWriter&);
    void write_delete_to(SMsgWriter&);

    ServerTableDelegate* get_source() const;

signals:
    void send_data(QByteArray);

private slots:
    void on_table_reset();
    void on_table_selection_updated(Selection const&);
    void on_table_row_updated(QCborArray keys, QCborArray rows);
    void on_table_row_deleted(QCborArray keys);
};

QCborMap make_table_init_data(ServerTableDelegate& source);

} // namespace noo

#endif // TABLELIST_H
