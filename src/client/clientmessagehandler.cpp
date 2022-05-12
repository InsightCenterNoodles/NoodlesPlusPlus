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
#include <QString>
#include <qnetworkrequest.h>

namespace nooc {

ClientWriter::ClientWriter(QWebSocket& s) : m_socket(s) { }
ClientWriter::~ClientWriter() {
    flush();
}

void ClientWriter::add(QCborValue message, unsigned message_id) {
    m_written = false;

    QCborMap mmap;
    mmap[message_id] = message;

    m_message_list << mmap;
}

void ClientWriter::flush() {
    if (m_written) return;

    auto byte_array = m_message_list.toCborValue().toCbor();

    m_socket.sendBinaryMessage(byte_array);

    m_written = true;

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

static void process_MsgMethodCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::MethodID>(value);

    MethodInit md(value);

    MethodDelegate* delegate =
        ms.state.method_list().handle_new(id, std::move(md));

    QObject::connect(delegate,
                     &MethodDelegate::invoke,
                     &ms.state,
                     &InternalClientState::on_method_ask_invoke);
}
static void process_MsgMethodDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::MethodID>(value);

    ms.state.method_list().handle_delete(id);
}

static void process_MsgSignalCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::SignalID>(value);

    SignalInit init(value);

    ms.state.signal_list().handle_new(id, std::move(init));
}
static void process_MsgSignalDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::SignalID>(value);

    ms.state.signal_list().handle_delete(id);
}

static void process_MsgEntityCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::EntityID>(value);

    EntityInit init(value);
}
static void process_MsgEntityUpdate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::EntityID>(value);

    EntityUpdateData data(value, ms.state);

    ms.state.object_list().handle_new(id, std::move(data));
}
static void process_MsgEntityDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::EntityID>(value);

    ms.state.object_list().handle_delete(id);
}

static void process_MsgBufferCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::BufferID>(value);

    BufferInit init(value);

    auto* d = ms.state.buffer_list().handle_new(id, std::move(init));

    d->start_download(ms.state.network_manager());
}
static void process_MsgBufferDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::BufferID>(value);

    ms.state.buffer_list().handle_delete(id);
}

static void process_MsgBufferViewCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::BufferViewID>(value);

    BufferViewInit init(value, ms.state);

    ms.state.buffer_view_list().handle_new(id, std::move(init));
}
static void process_MsgBufferViewDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::BufferViewID>(value);

    ms.state.buffer_view_list().handle_delete(id);
}

static void process_MsgMaterialCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::MaterialID>(value);

    MaterialInit md(value, ms.state);

    ms.state.material_list().handle_new(id, std::move(md));
}
static void process_MsgMaterialUpdate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::MaterialID>(value);

    MaterialUpdate md;

    ms.state.material_list().handle_update(id, std::move(md));
}
static void process_MsgMaterialDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::MaterialID>(value);

    ms.state.material_list().handle_delete(id);
}

static void process_MsgTextureCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::TextureID>(value);

    TextureInit td(value, ms.state);

    ms.state.texture_list().handle_new(id, std::move(td));
}
static void process_MsgTextureDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::TextureID>(value);

    ms.state.texture_list().handle_delete(id);
}

static void process_MsgImageCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::ImageID>(value);

    ImageInit init(value, ms.state);

    ms.state.image_list().handle_new(id, std::move(init));
}
static void process_MsgImageDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::ImageID>(value);

    ms.state.image_list().handle_delete(id);
}

static void process_MsgSamplerCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::SamplerID>(value);

    SamplerInit init(value, ms.state);

    ms.state.sampler_list().handle_new(id, std::move(init));
}
static void process_MsgSamplerDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::SamplerID>(value);

    ms.state.sampler_list().handle_delete(id);
}

static void process_MsgLightCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::LightID>(value);

    LightInit ld(value);

    ms.state.light_list().handle_new(id, std::move(ld));
}
static void process_MsgLightUpdate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::LightID>(value);

    LightUpdate ld(value);

    ms.state.light_list().handle_update(id, std::move(ld));
}
static void process_MsgLightDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::LightID>(value);

    ms.state.light_list().handle_delete(id);
}

static void process_MsgGeometryCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::GeometryID>(value);

    MeshInit md(value, ms.state);

    ms.state.mesh_list().handle_new(id, std::move(md));
}
static void process_MsgGeometryDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::GeometryID>(value);

    ms.state.mesh_list().handle_delete(id);
}

static void process_MsgTableCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::TableID>(value);

    TableInit td(value);

    ms.state.table_list().handle_new(id, std::move(td));
}
static void process_MsgTableUpdate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::TableID>(value);

    TableUpdate td(value, ms.state);

    ms.state.table_list().handle_update(id, std::move(td));
}
static void process_MsgTableDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::TableID>(value);

    ms.state.table_list().handle_delete(id);
}

static void process_MsgPlotCreate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::PlotID>(value);

    PlotInit pd(value);

    ms.state.plot_list().handle_new(id, std::move(pd));
}
static void process_MsgPlotUpdate(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::PlotID>(value);

    PlotUpdate pd(value, ms.state);

    ms.state.plot_list().handle_update(id, std::move(pd));
}
static void process_MsgPlotDelete(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::PlotID>(value);

    ms.state.plot_list().handle_delete(id);
}

