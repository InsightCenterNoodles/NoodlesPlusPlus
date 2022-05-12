#pragma once

#include <QCborArray>
#include <QCborValue>
#include <QWebSocket>

namespace nooc {

class ClientWriter {
    QWebSocket& m_socket;
    QCborArray  m_message_list;

    bool m_written = false;

public:
    ClientWriter(QWebSocket& s);
    ~ClientWriter();

    void add(QCborValue message, unsigned message_id);

    void flush();
};

// =============================================================================

class InternalClientState;

void process_message(QWebSocket& s, InternalClientState& state, QByteArray m);

} // namespace nooc
