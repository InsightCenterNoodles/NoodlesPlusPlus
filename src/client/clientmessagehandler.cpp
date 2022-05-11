#include "clientmessagehandler.h"

#include "clientstate.h"
#include "noo_client_interface.h"
#include "noo_id.h"

#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QNetworkAccessManager>
#include <QString>
#include <qnetworkrequest.h>

namespace nooc {

void ClientWriter::finished_writing_and_export() {

    auto byte_array = m_message_list.toCborValue().toCbor();

    m_socket.sendBinaryMessage(byte_array);

    m_written = true;
}

ClientWriter::ClientWriter(QWebSocket& s) : m_socket(s) { }
ClientWriter::~ClientWriter() {
    if (!m_written) { qWarning() << "Message should have been written!"; }
}

void ClientWriter::complete_message(QCborValue message, unsigned message_id) {
    m_message_list << QCborArray({ message_id, message });

    finished_writing_and_export();
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

    SignalInit init(value);
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
static void process_message(MessageState& ms, QCborMap value) {
    auto id = noo::id_from_message<noo::BufferID>(value);

    BufferInit init(value);

    auto* d = ms.state.buffer_list().handle_new(id, std::move(init));

    if (!d->is_data_ready()) {
        d->start_download(ms.state.network_manager());
    }
}
static void process_message(MessageState& ms, noodles::BufferDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.buffer_list().handle_delete(at);
}
static void process_message(MessageState&                        ms,
                            noodles::MaterialCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    MaterialInit md;

    EXIST_EXE(m.color(), { md.color = noo::convert(VALUE); })

    md.metallic     = m.metallic();
    md.roughness    = m.roughness();
    md.use_blending = m.use_blending();
    md.texture = m.texture_id() ? lookup(m_state, *m.texture_id()) : nullptr;

    m_state.material_list().handle_new(at, std::move(md));
}
static void process_message(MessageState&                  ms,
                            noodles::MaterialDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.material_list().handle_delete(at);
}
static void process_message(MessageState&                       ms,
                            noodles::TextureCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    TextureInit td;

    if (!m.reference()) {
        m_state.texture_list().handle_new(at, std::move(td));
        return;
    }

    td.buffer = lookup(m_state, *m.reference()->id());
    td.start  = m.reference()->start();
    td.size   = m.reference()->size();

    m_state.texture_list().handle_new(at, std::move(td));
}
static void process_message(MessageState& ms, noodles::TextureDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.texture_list().handle_delete(at);
}
static void process_message(MessageState&                     ms,
                            noodles::LightCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    LightData ld;

    EXIST_EXE(m.color(), { ld.color = noo::convert(VALUE); });
    ld.intensity = m.intensity();
    EXIST_EXE(m.spatial(), { ld.spatial = noo::convert(VALUE); });
    ld.type = (nooc::LightType)m.light_type();

    m_state.light_list().handle_new(at, std::move(ld));
}
static void process_message(MessageState& ms, noodles::LightDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.light_list().handle_delete(at);
}

ComponentRef convert(ClientState& state, ::noodles::ComponentRef const& cr) {
    qDebug() << cr.size() << cr.start() << cr.stride();
    return { .buffer = lookup(state, *cr.id()),
             .start  = cr.start(),
             .size   = cr.size(),
             .stride = cr.stride() };
}

static void process_message(MessageState&                  ms,
                            noodles::GeometryCreate const& m) {
    auto at = noo::convert_id(m.id());

    MeshData md;

    EXIST_EXE(m.extent(), md.extent = noo::convert(VALUE););

    EXIST_EXE(m.positions(), md.positions = convert(m_state, VALUE););
    EXIST_EXE(m.normals(), md.normals = convert(m_state, VALUE););
    EXIST_EXE(m.tex_coords(), md.textures = convert(m_state, VALUE););
    EXIST_EXE(m.colors(), md.colors = convert(m_state, VALUE););
    EXIST_EXE(m.lines(), md.lines = convert(m_state, VALUE););
    EXIST_EXE(m.triangles(), md.triangles = convert(m_state, VALUE););

    m_state.mesh_list().handle_new(at, std::move(md));
}
static void process_message(MessageState&                  ms,
                            noodles::GeometryDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.mesh_list().handle_delete(at);
}
static void process_message(MessageState&                     ms,
                            noodles::TableCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    TableData td;

    EXIST_EXE(m.name(), { td.name = VALUE.string_view(); })
    EXIST_EXE(m.methods_list(), { td.method_list = make_v(m_state, VALUE); })
    EXIST_EXE(m.signals_list(), { td.signal_list = make_v(m_state, VALUE); })

    m_state.table_list().handle_new(at, std::move(td));
}
static void process_message(MessageState& ms, noodles::TableDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.table_list().handle_delete(at);
}

static void process_message(MessageState&                    ms,
                            noodles::PlotCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    PlotData pd;

    if (m.type()) {
        auto& def = pd.type.emplace();

        switch (m.type_type()) {
        case noodles::PlotType::SimplePlot: {
            auto const* as = m.type_as_SimplePlot();
            def.emplace<PlotSimpleDelegate>().definition = QString::fromUtf8(
                as->definition()->data(), as->definition()->size());
        }
        case noodles::PlotType::URLPlot: {
            auto const* as = m.type_as_URLPlot();
            auto str = QString::fromUtf8(as->url()->data(), as->url()->size());
            def.emplace<PlotURLDelegate>().url = QUrl(str);
        }
        default:
            // nothing?
            pd.type.reset();
        }
    }

    EXIST_EXE(m.table(), { pd.table = lookup(m_state, VALUE); })


    EXIST_EXE(m.methods_list(), { pd.method_list = make_v(m_state, VALUE); })
    EXIST_EXE(m.signals_list(), { pd.signal_list = make_v(m_state, VALUE); })

    m_state.plot_list().handle_new(at, std::move(pd));
}
static void process_message(MessageState& ms, noodles::PlotDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.plot_list().handle_delete(at);
}

static void process_message(MessageState&                  ms,
                            noodles::DocumentUpdate const& m) {

    DocumentData dd;

    EXIST_EXE(m.methods_list(), { dd.method_list = make_v(m_state, VALUE); })
    EXIST_EXE(m.signals_list(), { dd.signal_list = make_v(m_state, VALUE); })

    m_state.document().update(dd);
}
static void process_message(MessageState& ms, noodles::DocumentReset const&) {
    m_state.document().clear();

    m_state.method_list().clear();
    m_state.signal_list().clear();
    m_state.buffer_list().clear();
    m_state.table_list().clear();
    m_state.texture_list().clear();
    m_state.light_list().clear();
    m_state.material_list().clear();
    m_state.mesh_list().clear();
    m_state.object_list().clear();
}
static void process_message(MessageState& ms, noodles::SignalInvoke const& m) {
    qDebug() << Q_FUNC_INFO;
    auto sig = lookup(m_state, *m.id());

    if (!sig) {
        qWarning() << "Unknown signal being invoked!";
        return;
    }

    QCborValueListRef av;
    if (m.signal_data()) { av = QCborValueListRef(m.signal_data()); }

    MethodContext   ctx;
    AttachedSignal* attached = nullptr;

    if (m.on_object()) {
        qDebug() << "Invoke on object";
        auto obj = lookup(m_state, *m.on_object());
        if (!obj) {
            qWarning() << "Unknown object for signal!";
            return;
        }
        ctx = obj;
        // listener notification
        attached = obj->attached_signals().find_by_delegate(sig);

    } else if (m.on_table()) {
        qDebug() << "Invoke on table";
        auto tbl = lookup(m_state, *m.on_table());
        if (!tbl) {
            qWarning() << "Unknown table for signal!";
            return;
        }
        ctx      = tbl;
        attached = tbl->attached_signals().find_by_delegate(sig);
    } else {
        qDebug() << "Invoke on document";
        ctx      = std::monostate();
        attached = m_state.document().attached_signals().find_by_delegate(sig);
    }

    // global notification
    emit sig->fired(ctx, av);

    // local notification
    if (attached) emit attached->fired(av);
}
static void process_message(MessageState& ms, noodles::MethodReply const& m) {
    auto ident = m.invoke_ident()->str();

    auto iter = m_state.inflight_methods().find(ident);

    if (iter == m_state.inflight_methods().end()) {
        qWarning() << "Reply for method we did not send!";
        return;
    }

    auto& reply_ptr = iter->second;

    Q_ASSERT(reply_ptr);

    QCborValueRef av;

    std::optional<MethodException> err;

    if (m.method_data()) { av = QCborValueRef(m.method_data()); }

    if (m.method_exception()) {
        err.emplace();

        auto* excp = m.method_exception();

        err->code = excp->code();

        if (excp->message()) {
            err->message = QString::fromUtf8(excp->message()->c_str());
        }

        if (excp->data()) { err->additional = QCborValueRef(excp->data()); }
    }

    qDebug() << "Completing reply...";
    reply_ptr->complete(std::move(av), err ? &err.value() : nullptr);

    qDebug() << "Clean up...";
    reply_ptr->deleteLater();

    m_state.inflight_methods().erase(iter);
}


static void process_server_message(MessageState const& ms,
                                   QCborValue const&   value) {
    qDebug() << Q_FUNC_INFO << value.toDiagnosticNotation();

    auto array = value.toArray();

    for (auto const& v : array) {
        auto pack    = v.toArray();
        auto id      = pack.at(0).toInteger(-1);
        auto content = pack.at(1);
    }

    switch (message.message_type()) {
    case noodles::ServerMessageType::NONE: return;
    case noodles::ServerMessageType::MethodCreate:
        return process_message(*message.message_as_MethodCreate());
    case noodles::ServerMessageType::MethodDelete:
        return process_message(*message.message_as_MethodDelete());
    case noodles::ServerMessageType::SignalCreate:
        return process_message(*message.message_as_SignalCreate());
    case noodles::ServerMessageType::SignalDelete:
        return process_message(*message.message_as_SignalDelete());
    case noodles::ServerMessageType::ObjectCreateUpdate:
        return process_message(*message.message_as_ObjectCreateUpdate());
    case noodles::ServerMessageType::ObjectDelete:
        return process_message(*message.message_as_ObjectDelete());
    case noodles::ServerMessageType::BufferCreate:
        return process_message(*message.message_as_BufferCreate());
    case noodles::ServerMessageType::BufferDelete:
        return process_message(*message.message_as_BufferDelete());
    case noodles::ServerMessageType::MaterialCreateUpdate:
        return process_message(*message.message_as_MaterialCreateUpdate());
    case noodles::ServerMessageType::MaterialDelete:
        return process_message(*message.message_as_MaterialDelete());
    case noodles::ServerMessageType::TextureCreateUpdate:
        return process_message(*message.message_as_TextureCreateUpdate());
    case noodles::ServerMessageType::TextureDelete:
        return process_message(*message.message_as_TextureDelete());
    case noodles::ServerMessageType::LightCreateUpdate:
        return process_message(*message.message_as_LightCreateUpdate());
    case noodles::ServerMessageType::LightDelete:
        return process_message(*message.message_as_LightDelete());
    case noodles::ServerMessageType::GeometryCreate:
        return process_message(*message.message_as_GeometryCreate());
    case noodles::ServerMessageType::GeometryDelete:
        return process_message(*message.message_as_GeometryDelete());
    case noodles::ServerMessageType::TableCreateUpdate:
        return process_message(*message.message_as_TableCreateUpdate());
    case noodles::ServerMessageType::TableDelete:
        return process_message(*message.message_as_TableDelete());
    case noodles::ServerMessageType::DocumentUpdate:
        return process_message(*message.message_as_DocumentUpdate());
    case noodles::ServerMessageType::DocumentReset:
        return process_message(*message.message_as_DocumentReset());
    case noodles::ServerMessageType::SignalInvoke:
        return process_message(*message.message_as_SignalInvoke());
    case noodles::ServerMessageType::MethodReply:
        return process_message(*message.message_as_MethodReply());
    }
}


void process_message(QWebSocket& s, InternalClientState& state, QByteArray m) {
    MessageState ms { .m_socket = s, .m_state = state };

    QCborParserError error;
    auto             value = QCborValue::fromCbor(m, &error);

    if (error.error != QCborError::NoError) {
        qWarning() << "Bad message from server:" << error.errorString();
        return;
    }

    process_server_message(m, value);
}

} // namespace nooc
