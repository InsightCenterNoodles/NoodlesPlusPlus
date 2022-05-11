#include "clientstate.h"

#include "clientmessagehandler.h"
#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"

#include <QUuid>

namespace nooc {

InternalClientState::InternalClientState(QWebSocket& s, ClientDelegates& makers)
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
      m_object_list(makers.object_maker),
      m_plot_list(makers.plot_maker) {
    connect(&m_socket,
            &QWebSocket::textMessageReceived,
            this,
            &InternalClientState::on_new_text_message);

    connect(&m_socket,
            &QWebSocket::binaryMessageReceived,
            this,
            &InternalClientState::on_new_binary_message);

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

InternalClientState::~InternalClientState() {
    qDebug() << Q_FUNC_INFO;
}

void InternalClientState::on_new_binary_message(QByteArray m) {
    MessageHandler handler(m_socket, *this, m);

    handler.process();
}
void InternalClientState::on_new_text_message(QString t) {
    // we dont expect there to be text...
    qWarning() << "Unexpected text from server" << t;
}

void InternalClientState::on_method_ask_invoke(noo::MethodID          method_id,
                                       MethodContext          context,
                                       QCborValueList const& args,
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

    flatbuffers::Offset<::noodles::EntityID> const null_oid;
    flatbuffers::Offset<::noodles::TableID> const  null_tid;
    flatbuffers::Offset<::noodles::PlotID> const   null_pid;

    auto x = VMATCH(
        context,
        VCASE(std::monostate) {
            return noodles::CreateMethodInvokeMessage(writer,
                                                      mid,
                                                      null_oid,
                                                      null_tid,
                                                      null_pid,
                                                      ident_handle,
                                                      arg_handle);
        },
        VCASE(EntityDelegate * ptr) {
            auto oid = convert_id(ptr->id(), writer);

            return noodles::CreateMethodInvokeMessage(
                writer, mid, oid, null_tid, null_pid, ident_handle, arg_handle);
        },
        VCASE(TableDelegate * ptr) {
            auto tid = convert_id(ptr->id(), writer);
            return noodles::CreateMethodInvokeMessage(
                writer, mid, null_oid, tid, null_pid, ident_handle, arg_handle);
        },
        VCASE(PlotDelegate * ptr) {
            auto tid = convert_id(ptr->id(), writer);
            return noodles::CreateMethodInvokeMessage(
                writer, mid, null_oid, null_tid, tid, ident_handle, arg_handle);
        });


    writer.complete_message(x);
}

} // namespace nooc
