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
    void
    recv(QCborArray const&, QCborValue, QCborArray const&, QCborArray const&);
};

} // namespace nooc

#endif // CLIENT_COMMON_H
