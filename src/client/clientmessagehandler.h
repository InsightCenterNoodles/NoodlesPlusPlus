#pragma once

#include "src/common/serialize.h"

#include <QCborArray>
#include <QCborValue>
#include <QWebSocket>

namespace nooc {

class ClientWriter {
    QWebSocket&                               m_socket;
    std::vector<noo::messages::ClientMessage> m_message_list;

public:
    ClientWriter(QWebSocket& s);
    ~ClientWriter();

    void add(noo::messages::ClientMessage);

    void flush();
};

// =============================================================================

class InternalClientState;

void process_message(QWebSocket& s, InternalClientState& state, QByteArray m);

} // namespace nooc
