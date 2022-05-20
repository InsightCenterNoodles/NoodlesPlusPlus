#include "clientmessagehandler.h"

#include "clientstate.h"
#include "noo_client_interface.h"
#include "noo_common.h"
#include "noo_id.h"

#include "src/common/variant_tools.h"

#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QString>

namespace nooc {

ClientWriter::ClientWriter(QWebSocket& s) : m_socket(s) { }
ClientWriter::~ClientWriter() {
    flush();
}

void ClientWriter::add(noo::messages::ClientMessage message) {
    m_message_list.push_back(message);
}

void ClientWriter::flush() {
    if (m_message_list.empty()) return;

    auto byte_array = serialize_client(m_message_list);

    m_socket.sendBinaryMessage(byte_array);

    m_message_list.clear();
}

// =============================================================================


template <class U>
auto make_v(InternalClientState& state, U const& arr) {
    using T = decltype(lookup(state, *(*arr.begin())));
    std::vector<T> ret;

    for (auto* item : arr) {
        ret.push_back(lookup(state, *item));
    }

    return ret;
};

struct MessageState {
    QWebSocket&          socket;
    InternalClientState& state;
};

static void process(noo::messages::MsgMethodCreate const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    MethodInit md(value, ms.state);

    MethodDelegate* delegate =
        ms.state.method_list().handle_new(id, std::move(md));

    QObject::connect(delegate,
                     &MethodDelegate::invoke,
                     &ms.state,
                     &InternalClientState::on_method_ask_invoke);
}
static void process(noo::messages::MsgMethodDelete const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    ms.state.method_list().handle_delete(id);
}

static void process(noo::messages::MsgSignalCreate const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    SignalInit init(value, ms.state);

    ms.state.signal_list().handle_new(id, std::move(init));
}
static void process(noo::messages::MsgSignalDelete const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    ms.state.signal_list().handle_delete(id);
}

static void process(noo::messages::MsgEntityCreate const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    EntityInit init(value, ms.state);

    ms.state.object_list().handle_new(id, std::move(init));
}
static void process(noo::messages::MsgEntityUpdate const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    EntityUpdateData data(value, ms.state);

    ms.state.object_list().handle_update(id, std::move(data));
}
static void process(noo::messages::MsgEntityDelete const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    ms.state.object_list().handle_delete(id);
}

static void process(noo::messages::MsgBufferCreate const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    BufferInit init(value, ms.state);

    auto* d = ms.state.buffer_list().handle_new(id, std::move(init));

    d->start_download(ms.state.network_manager());
}
static void process(noo::messages::MsgBufferDelete const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    ms.state.buffer_list().handle_delete(id);
}

static void process(noo::messages::MsgBufferViewCreate const& value,
                    MessageState&                             ms) {
    auto id = value.id;

    BufferViewInit init(value, ms.state);

    ms.state.buffer_view_list().handle_new(id, std::move(init));
}
static void process(noo::messages::MsgBufferViewDelete const& value,
                    MessageState&                             ms) {
    auto id = value.id;

    ms.state.buffer_view_list().handle_delete(id);
}

static void process(noo::messages::MsgMaterialCreate const& value,
                    MessageState&                           ms) {
    auto id = value.id;

    MaterialInit md(value, ms.state);

    ms.state.material_list().handle_new(id, std::move(md));
}
static void process(noo::messages::MsgMaterialUpdate const& value,
                    MessageState&                           ms) {
    auto id = value.id;

    MaterialUpdate md;

    ms.state.material_list().handle_update(id, std::move(md));
}
static void process(noo::messages::MsgMaterialDelete const& value,
                    MessageState&                           ms) {
    auto id = value.id;

    ms.state.material_list().handle_delete(id);
}

static void process(noo::messages::MsgTextureCreate const& value,
                    MessageState&                          ms) {
    auto id = value.id;

    TextureInit td(value, ms.state);

    ms.state.texture_list().handle_new(id, std::move(td));
}
static void process(noo::messages::MsgTextureDelete const& value,
                    MessageState&                          ms) {
    auto id = value.id;

    ms.state.texture_list().handle_delete(id);
}

static void process(noo::messages::MsgImageCreate const& value,
                    MessageState&                        ms) {
    auto id = value.id;

    ImageInit init(value, ms.state);

    ms.state.image_list().handle_new(id, std::move(init));
}
static void process(noo::messages::MsgImageDelete const& value,
                    MessageState&                        ms) {
    auto id = value.id;

    ms.state.image_list().handle_delete(id);
}

static void process(noo::messages::MsgSamplerCreate const& value,
                    MessageState&                          ms) {
    auto id = value.id;

    SamplerInit init(value, ms.state);

    ms.state.sampler_list().handle_new(id, std::move(init));
}
static void process(noo::messages::MsgSamplerDelete const& value,
                    MessageState&                          ms) {
    auto id = value.id;

    ms.state.sampler_list().handle_delete(id);
}

static void process(noo::messages::MsgLightCreate const& value,
                    MessageState&                        ms) {
    auto id = value.id;

    LightInit ld(value, ms.state);

    ms.state.light_list().handle_new(id, std::move(ld));
}
static void process(noo::messages::MsgLightUpdate const& value,
                    MessageState&                        ms) {
    auto id = value.id;

    LightUpdate ld(value, ms.state);

    ms.state.light_list().handle_update(id, std::move(ld));
}
static void process(noo::messages::MsgLightDelete const& value,
                    MessageState&                        ms) {
    auto id = value.id;

    ms.state.light_list().handle_delete(id);
}

static void process(noo::messages::MsgGeometryCreate const& value,
                    MessageState&                           ms) {
    auto id = value.id;

    MeshInit md(value, ms.state);

    ms.state.mesh_list().handle_new(id, std::move(md));
}
static void process(noo::messages::MsgGeometryDelete const& value,
                    MessageState&                           ms) {
    auto id = value.id;

    ms.state.mesh_list().handle_delete(id);
}

static void process(noo::messages::MsgTableCreate const& value,
                    MessageState&                        ms) {
    auto id = value.id;

    TableInit td(value, ms.state);

    ms.state.table_list().handle_new(id, std::move(td));
}
static void process(noo::messages::MsgTableUpdate const& value,
                    MessageState&                        ms) {
    auto id = value.id;

    TableUpdate td(value, ms.state);

    ms.state.table_list().handle_update(id, std::move(td));
}
static void process(noo::messages::MsgTableDelete const& value,
                    MessageState&                        ms) {
    auto id = value.id;

    ms.state.table_list().handle_delete(id);
}

static void process(noo::messages::MsgPlotCreate const& value,
                    MessageState&                       ms) {
    auto id = value.id;

    PlotInit pd(value, ms.state);

    ms.state.plot_list().handle_new(id, std::move(pd));
}
static void process(noo::messages::MsgPlotUpdate const& value,
                    MessageState&                       ms) {
    auto id = value.id;

    PlotUpdate pd(value, ms.state);

    ms.state.plot_list().handle_update(id, std::move(pd));
}
static void process(noo::messages::MsgPlotDelete const& value,
                    MessageState&                       ms) {
    auto id = value.id;

    ms.state.plot_list().handle_delete(id);
}

static void process(noo::messages::MsgDocumentUpdate const& value,
                    MessageState&                           ms) {

    DocumentData dd(value, ms.state);

    ms.state.document().update(dd);
}
static void process(noo::messages::MsgDocumentReset const& value,
                    MessageState&                          ms) {
    ms.state.clear();
}

static void process(noo::messages::MsgSignalInvoke const& value,
                    MessageState&                         ms) {
    auto id = value.id;

    qDebug() << Q_FUNC_INFO;
    auto sig = ms.state.lookup(id);

    if (!sig) {
        qWarning() << "Unknown signal being invoked!";
        return;
    }

    QCborArray av = value.signal_data;

    MethodContextPtr ctx;
    AttachedSignal*  attached = nullptr;

    auto inv_id = value.context;

    if (inv_id) {
        VMATCH(
            *inv_id,
            VCASE(std::monostate) {
                // invoke on document
                qDebug() << "Invoke on document";
                ctx = std::monostate();
                attached =
                    ms.state.document().attached_signals().find_by_delegate(
                        sig);
            },
            VCASE(auto eid) {
                // invoke on document
                qDebug() << "Invoke on thingy";
                auto obj = ms.state.lookup(eid);
                if (!obj) {
                    qWarning() << "Unknown object for signal!";
                    return;
                }
                ctx = obj;
                // listener notification
                attached = obj->attached_signals().find_by_delegate(sig);
            });
    } else {
        // invoke on document
        qDebug() << "Invoke on document";
        ctx      = std::monostate();
        attached = ms.state.document().attached_signals().find_by_delegate(sig);
    }

    // global notification
    emit sig->fired(ctx, av);

    // local notification
    if (attached) emit attached->fired(av);
}

static void process(noo::messages::MsgMethodReply const& value,
                    MessageState&                        ms) {

    QString ident = value.invoke_id;

    auto iter = ms.state.inflight_methods().find(ident);

    if (iter == ms.state.inflight_methods().end()) {
        qWarning() << "Reply for method we did not send!";
        return;
    }

    QPointer<PendingMethodReply> reply_ptr = iter.value();

    Q_ASSERT(reply_ptr);

    std::optional<MethodException> err;

    if (value.method_exception) {
        err.emplace(value.method_exception.value(), ms.state);
    }

    qDebug() << "Completing reply...";
    reply_ptr->complete(value.result.value_or(QCborValue()),
                        err ? &err.value() : nullptr);

    qDebug() << "Clean up...";
    reply_ptr->deleteLater();

    ms.state.inflight_methods().erase(iter);
}


void process_message(QWebSocket& s, InternalClientState& state, QByteArray m) {
    MessageState ms { .socket = s, .state = state };

    auto message_array = noo::messages::deserialize_server(m);

    for (auto const& m : message_array) {
        noo::visit([&ms](auto const& message) { process(message, ms); }, m);
    }
}

} // namespace nooc
