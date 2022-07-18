#include "noodlesserver.h"

#include "noodlesstate.h"
#include "src/common/serialize.h"
#include "src/common/variant_tools.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QWebSocket>
#include <QWebSocketServer>

namespace noo {

// =============================================================================

ClientT::ClientT(QWebSocket* socket, QObject* parent)
    : QObject(parent), m_socket(socket) {
    socket->setParent(this);
    connect(socket, &QWebSocket::disconnected, this, &ClientT::finished);

    connect(socket, &QWebSocket::textMessageReceived, this, &ClientT::on_text);
    connect(
        socket, &QWebSocket::binaryMessageReceived, this, &ClientT::on_binary);
}

ClientT::~ClientT() {
    qInfo() << "Client" << m_name << "closed, sent" << m_bytes_counter
            << "bytes";
}

void ClientT::on_text(QString text) {
    qCritical() << "Unable to handle text messages!";
    (void)text;
    //    m_use_binary = false;

    //    QJsonParseError err;

    //    auto doc = QJsonDocument::fromJson(text.toUtf8(), &err);

    //    auto ptr = std::make_shared<IncomingMessage>(text);

    //    if (ptr->messages().empty()) {
    //        qCritical() << "Bad message from ClientT" << this;
    //        kill();
    //        return;
    //    }

    //    emit message_recvd(ptr);
}

void ClientT::on_binary(QByteArray array) {
    // m_use_binary = true;

    auto messages = messages::deserialize_client(array);

    if (messages.empty()) {
        qCritical() << "Bad message from ClientT" << this;
        kill();
        return;
    }

    emit message_recvd(messages);
}

void ClientT::set_name(QString s) {
    m_name = s;
    qDebug() << "Identifying ClientT" << m_socket << "as" << m_name;
}

void ClientT::kill() {
    m_socket->close(QWebSocketProtocol::CloseCodeBadOperation,
                    "Killing ClientT");
}

void ClientT::send(QByteArray data) {
    m_bytes_counter += data.size();
    if (data.isEmpty()) return;

    m_socket->sendBinaryMessage(data);

    qDebug() << QCborValue::fromCbor(data).toDiagnosticNotation();

    //    if (m_use_binary) {
    //        m_socket->sendBinaryMessage(data);
    //    } else {
    //        qCritical() << "NOT YET IMPLEMENTED";
    //        // m_socket->sendTextMessage(message);
    //    }
}

// =============================================================================


ServerT::ServerT(quint16 port, QObject* parent) : QObject(parent) {

    m_state = new NoodlesState(this, port + 1);

    m_socket_server = new QWebSocketServer(QStringLiteral("Noodles Server"),
                                           QWebSocketServer::NonSecureMode,
                                           this);

    bool is_listening = m_socket_server->listen(QHostAddress::Any, port);

    if (!is_listening) return;

    connect(m_socket_server,
            &QWebSocketServer::newConnection,
            this,
            &ServerT::on_new_connection);
}

quint16 ServerT::port() const {
    return m_socket_server->serverPort();
}

NoodlesState* ServerT::state() {
    return m_state;
}

std::unique_ptr<SMsgWriter> ServerT::get_broadcast_writer() {
    auto p = std::make_unique<SMsgWriter>();

    connect(p.get(), &SMsgWriter::data_ready, this, &ServerT::broadcast);

    return p;
}

std::unique_ptr<SMsgWriter> ServerT::get_single_client_writer(ClientT& c) {
    auto p = std::make_unique<SMsgWriter>();

    connect(p.get(), &SMsgWriter::data_ready, &c, &ClientT::send);

    return p;
}

std::unique_ptr<SMsgWriter> ServerT::get_table_subscribers_writer(TableT& t) {
    auto p = std::make_unique<SMsgWriter>();

    connect(p.get(), &SMsgWriter::data_ready, &t, &TableT::send_data);

    return p;
}

void ServerT::broadcast(QByteArray ptr) {
    for (ClientT* c : qAsConst(m_connected_clients)) {
        c->send(ptr);
    }
}


void ServerT::on_new_connection() {
    QWebSocket* socket = m_socket_server->nextPendingConnection();

    ClientT* client = new ClientT(socket, this);

    qInfo() << "New client: " << socket->origin();

    connect(client, &ClientT::finished, this, &ServerT::on_client_done);
    connect(client, &ClientT::message_recvd, this, &ServerT::on_client_message);

    m_connected_clients.insert(client);
}

void ServerT::on_client_done() {
    ClientT* c = qobject_cast<ClientT*>(sender());

    if (!c) return;

    // qDebug() << Q_FUNC_INFO << c;

    m_connected_clients.remove(c);

    c->deleteLater();
}

class MessageHandler {
    ServerT* m_server;
    ClientT& m_client;

