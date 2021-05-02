#define FLATBUFFERS_DEBUG_VERIFICATION_FAILURE

#include "noodlesserver.h"

#include "noodlesstate.h"
#include "serialize.h"
#include "src/generated/noodles_client_generated.h"

#include <QDebug>
#include <QFile>
#include <QWebSocket>
#include <QWebSocketServer>

namespace noo {

class IncomingMessage {
    QByteArray m_data_ref;

    noodles::ClientMessages const* m_messages = nullptr;

public:
    IncomingMessage(QByteArray bytes) {
        qDebug() << Q_FUNC_INFO << bytes.size();

        // need to hold onto where our data is coming from
        // as the reader uses refs to it.
        m_data_ref = bytes;

        {
            flatbuffers::Verifier v((uint8_t*)bytes.data(), bytes.size());

            if (!noodles::VerifyClientMessagesBuffer(v)) {
                qCritical() << "Bad message!";
                return;
            }
            qDebug() << "Message verified";
        }

        m_messages = noodles::GetClientMessages(m_data_ref.data());
    }

    ~IncomingMessage() noexcept { }

    noodles::ClientMessages const* get_root() { return m_messages; }
};

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
    qWarning() << "Text not supported!" << text;
}

void ClientT::on_binary(QByteArray array) {

    auto ptr = std::make_shared<IncomingMessage>(array);

    if (!ptr->get_root()) {
        qCritical() << "Bad message from ClientT" << this;
        kill();
        return;
    }


    emit message_recvd(ptr);
}

void ClientT::set_name(std::string const& s) {
    m_name = QString::fromStdString(s);
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
}

// =============================================================================


