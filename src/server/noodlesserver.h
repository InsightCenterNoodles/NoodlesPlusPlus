#pragma once

#include "include/noo_id.h"

#include <QObject>
#include <QSet>

#include <memory>
#include <unordered_set>

class QWebSocketServer;
class QWebSocket;

namespace noo {

namespace messages {
class ClientMessage;
}

class SMsgWriter;
class NoodlesState;
class TableT;
class DocumentT;
class ServerOptions;

class IncomingMessage;

// =============================================================================

class ClientT : public QObject {
    Q_OBJECT

    QString     m_name;
    QWebSocket* m_socket;
    // bool        m_use_binary = true;

    size_t m_bytes_counter = 0;

public:
    ClientT(QWebSocket*, QObject*);
    ~ClientT();

    void set_name(QString);

    void kill();

public slots:
    void send(QByteArray);

private slots:
    void on_text(QString);
    void on_binary(QByteArray);

signals:
    void finished();
    void message_recvd(QVector<messages::ClientMessage> const&);
};

// =============================================================================

class ServerT : public QObject {
    Q_OBJECT

    NoodlesState* m_state;

    QWebSocketServer* m_socket_server;

    QSet<ClientT*> m_connected_clients;


public:
    explicit ServerT(ServerOptions const& options, QObject* parent = nullptr);

    quint16 port() const;

    NoodlesState* state();

    std::unique_ptr<SMsgWriter> get_broadcast_writer();
    std::unique_ptr<SMsgWriter> get_single_client_writer(ClientT&);
    std::unique_ptr<SMsgWriter> get_table_subscribers_writer(TableT&);

public slots:
    void broadcast(QByteArray);

private slots:
    void on_new_connection();
    void on_client_done();

    void on_client_message(QVector<messages::ClientMessage> const&);

signals:
};

} // namespace noo