    NoodlesState& get_state() { return *(m_server->state()); }
    DocumentT&    get_document() { return *get_state().document(); }

    template <class List>
    void dump_list(List& l) {
        l.for_all([this](auto& item) {
            auto w = m_server->get_single_client_writer(m_client);

            item.write_new_to(*w);
            // TODO: Pack into a single message!
        });
    }

    void handle(messages::MsgIntroduction const& m) {
        m_client.set_name(m.client_name);

        // we must now reply with all of the stuff

        // we will dump in an order that should be not so bad on the client.
        // that means no undef references

        // first methods

        auto& d = get_document();

        dump_list(d.method_list());
        dump_list(d.signal_list());
        dump_list(d.light_list());
        dump_list(d.buffer_list());
        dump_list(d.buffer_view_list());
        dump_list(d.sampler_list());
        dump_list(d.image_list());
        dump_list(d.tex_list());
        dump_list(d.mat_list());
        dump_list(d.mesh_list());
        dump_list(d.table_list());
        dump_list(d.obj_list());

        auto w = m_server->get_single_client_writer(m_client);
        get_document().write_refresh(*w);
    }


    void send_method_ok_reply(QString const& id, QCborValue const& var) {
        if (!id.size()) return;

        auto w = m_server->get_single_client_writer(m_client);

        messages::MsgMethodReply m {
            .invoke_id = id,
            .result    = var,
        };

        w->add(m);
    }

    void send_method_error_reply(QString const&                   id,
                                 messages::MethodException const& exception) {
        if (!id.size()) return;

        auto w = m_server->get_single_client_writer(m_client);

        messages::MsgMethodReply m {
            .invoke_id        = id,
            .method_exception = exception,
        };

        w->add(m);
    }

    void send_table_reply(QString const&                   id,
                          TableT&                          table,
                          bool                             exclusive,
                          QCborValue const*                var,
                          messages::MethodException const* err) {

        if (!id.size()) return;

        auto w = exclusive ? m_server->get_single_client_writer(m_client)
                           : m_server->get_table_subscribers_writer(table);

        messages::MsgMethodReply m {
            .invoke_id = id,
        };

        if (var) {
            m.result = *var;
        } else {
            m.method_exception = *err;
        }

        w->add(m);
    }

