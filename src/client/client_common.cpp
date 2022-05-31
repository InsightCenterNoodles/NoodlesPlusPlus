#include "client_common.h"

#include <QDebug>

namespace nooc {

void SubscribeInitReply::interpret() {

    noo::CborDecoder decoder(m_var.toMap());

    QVector<TableDelegate::ColumnInfo> names;
    QVector<int64_t>                   keys;
    QVector<QCborArray>                data_columns;
    QVector<noo::Selection>            selections;

    decoder(QStringLiteral("columns"), names);
    decoder(QStringLiteral("keys"), keys);
    decoder(QStringLiteral("data"), data_columns);
    decoder.conditional(QStringLiteral("selections"), selections);

    emit recv(names, keys, data_columns, selections);
}

} // namespace nooc
