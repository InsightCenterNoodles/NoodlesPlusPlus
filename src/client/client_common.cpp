#include "client_common.h"

#include "include/noo_anyref.h"

#include <QDebug>

namespace nooc {

void SubscribeInitReply::interpret() {

    auto arg_map = m_var.toMap();

    if (arg_map.size() < 4) {
        qDebug() << "Malformed subscribe reply";
        emit recv_fail("Bad subscription reply!");
        return;
    }

    auto names = arg_map[QStringLiteral("columns")].toArray();
    auto keys  = arg_map[QStringLiteral("keys")];
    auto cols  = arg_map[QStringLiteral("data")].toArray();
    auto sels  = arg_map[QStringLiteral("selections")].toArray();

    emit recv(names, keys, cols, sels);
}

} // namespace nooc
