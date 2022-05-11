#pragma once

#include <QCborArray>
#include <QCborValue>
#include <QWebSocket>

namespace nooc {

class ClientWriter {
    QWebSocket& m_socket;
    QCborArray  m_message_list;

    bool m_written = false;

    void finished_writing_and_export();

public:
    ClientWriter(QWebSocket& s);
    ~ClientWriter();

    void complete_message(QCborValue message, unsigned message_id);
};

// =============================================================================

class ClientState;

void process_message(QWebSocket& s, ClientState& state, QByteArray m);

} // namespace nooc