ServerT::ServerT(quint16 port, QObject* parent) : QObject(parent) {

    m_state = new NoodlesState(this);

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

NoodlesState* ServerT::state() {
    return m_state;
}

std::unique_ptr<Writer> ServerT::get_broadcast_writer() {
    auto p = std::make_unique<Writer>();

    connect(p.get(), &Writer::data_ready, this, &ServerT::broadcast);

    return p;
}

std::unique_ptr<Writer> ServerT::get_single_client_writer(ClientT& c) {
    auto p = std::make_unique<Writer>();

    connect(p.get(), &Writer::data_ready, &c, &ClientT::send);

    return p;
}

std::unique_ptr<Writer> ServerT::get_table_subscribers_writer(TableT& t) {
    auto p = std::make_unique<Writer>();

    connect(p.get(), &Writer::data_ready, &t, &TableT::send_data);

    return p;
}

void ServerT::broadcast(QByteArray ptr) {
    for (ClientT* c : m_connected_clients) {
        c->send(ptr);
    }
}


void ServerT::on_new_connection() {
    QWebSocket* socket = m_socket_server->nextPendingConnection();

    ClientT* client = new ClientT(socket, this);

    qDebug() << Q_FUNC_INFO << client;

    connect(client, &ClientT::finished, this, &ServerT::on_client_done);
    connect(client, &ClientT::message_recvd, this, &ServerT::on_client_message);

    m_connected_clients.insert(client);
}

void ServerT::on_client_done() {
    ClientT* c = qobject_cast<ClientT*>(sender());

    if (!c) return;

    qDebug() << Q_FUNC_INFO << c;

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

    void handle_introduction(noodles::IntroductionMessage const* m) {
        if (!m) return;
        qDebug() << Q_FUNC_INFO;

        m_client.set_name(m->client_name()->str());

        // we must now reply with all of the stuff

        // we will dump in an order that should be not so bad on the client.
        // that means no undef references

        // first methods

        auto& d = get_document();

        dump_list(d.method_list());
        dump_list(d.signal_list());
        dump_list(d.light_list());
        dump_list(d.buffer_list());
        dump_list(d.tex_list());
        dump_list(d.mat_list());
        dump_list(d.mesh_list());
        dump_list(d.table_list());
        dump_list(d.obj_list());

        auto w = m_server->get_single_client_writer(m_client);
        get_document().write_refresh(*w);
    }


    void send_method_ok_reply(std::string const& id, AnyVar const& var) {
        if (id.empty()) return;

        auto w = m_server->get_single_client_writer(m_client);

        auto x =
            noodles::CreateMethodReplyDirect(*w, id.c_str(), write_to(var, *w));

        w->complete_message(x);
    }

    void send_method_error_reply(std::string const& id,
                                 std::string const& view) {
        if (id.empty()) return;

        auto w = m_server->get_single_client_writer(m_client);

        auto x =
            noodles::CreateMethodReplyDirect(*w, id.c_str(), 0, view.c_str());

        w->complete_message(x);
    }

    void send_table_reply(std::string const& id,
                          TableT&            table,
                          bool               exclusive,
                          AnyVar const*      var,
                          std::string const* err) {

        if (id.empty()) return;

        auto w = exclusive ? m_server->get_single_client_writer(m_client)
                           : m_server->get_table_subscribers_writer(table);

        flatbuffers::Offset<noodles::MethodReply> x;

        if (var) {
            x = noodles::CreateMethodReplyDirect(
                *w, id.c_str(), write_to(*var, *w));
        } else {
            x = noodles::CreateMethodReplyDirect(
                *w, id.c_str(), 0, err->c_str());
        }

        w->complete_message(x);
    }

    void handle_invoke(noodles::MethodInvokeMessage const* message) {
        if (!message) return;
        qDebug() << Q_FUNC_INFO;

        MethodContext             context;
        AttachedMethodList const* target_list = nullptr;
        bool                      is_table    = false;

        if (message->on_object()) {
            qDebug() << "INVOKE on obj";

            ObjectID source = convert_id(*message->on_object());

            auto const& obj_ptr = get_document().obj_list().get_at(source);

            if (obj_ptr) {
                context     = obj_ptr;
                target_list = &(obj_ptr->att_method_list());
            }

        } else if (message->on_table()) {
            qDebug() << "INVOKE on table";

            TableID source = convert_id(*message->on_table());

            auto const& tbl_ptr = get_document().table_list().get_at(source);

            if (tbl_ptr) {
                context     = tbl_ptr;
                target_list = &(tbl_ptr->att_method_list());
                is_table    = true;
            }

        } else {
            qDebug() << "INVOKE on doc";
            target_list = &(get_document().att_method_list());
        }

        context.client = &m_client;

        std::string id = message->invoke_ident()->c_str();

        bool must_reply = id.size();

        if (!target_list) {
            if (must_reply) {
                MethodException exp(MethodException::CLIENT,
                                    "Unable to find method.");
                send_method_error_reply(id, exp.reason());
            }

            qWarning() << "unable to find method target!";
            return;
        }

        MethodID method_id = convert_id(message->methodId());

        if (!method_id.valid()) {

            if (must_reply) {
                MethodException exp(MethodException::CLIENT,
                                    "Unable to find method.");
                send_method_error_reply(id, exp.reason());
            }

            qWarning() << "Invalid method";
            return;
        }

        qDebug() << "METHOD ID" << method_id.id_slot << method_id.id_gen;

        auto* method = target_list->find(method_id);

        if (!method) {
            if (must_reply) {
                MethodException exp(MethodException::CLIENT,
                                    "Unable to find method on context");
                send_method_error_reply(id, exp.reason());
            }
            qWarning() << "unable to find method" << method_id.id_slot;
            return;
        }

        auto const& function = method->function();

        if (!function) {
            if (must_reply) {
                MethodException exp(
                    MethodException::APPLICATION,
                    "Unable to execute method: method has no code.");
                send_method_error_reply(id, exp.reason());
            }
            qWarning() << "method has no code";
            return;
        }

        std::vector<AnyVar> vars;

        if (message->method_args()) {
            read_to_array(message->method_args(), vars);
        }

        AnyVar      ret_data;
        std::string err_str;

        try {
            ret_data = function(context, vars);
        } catch (MethodException const& e) {
            err_str = e.reason();
        } catch (...) {
            MethodException exp(MethodException::APPLICATION,
                                "An error occurred; check server!");
            err_str = exp.reason();
        }

        qDebug() << "Method Done" << ret_data.dump_string().c_str()
                 << err_str.c_str();

        if (is_table) {
            auto this_tbl = context.get_table();

            if (err_str.size()) {
                send_table_reply(id, *this_tbl, true, nullptr, &err_str);
            } else {
                send_table_reply(id, *this_tbl, true, &ret_data, nullptr);
            }
        } else {
            if (must_reply) {
                if (err_str.size()) {
                    send_method_error_reply(id, err_str);
                } else {
                    send_method_ok_reply(id, ret_data);
                }
            }
        }
    }

    void handle_refresh(::noodles::AssetRefreshMessage const* message) {
        if (!message) return;
        qDebug() << Q_FUNC_INFO;

        auto buffer_list = message->for_buffers();

        for (auto buffer_ref : *buffer_list) {

            BufferID bid = convert_id(buffer_ref);

            if (!bid.valid()) return;

            auto const& buffer_ptr = get_document().buffer_list().get_at(bid);

            if (!buffer_ptr) continue;

            auto w = m_server->get_single_client_writer(m_client);

            buffer_ptr->write_refresh_to(*w);
        }
    }


public:
    MessageHandler(ServerT* s, ClientT& c) : m_server(s), m_client(c) { }

    void handle(IncomingMessage& message) {
        try {
            auto message_list = message.get_root();

            if (!message_list) return;
            if (!message_list->messages()) return;

            for (auto this_msg_ptr : *message_list->messages()) {
                switch (this_msg_ptr->content_type()) {
                case noodles::ClientMessageType::NONE: break;
                case noodles::ClientMessageType::IntroductionMessage:
                    handle_introduction(
                        this_msg_ptr->content_as_IntroductionMessage());
                    break;
                case noodles::ClientMessageType::MethodInvokeMessage:
                    handle_invoke(
                        this_msg_ptr->content_as_MethodInvokeMessage());
                    break;
                case noodles::ClientMessageType::AssetRefreshMessage:
                    handle_refresh(
                        this_msg_ptr->content_as_AssetRefreshMessage());
                    break;
                }
            }


        } catch (...) {
            qCritical() << "Exception while handling client message!";
        }
    }
};


void ServerT::on_client_message(std::shared_ptr<IncomingMessage> ptr) {

    assert(ptr);

    ClientT* c = qobject_cast<ClientT*>(sender());
    qDebug() << Q_FUNC_INFO << c;

    if (!c) return;

    MessageHandler handler(this, *c);

    handler.handle(*ptr);
}

} // namespace noo
