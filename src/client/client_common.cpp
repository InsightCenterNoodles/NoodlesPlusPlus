#include "client_common.h"

#include "include/noo_anyref.h"

#include <QDebug>

namespace nooc {

void SubscribeInitReply::interpret() {

    auto arg_map = m_var.to_map();

    if (arg_map.size() < 4) {
        qDebug() << "Malformed subscribe reply";
        emit recv_fail("Bad subscription reply!");
        return;
    }

    auto names = arg_map["columns"].to_vector();
    auto keys  = arg_map["keys"];
    auto cols  = arg_map["data"].to_vector();
    auto sels  = arg_map["selections"].to_vector();

    emit recv(names, keys, cols, sels);
}

} // namespace nooc
