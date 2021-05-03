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
    void recv(noo::AnyVarListRef const&,
              noo::AnyVarRef,
              noo::AnyVarListRef const&,
              noo::AnyVarListRef const&);
};

} // namespace nooc

#endif // CLIENT_COMMON_H
