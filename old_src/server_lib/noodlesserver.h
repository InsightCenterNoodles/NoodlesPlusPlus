#ifndef NOODLESSERVER_H
#define NOODLESSERVER_H

#include "../shared/id.h"

#include <QObject>
#include <QSet>

#include <unordered_set>

class QWebSocketServer;
class QWebSocket;

namespace noo {

class Writer;
class NoodlesState;
class TableT;
class DocumentT;

class IncomingMessage;

// =============================================================================

class ClientT : public QObject {
    Q_OBJECT

    QString     m_name;
    QWebSocket* m_socket;

    size_t m_bytes_counter = 0;

public:
    ClientT(QWebSocket*, QObject*);
    ~ClientT();

    void set_name(std::string const&);

    void kill();

public slots:
    void send(QByteArray);

private slots:
    void on_text(QString);
    void on_binary(QByteArray);

signals:
    void finished();
    void message_recvd(std::shared_ptr<IncomingMessage>);
};

// =============================================================================

class ServerT : public QObject {
    Q_OBJECT

    NoodlesState* m_state;

    QWebSocketServer* m_socket_server;

    QSet<ClientT*> m_connected_clients;


public:
    explicit ServerT(quint16 port = 50000, QObject* parent = nullptr);

    NoodlesState* state();

    std::unique_ptr<Writer> get_broadcast_writer();
    std::unique_ptr<Writer> get_single_client_writer(ClientT&);
    std::unique_ptr<Writer> get_table_subscribers_writer(TableT&);

public slots:
    void broadcast(QByteArray);

private slots:
    void on_new_connection();
    void on_client_done();

    void on_client_message(std::shared_ptr<IncomingMessage>);

signals:
};

} // namespace noo

#endif // NOODLESSERVER_H
