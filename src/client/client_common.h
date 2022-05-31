#ifndef CLIENT_COMMON_H
#define CLIENT_COMMON_H

#include "include/noo_client_interface.h"

namespace nooc {

class SubscribeInitReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;

signals:
    void recv(QVector<TableDelegate::ColumnInfo> const&,
              QVector<int64_t>,
              QVector<QCborArray> const&,
              QVector<noo::Selection> const&);
};

} // namespace nooc

#endif // CLIENT_COMMON_H
