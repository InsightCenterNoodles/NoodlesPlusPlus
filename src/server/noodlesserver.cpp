#include "noodlesserver.h"

#include "noodlesstate.h"
#include "serialize.h"
#include "src/generated/noodles_bfbs.h"
#include "src/generated/noodles_generated.h"

#include <flatbuffers/idl.h>
#include <flatbuffers/minireflect.h>

#include <QDebug>
#include <QFile>
#include <QWebSocket>
#include <QWebSocketServer>

namespace noo {

class IncomingMessage {
    QByteArray m_data_ref;

    std::unique_ptr<flatbuffers::Parser> m_parser;

    noodles::ClientMessages const* m_messages = nullptr;

public:
    IncomingMessage(QByteArray bytes) {
        qDebug() << Q_FUNC_INFO << bytes.size();

        // need to hold onto where our data is coming from
        // as the reader uses refs to it.
        m_data_ref = bytes;

        {
            flatbuffers::Verifier v((uint8_t*)bytes.data(), bytes.size());

            if (!v.VerifyBuffer<noodles::ClientMessages>(nullptr)) {
                qCritical() << "Unable to verify message from client!";
                return;
            }

            qDebug() << "Message verified";
        }

        m_messages =
            flatbuffers::GetRoot<noodles::ClientMessages>(m_data_ref.data());
    }

    IncomingMessage(QString text) {
        qDebug() << Q_FUNC_INFO << text.size();

        m_parser = std::make_unique<flatbuffers::Parser>();

        bool ok = m_parser->Deserialize(noodles::noodles_bfbs,
                                        noodles::noodles_bfbs_len);

        if (!ok) {
            qCritical() << "Internal error: Cannot load schema.";
            return;
        }

        ok = m_parser->SetRootType("ServerMessages");

        if (!ok) {
            qCritical() << "Internal error: Cannot override root type.";
            return;
        }

        auto utf8_text = text.toUtf8();

        ok = m_parser->ParseJson(utf8_text.data());

        if (!ok) {
            qCritical() << "Unable to parse message from client!"
                        << m_parser->error_.c_str();
            return;
        }

        m_messages = flatbuffers::GetRoot<noodles::ClientMessages>(
            m_parser->builder_.GetBufferPointer());
    }

    ~IncomingMessage() noexcept { }

    noodles::ClientMessages const* get_root() { return m_messages; }
};

// =============================================================================

// This visitor is copied from the flatbuffer visitor, but modified to use
// QString natively
struct ToQStringVisitor : public flatbuffers::IterationVisitor {
    QString s;
    QString d;
    bool    q;
    QString in;
    size_t  indent_level;
    bool    vector_delimited;
    ToQStringVisitor(QString delimiter,
                     bool    quotes,
                     QString indent,
                     bool    vdelimited = true)
        : d(delimiter),
          q(quotes),
          in(indent),
          indent_level(0),
          vector_delimited(vdelimited) { }

    void append_indent() {
        for (size_t i = 0; i < indent_level; i++) {
            s += in;
        }
    }

    void StartSequence() {
        s += "{";
        s += d;
        indent_level++;
    }
    void EndSequence() {
        s += d;
        indent_level--;
        append_indent();
        s += "}";
    }
    void Field(size_t /*field_idx*/,
               size_t set_idx,
               flatbuffers::ElementaryType /*type*/,
               bool /*is_vector*/,
               const flatbuffers::TypeTable* /*type_table*/,
               const char*    name,
               const uint8_t* val) {
        if (!val) return;
        if (set_idx) {
            s += ",";
            s += d;
        }
        append_indent();
        if (name) {
            if (q) s += "\"";
            s += name;
            if (q) s += "\"";
            s += ": ";
        }
    }
    template <typename T>
    void Named(T x, const char* name) {
        if (name) {
            if (q) s += "\"";
            s += name;
            if (q) s += "\"";
        } else {
            s += QString::number(x); //  flatbuffers::NumToString(x);
        }
    }
    void UType(uint8_t x, const char* name) { Named(x, name); }
    void Bool(bool x) { s += x ? "true" : "false"; }
    void Char(int8_t x, const char* name) { Named(x, name); }
    void UChar(uint8_t x, const char* name) { Named(x, name); }
    void Short(int16_t x, const char* name) { Named(x, name); }
    void UShort(uint16_t x, const char* name) { Named(x, name); }
    void Int(int32_t x, const char* name) { Named(x, name); }
    void UInt(uint32_t x, const char* name) { Named(x, name); }
    void Long(int64_t x) { s += QString::number(x); }
    void ULong(uint64_t x) { s += QString::number(x); }
    void Float(float x) { s += QString::number(x); }
    void Double(double x) { s += QString::number(x); }
    void String(const struct flatbuffers::String* str) {
        s += QString::fromUtf8(str->c_str(), str->size());
    }
    void Unknown(const uint8_t*) { s += "(?)"; }
    void StartVector() {
        s += "[";
        if (vector_delimited) {
            s += d;
            indent_level++;
            append_indent();
        } else {
            s += " ";
        }
    }
    void EndVector() {
        if (vector_delimited) {
            s += d;
            indent_level--;
            append_indent();
        } else {
            s += " ";
        }
        s += "]";
    }
    void Element(size_t i,
                 flatbuffers::ElementaryType /*type*/,
                 const flatbuffers::TypeTable* /*type_table*/,
                 const uint8_t* /*val*/) {
        if (i) {
            s += ",";
            if (vector_delimited) {
                s += d;
                append_indent();
            } else {
                s += " ";
            }
        }
    }
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
    m_use_binary = false;