    void handle(messages::MsgInvokeMethod const& message) {
        qDebug() << Q_FUNC_INFO;

        MethodContext             context;
        AttachedMethodList const* target_list = nullptr;
        bool                      is_table    = false;

        if (message.context) {

            VMATCH(
                *message.context,
                VCASE(std::monostate) {
                    qDebug() << "INVOKE on doc";
                    target_list = &(get_document().att_method_list());
                },
                VCASE(noo::EntityID id) {
                    qDebug() << "INVOKE on obj";

                    auto const& obj_ptr = get_document().obj_list().get_at(id);

                    if (obj_ptr) {
                        context     = obj_ptr;
                        target_list = &(obj_ptr->att_method_list());
                    }
                },
                VCASE(noo::TableID id) {
                    qDebug() << "INVOKE on table";

                    auto const& tbl_ptr =
                        get_document().table_list().get_at(id);

                    if (tbl_ptr) {
                        context     = tbl_ptr;
                        target_list = &(tbl_ptr->att_method_list());
                        is_table    = true;
                    }
                },
                VCASE(noo::PlotID id) {
                    qDebug() << "INVOKE on table";

                    auto const& plt_ptr = get_document().plot_list().get_at(id);

                    if (plt_ptr) {
                        context     = plt_ptr;
                        target_list = &(plt_ptr->att_method_list());
                    }
                }, );
        } else {
            qDebug() << "INVOKE on doc";
            target_list = &(get_document().att_method_list());
        }


        context.client = &m_client;

        QString id = message.invoke_id.value_or(QString());

        bool must_reply = id.size();

        if (!target_list) {
            if (must_reply) {
                send_method_error_reply(
                    id,
                    { ErrorCodes::METHOD_NOT_FOUND, "Method not found!", {} });
            }

            qWarning() << "unable to find method target!";
            return;
        }


        if (!message.method.valid()) {

            if (must_reply) {
                send_method_error_reply(
                    id,
                    { ErrorCodes::METHOD_NOT_FOUND,
                      "Unable to find method; bad method id!",
                      {} });
            }

            qWarning() << "Invalid method";
            return;
        }

        qDebug() << "METHOD ID" << message.method.id_slot
                 << message.method.id_gen;

        auto* method = target_list->find(message.method);

        if (!method) {
            if (must_reply) {
                send_method_error_reply(id,
                                        { ErrorCodes::METHOD_NOT_FOUND,
                                          "Unable to find method on context",
                                          {} });
            }
            qWarning() << "unable to find method" << message.method.id_slot;
            return;
        }

        auto const& function = method->function();

        if (!function) {
            if (must_reply) {
                send_method_error_reply(
                    id,
                    { ErrorCodes::INTERNAL_ERROR,
                      "Unable to execute method; method has no implementation.",
                      {} });
            }
            qWarning() << "method has no code";
            return;
        }

        {
            qDebug() << "Method arguments:"
                     << message.method.to_cbor().toDiagnosticNotation();
        }

        QCborValue                               ret_data;
        std::optional<messages::MethodException> err_state;

        try {
            ret_data = function(context, message.args);
        } catch (MethodException const& e) {
            auto& estate   = err_state.emplace();
            estate.code    = e.code();
            estate.message = e.reason();
            estate.data    = e.data();
        } catch (...) {
            auto& estate   = err_state.emplace();
            estate.code    = ErrorCodes::INTERNAL_ERROR;
            estate.message = "An internal error occured";
        }

        qDebug() << "Method Done" << ret_data.toDiagnosticNotation()
                 << (err_state ? "ERROR" : "OK");

        if (is_table) {
            auto this_tbl = context.get_table();

            if (err_state) {
                send_table_reply(
                    id, *this_tbl, true, nullptr, &err_state.value());
            } else {
                send_table_reply(id, *this_tbl, true, &ret_data, nullptr);
            }
        } else {
            if (must_reply) {
                if (err_state) {
                    send_method_error_reply(id, err_state.value());
                } else {
                    send_method_ok_reply(id, ret_data);
                }
            }
        }
    }


public:
    MessageHandler(ServerT* s, ClientT& c) : m_server(s), m_client(c) { }

    void handle(messages::ClientMessage const& message) {
        try {

            noo::visit([this](auto const& m) { this->handle(m); }, message);

        } catch (std::exception const& e) {
            qCritical() << "Internal exception while handling client message!"
                        << e.what();
        } catch (...) {
            qCritical() << "Internal exception while handling client message!";
        }
    }
};


void ServerT::on_client_message(QVector<messages::ClientMessage> const& list) {
    if (list.empty()) return;

    ClientT* c = qobject_cast<ClientT*>(sender());

    qDebug() << Q_FUNC_INFO << c;

    if (!c) return;

    MessageHandler handler(this, *c);

    for (auto const& cm : list) {
        handler.handle(cm);
    }
}

} // namespace noo