static void process_MsgDocumentUpdate(MessageState& ms, QCborMap value) {

    DocumentData dd(value, ms.state);

    ms.state.document().update(dd);
}
static void process_MsgDocumentReset(MessageState& ms, QCborMap) {
    ms.state.clear();
}

static void process_MsgSignalInvoke(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::SignalID>(value);

    qDebug() << Q_FUNC_INFO;
    auto sig = ms.state.lookup(id);

    if (!sig) {
        qWarning() << "Unknown signal being invoked!";
        return;
    }

    auto d = noo::CborDecoder(value);

    QCborArray av;

    d.conditional("signal_data", av);

    MethodContextPtr ctx;
    AttachedSignal*  attached = nullptr;

    auto inv_id = noo::InvokeID(value[QStringLiteral("context")].toMap());

    VMATCH(
        inv_id,
        VCASE(std::monostate) {
            // invoke on document
            qDebug() << "Invoke on document";
            ctx = std::monostate();
            attached =
                ms.state.document().attached_signals().find_by_delegate(sig);
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

    // global notification
    emit sig->fired(ctx, av);

    // local notification
    if (attached) emit attached->fired(av);
}

static void process_MsgMethodReply(MessageState& ms, QCborMap value) {
    auto d = noo::CborDecoder(value);

    QString ident;
    d("invoke_id", ident);

    auto iter = ms.state.inflight_methods().find(ident);

    if (iter == ms.state.inflight_methods().end()) {
        qWarning() << "Reply for method we did not send!";
        return;
    }

    QPointer<PendingMethodReply> reply_ptr = iter.value();

    Q_ASSERT(reply_ptr);

    QCborValue result;

    std::optional<MethodException> err;

    d.conditional("result", result);

    d("method_exception", err);

    qDebug() << "Completing reply...";
    reply_ptr->complete(std::move(result), err ? &err.value() : nullptr);

    qDebug() << "Clean up...";
    reply_ptr->deleteLater();

    ms.state.inflight_methods().erase(iter);
}


static void
process_server_message(MessageState& ms, int mid, QCborValue const& value) {
    qDebug() << Q_FUNC_INFO << value.toDiagnosticNotation();


    switch (mid) {
    case 0: process_MsgMethodCreate(ms, value.toMap()); break;
    case 1: process_MsgMethodDelete(ms, value.toMap()); break;
    case 2: process_MsgSignalCreate(ms, value.toMap()); break;
    case 3: process_MsgSignalDelete(ms, value.toMap()); break;
    case 4: process_MsgEntityCreate(ms, value.toMap()); break;
    case 5: process_MsgEntityUpdate(ms, value.toMap()); break;
    case 6: process_MsgEntityDelete(ms, value.toMap()); break;
    case 7: process_MsgPlotCreate(ms, value.toMap()); break;
    case 8: process_MsgPlotUpdate(ms, value.toMap()); break;
    case 9: process_MsgPlotDelete(ms, value.toMap()); break;
    case 10: process_MsgBufferCreate(ms, value.toMap()); break;
    case 11: process_MsgBufferDelete(ms, value.toMap()); break;
    case 12: process_MsgBufferViewCreate(ms, value.toMap()); break;
    case 13: process_MsgBufferViewDelete(ms, value.toMap()); break;
    case 14: process_MsgMaterialCreate(ms, value.toMap()); break;
    case 15: process_MsgMaterialUpdate(ms, value.toMap()); break;
    case 16: process_MsgMaterialDelete(ms, value.toMap()); break;
    case 17: process_MsgImageCreate(ms, value.toMap()); break;
    case 18: process_MsgImageDelete(ms, value.toMap()); break;
    case 19: process_MsgTextureCreate(ms, value.toMap()); break;
    case 20: process_MsgTextureDelete(ms, value.toMap()); break;
    case 21: process_MsgSamplerCreate(ms, value.toMap()); break;
    case 22: process_MsgSamplerDelete(ms, value.toMap()); break;
    case 23: process_MsgLightCreate(ms, value.toMap()); break;
    case 24: process_MsgLightUpdate(ms, value.toMap()); break;
    case 25: process_MsgLightDelete(ms, value.toMap()); break;
    case 26: process_MsgGeometryCreate(ms, value.toMap()); break;
    case 27: process_MsgGeometryDelete(ms, value.toMap()); break;
    case 28: process_MsgTableCreate(ms, value.toMap()); break;
    case 29: process_MsgTableUpdate(ms, value.toMap()); break;
    case 30: process_MsgTableDelete(ms, value.toMap()); break;
    case 31: process_MsgDocumentUpdate(ms, value.toMap()); break;
    case 32: process_MsgDocumentReset(ms, value.toMap()); break;
    case 33: process_MsgSignalInvoke(ms, value.toMap()); break;
    case 34: process_MsgMethodReply(ms, value.toMap()); break;
    default: qWarning() << "Unknown message: " << mid;
    }
}


void process_message(QWebSocket& s, InternalClientState& state, QByteArray m) {
    MessageState ms { .socket = s, .state = state };

    QCborParserError error;
    auto             value = QCborValue::fromCbor(m, &error);

    if (error.error != QCborError::NoError) {
        qWarning() << "Bad message from server:" << error.errorString();
        return;
    }

    auto array = value.toArray();

    for (auto const& v : array) {
        auto pack = v.toMap();

        for (auto [k, v] : pack) {
            process_server_message(ms, k.toInteger(-1), v);
        }
    }
}

} // namespace nooc