    auto ptr = std::make_shared<IncomingMessage>(text);

    if (!ptr->get_root()) {
        qCritical() << "Bad message from ClientT" << this;
        kill();
        return;
    }

    emit message_recvd(ptr);
}

void ClientT::on_binary(QByteArray array) {
    m_use_binary = true;

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

    if (m_use_binary) {
        m_socket->sendBinaryMessage(data);
    } else {

        ToQStringVisitor visitor(" ", true, "");

        flatbuffers::IterateFlatBuffer((uint8_t*)data.data(),
                                       noodles::ServerMessagesTypeTable(),
                                       &visitor);

        m_socket->sendTextMessage(visitor.s);
    }
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

    // qDebug() << Q_FUNC_INFO << client;
    qInfo() << "New client:" << socket->origin();

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

struct ErrorState {
    int         code;
    std::string message;
    AnyVar      additional;
};

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
                                 ErrorState const&  state) {
        if (id.empty()) return;

        auto w = m_server->get_single_client_writer(m_client);

        auto e = noodles::CreateMethodExceptionDirect(
            *w,
            state.code,
            state.message.data(),
            write_to(state.additional, *w));

        auto x = noodles::CreateMethodReplyDirect(*w, id.c_str(), 0, e);

        w->complete_message(x);
    }

    void send_table_reply(std::string const& id,
                          TableT&            table,
                          bool               exclusive,
                          AnyVar const*      var,
                          ErrorState const*  err) {

        if (id.empty()) return;

        auto w = exclusive ? m_server->get_single_client_writer(m_client)
                           : m_server->get_table_subscribers_writer(table);

        flatbuffers::Offset<noodles::MethodReply> x;

        if (var) {
            x = noodles::CreateMethodReplyDirect(
                *w, id.c_str(), write_to(*var, *w));
        } else if (err) {

            auto e = noodles::CreateMethodExceptionDirect(
                *w,
                err->code,
                err->message.data(),
                write_to(err->additional, *w));

            x = noodles::CreateMethodReplyDirect(*w, id.c_str(), 0, e);
        } else {
            // uh oh
            qFatal("Not supposed to get here");
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
                send_method_error_reply(
                    id,
                    { ErrorCodes::METHOD_NOT_FOUND, "Method not found!", {} });
            }

            qWarning() << "unable to find method target!";
            return;
        }

        MethodID method_id = convert_id(message->method_id());

        if (!method_id.valid()) {

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

        qDebug() << "METHOD ID" << method_id.id_slot << method_id.id_gen;

        auto* method = target_list->find(method_id);

        if (!method) {
            if (must_reply) {
                send_method_error_reply(id,
                                        { ErrorCodes::METHOD_NOT_FOUND,
                                          "Unable to find method on context",
                                          {} });
            }
            qWarning() << "unable to find method" << method_id.id_slot;
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

        AnyVarListRef vars;

        if (message->method_args()) {
            vars = AnyVarListRef(message->method_args());
        }

        {
            qDebug() << "Method arguments:"
                     << QString::fromStdString(vars.dump_string());
        }

        AnyVar                    ret_data;
        std::optional<ErrorState> err_state;

        try {
            ret_data = function(context, vars);
        } catch (MethodException const& e) {
            auto& estate      = err_state.emplace();
            estate.code       = e.code();
            estate.message    = e.reason();
            estate.additional = e.data();
        } catch (...) {
            auto& estate   = err_state.emplace();
            estate.code    = ErrorCodes::INTERNAL_ERROR;
            estate.message = "An internal error occured";
        }

        qDebug() << "Method Done" << ret_data.dump_string().c_str()
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


        } catch (std::exception const& e) {
            qCritical() << "Internal exception while handling client message!"
                        << e.what();
        } catch (...) {
            qCritical() << "Internal exception while handling client message!";
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
