#include "clientstate.h"

#include "clientmessagehandler.h"
#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"

#include <QUuid>

namespace nooc {

ClientState::ClientState(QWebSocket& s, ClientDelegates& makers)
    : m_socket(s),
      m_document(makers.doc_maker()),
      m_method_list(makers.method_maker),
      m_signal_list(makers.sig_maker),
      m_buffer_list(makers.buffer_maker),
      m_table_list(makers.table_maker),
      m_texture_list(makers.tex_maker),
      m_light_list(makers.light_maker),
      m_material_list(makers.mat_maker),
      m_mesh_list(makers.mesh_maker),
      m_object_list(makers.object_maker) {
    connect(&m_socket,
            &QWebSocket::textMessageReceived,
            this,
            &ClientState::on_new_text_message);

    connect(&m_socket,
            &QWebSocket::binaryMessageReceived,
            this,
            &ClientState::on_new_binary_message);

    connect(&m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            [&](QAbstractSocket::SocketError) {
                qCritical()
                    << "Error from websocket!" << m_socket.errorString();
            });

    // send introduction message

    ClientWriter writer(m_socket);

    QString cname = makers.client_name.isEmpty() ? QString("Noodles C++ Client")
                                                 : makers.client_name;

    auto cname_std = cname.toStdString();

    auto x =
        noodles::CreateIntroductionMessageDirect(writer, cname_std.c_str());

    writer.complete_message(x);
    qDebug() << Q_FUNC_INFO;
}

ClientState::~ClientState() {
    qDebug() << Q_FUNC_INFO;
}

void ClientState::on_new_binary_message(QByteArray m) {
    MessageHandler handler(m_socket, *this, m);

    handler.process();
}
void ClientState::on_new_text_message(QString t) {
    // we dont expect there to be text...
    qWarning() << "Unexpected text from server" << t;
}

void ClientState::on_method_ask_invoke(noo::MethodID          method_id,
                                       MethodContext          context,
                                       noo::AnyVarList const& args,
                                       PendingMethodReply*    reply) {
    qDebug() << "Invoking" << method_id.to_qstring();
    Q_ASSERT(method_id.valid());
    Q_ASSERT(reply->parent() != this);
    reply->setParent(this);

    // generate an invoke id
    m_last_invoke_id++;
    auto id = std::to_string(m_last_invoke_id);

    auto [iter, did_insert] = m_in_flight_methods.try_emplace(id, reply);

    Q_ASSERT(did_insert);

    ClientWriter writer(m_socket);

    auto mid          = convert_id(method_id, writer);
    auto ident_handle = writer->CreateString(id);
    auto arg_handle   = noo::write_to(args, writer);

    flatbuffers::Offset<::noodles::ObjectID> const null_oid;
    flatbuffers::Offset<::noodles::TableID> const  null_tid;

    auto x = VMATCH(
        context,
        VCASE(std::monostate) {
            return noodles::CreateMethodInvokeMessage(
                writer, mid, null_oid, null_tid, ident_handle, arg_handle);
        },
        VCASE(ObjectDelegate * ptr) {
            auto oid = convert_id(ptr->id(), writer);

            return noodles::CreateMethodInvokeMessage(
                writer, mid, oid, null_tid, ident_handle, arg_handle);
        },
        VCASE(TableDelegate * ptr) {
            auto tid = convert_id(ptr->id(), writer);
            return noodles::CreateMethodInvokeMessage(
                writer, mid, null_oid, tid, ident_handle, arg_handle);
        });


    writer.complete_message(x);
}

} // namespace nooc
