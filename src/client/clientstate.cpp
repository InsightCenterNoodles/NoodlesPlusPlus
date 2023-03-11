#include "clientstate.h"

#include "clientmessagehandler.h"
#include "src/common/variant_tools.h"

#include <QUuid>

namespace nooc {

InternalClientState::InternalClientState(QWebSocket& s, ClientDelegates& makers)
    : m_socket(s),
      m_document(makers.doc_maker()),
      m_method_list(makers.method_maker),
      m_signal_list(makers.sig_maker),
      m_buffer_list(makers.buffer_maker),
      m_buffer_view_list(makers.buffer_view_maker),
      m_table_list(makers.table_maker),
      m_texture_list(makers.tex_maker),
      m_light_list(makers.light_maker),
      m_material_list(makers.mat_maker),
      m_mesh_list(makers.mesh_maker),
      m_object_list(makers.object_maker),
      m_plot_list(makers.plot_maker),
      m_sampler_list(makers.sampler_maker),
      m_image_list(makers.image_maker) {
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
            this,
            [&](QAbstractSocket::SocketError) {
                qCritical()
                    << "Error from websocket!" << m_socket.errorString();
            });

    m_network_manager = new QNetworkAccessManager(this);

    // send introduction message


    QString cname = makers.client_name.isEmpty() ? QString("Noodles C++ Client")
                                                 : makers.client_name;

    noo::messages::MsgIntroduction intro;
    intro.client_name = cname;

    ClientWriter writer(m_socket);

    writer.add(intro);

    qDebug() << Q_FUNC_INFO;
}

InternalClientState::~InternalClientState() {
    clear();
    qDebug() << Q_FUNC_INFO;
}

void InternalClientState::clear() {
    document().clear();

    method_list().clear();
    signal_list().clear();
    image_list().clear();
    mesh_list().clear();
    material_list().clear();
    sampler_list().clear();
    texture_list().clear();
    buffer_view_list().clear();
    buffer_list().clear();
    table_list().clear();
    light_list().clear();
    object_list().clear();
}

void InternalClientState::on_new_binary_message(QByteArray m) {
    process_message(m_socket, *this, m);
}
void InternalClientState::on_new_text_message(QString t) {
    // Assuming it is JSON
    qWarning() << "Unexpected text from server" << t;
}

void InternalClientState::on_method_ask_invoke(noo::MethodID       method_id,
                                               MethodContextPtr    context,
                                               QCborArray const&   args,
                                               PendingMethodReply* reply) {
    qDebug() << "Invoking" << method_id.to_qstring();
    Q_ASSERT(method_id.valid());
    Q_ASSERT(reply->parent() != this);
    reply->setParent(this);

    noo::messages::MsgInvokeMethod invoke;

    // generate an invoke id
    m_last_invoke_id++;
    auto id = QString("%1").arg(m_last_invoke_id);

    Q_ASSERT(!m_in_flight_methods.contains(id));

    m_in_flight_methods[id] = reply;

    invoke.invoke_id = id;
    invoke.args      = args;
    invoke.method    = method_id;

    VMATCH(
        context,
        VCASE(std::monostate) {
            // do nothing
        },
        VCASE(auto const& ptr) { invoke.context = ptr->id(); }, );

    ClientWriter writer(m_socket);

    writer.add(invoke);
}

} // namespace nooc
