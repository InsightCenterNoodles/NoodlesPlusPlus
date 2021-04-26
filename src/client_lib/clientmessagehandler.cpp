#include "clientmessagehandler.h"

#include "../shared/interface_tools.h"
#include "clientstate.h"

namespace nooc {

void ClientWriter::finished_writing_and_export() {
    auto* ptr  = m_builder.GetBufferPointer();
    auto  size = m_builder.GetSize();

    QByteArray array(reinterpret_cast<char*>(ptr), size);

    m_socket.sendBinaryMessage(array);

    m_written = true;
}

ClientWriter::ClientWriter(QWebSocket& s) : m_socket(s) { }
ClientWriter::~ClientWriter() {
    if (!m_written) { qWarning() << "Message should have been written!"; }
}


static MethodDelegatePtr lookup(ClientState&               state,
                                ::noodles::MethodID const& ptr) {
    return state.method_list().comp_at(noo::convert_id(ptr));
}
static SignalDelegatePtr lookup(ClientState&               state,
                                ::noodles::SignalID const& ptr) {
    return state.signal_list().comp_at(noo::convert_id(ptr));
}
static BufferDelegatePtr lookup(ClientState&               state,
                                ::noodles::BufferID const& ptr) {
    return state.buffer_list().comp_at(noo::convert_id(ptr));
}
static TableDelegatePtr lookup(ClientState&              state,
                               ::noodles::TableID const& ptr) {
    return state.table_list().comp_at(noo::convert_id(ptr));
}
static TextureDelegatePtr lookup(ClientState&                state,
                                 ::noodles::TextureID const& ptr) {
    return state.texture_list().comp_at(noo::convert_id(ptr));
}
static LightDelegatePtr lookup(ClientState&              state,
                               ::noodles::LightID const& ptr) {
    return state.light_list().comp_at(noo::convert_id(ptr));
}
static MaterialDelegatePtr lookup(ClientState&                 state,
                                  ::noodles::MaterialID const& ptr) {
    return state.material_list().comp_at(noo::convert_id(ptr));
}
static MeshDelegatePtr lookup(ClientState&                 state,
                              ::noodles::GeometryID const& ptr) {
    return state.mesh_list().comp_at(noo::convert_id(ptr));
}
static ObjectDelegatePtr lookup(ClientState&               state,
                                ::noodles::ObjectID const& ptr) {
    return state.object_list().comp_at(noo::convert_id(ptr));
}

// =============================================================================

#define EXIST_EXE(NPTR, OP)                                                    \
    if (NPTR) {                                                                \
        auto& VALUE = *NPTR;                                                   \
        OP                                                                     \
    }

template <class U>
auto make_v(ClientState& state, U const& arr) {
    using T = decltype(lookup(state, *(*arr.begin())));
    std::vector<T> ret;

    for (auto* item : arr) {
        ret.push_back(lookup(state, *item));
    }

    return ret;
};

void MessageHandler::process_message(noodles::MethodCreate const& c) {
    auto at = noo::convert_id(c.id());

    MethodData md;
    md.method_name          = c.name()->string_view();
    md.documentation        = c.documentation()->string_view();
    md.return_documentation = c.returnDoc()->string_view();

    std::vector<ArgDoc> arg_docs;

    for (auto* d : *c.argDoc()) {
        arg_docs.push_back(
            { d->name()->string_view(), d->doc()->string_view() });
    }

    md.argument_documentation = arg_docs;

    MethodDelegate* delegate =
        m_state.method_list().handle_new(at, std::move(md));

    QObject::connect(delegate,
                     &MethodDelegate::invoke,
                     &m_state,
                     &ClientState::on_method_ask_invoke);
}

void MessageHandler::process_message(noodles::MethodDelete const& c) {
    auto at = noo::convert_id(c.id());

    m_state.method_list().handle_delete(at);
}

void MessageHandler::process_message(noodles::SignalCreate const& m) {
    auto at = noo::convert_id(m.id());

    std::vector<ArgDoc> arg_docs;

    for (auto* d : *m.argDoc()) {
        arg_docs.push_back(
            { d->name()->string_view(), d->doc()->string_view() });
    }

    SignalData sd { .signal_name            = m.name()->string_view(),
                    .documentation          = m.documentation()->string_view(),
                    .argument_documentation = arg_docs };

    m_state.signal_list().handle_new(at, std::move(sd));
}
void MessageHandler::process_message(noodles::SignalDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.signal_list().handle_delete(at);
}
void MessageHandler::process_message(noodles::ObjectCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());
    qDebug() << Q_FUNC_INFO << at.to_qstring();

    ObjectUpdateData od;

    EXIST_EXE(m.name(), { od.name = VALUE.string_view(); })
    EXIST_EXE(m.parent(), { od.parent = lookup(m_state, VALUE); })
    EXIST_EXE(m.transform(), { od.transform = noo::convert(VALUE); })

    EXIST_EXE(m.material(), { od.material = lookup(m_state, VALUE); })
    EXIST_EXE(m.mesh(), { od.mesh = lookup(m_state, VALUE); })

    EXIST_EXE(m.lights(), { od.lights = make_v(m_state, VALUE); })
    EXIST_EXE(m.tables(), { od.tables = make_v(m_state, VALUE); })

    EXIST_EXE(m.instances(), {
        if (VALUE.size() == 0) {
            od.instances = std::span<glm::mat4 const>();
        } else {
            static_assert(sizeof(glm::mat4) == sizeof(noodles::Mat4));

            assert(VALUE.Data());

            auto const* lm = reinterpret_cast<glm::mat4 const*>(VALUE.Data());

            od.instances =
                std::span<glm::mat4 const>(lm, m.instances()->size());

            //            for (auto const& mat : *(od.instances)) {
            //                qDebug() << mat[0][0];
            //            }
        }
    })

    EXIST_EXE(m.tags(), {
        std::vector<std::string_view> t;

        for (auto* str : VALUE) {
            t.push_back(str->string_view());
        }

        od.tags = t;
    })

    EXIST_EXE(m.methods_list(), { od.method_list = make_v(m_state, VALUE); })
    EXIST_EXE(m.signals_list(), { od.signal_list = make_v(m_state, VALUE); })

    EXIST_EXE(m.text(), {
        ObjectText& ot = od.text.emplace();

        ot.text   = VALUE.text()->c_str();
        ot.font   = VALUE.font()->c_str();
        ot.height = VALUE.height();
        ot.width  = VALUE.opt_width();
    })

    m_state.object_list().handle_new(at, std::move(od));
}
void MessageHandler::process_message(noodles::ObjectDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.object_list().handle_delete(at);
}
void MessageHandler::process_message(noodles::BufferCreate const& m) {
    auto at = noo::convert_id(m.id());

    BufferData bd;

    if (m.bytes()) {
        bd.data = { (std::byte const*)m.bytes()->data(), m.bytes()->size() };
    }

    if (m.url()) {
        bd.url = QUrl(QString::fromLocal8Bit(m.url()->data(), m.url()->size()));
    }

    m_state.buffer_list().handle_new(at, std::move(bd));
}
void MessageHandler::process_message(noodles::BufferDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.buffer_list().handle_delete(at);
}
void MessageHandler::process_message(noodles::MaterialCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    MaterialData md;

    EXIST_EXE(m.color(), { md.color = noo::convert(VALUE); })

    md.metallic     = m.metallic();
    md.roughness    = m.roughness();
    md.use_blending = m.use_blending();
    md.texture = m.texture_id() ? lookup(m_state, *m.texture_id()) : nullptr;

    m_state.material_list().handle_new(at, std::move(md));
}
void MessageHandler::process_message(noodles::MaterialDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.material_list().handle_delete(at);
}
void MessageHandler::process_message(noodles::TextureCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    TextureData td;

    if (!m.reference()) {
        m_state.texture_list().handle_new(at, std::move(td));
        return;
    }

    td.buffer = lookup(m_state, *m.reference()->id());
    td.start  = m.reference()->start();
    td.size   = m.reference()->size();

    m_state.texture_list().handle_new(at, std::move(td));
}
void MessageHandler::process_message(noodles::TextureDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.texture_list().handle_delete(at);
}
void MessageHandler::process_message(noodles::LightCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    LightData ld;

    EXIST_EXE(m.color(), { ld.color = noo::convert(VALUE); });

    m_state.light_list().handle_new(at, std::move(ld));
}
void MessageHandler::process_message(noodles::LightDelete const& m) {
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

void MessageHandler::process_message(noodles::GeometryCreate const& m) {
    auto at = noo::convert_id(m.id());

    MeshData md;

    EXIST_EXE(m.min_extent(), md.extent_min = noo::convert(VALUE););
    EXIST_EXE(m.max_extent(), md.extent_max = noo::convert(VALUE););

    EXIST_EXE(m.positions(), md.positions = convert(m_state, VALUE););
    EXIST_EXE(m.normals(), md.normals = convert(m_state, VALUE););
    EXIST_EXE(m.texCoords(), md.textures = convert(m_state, VALUE););
    EXIST_EXE(m.colors(), md.colors = convert(m_state, VALUE););
    EXIST_EXE(m.lines(), md.lines = convert(m_state, VALUE););
    EXIST_EXE(m.triangles(), md.triangles = convert(m_state, VALUE););

    m_state.mesh_list().handle_new(at, std::move(md));
}
void MessageHandler::process_message(noodles::GeometryDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.mesh_list().handle_delete(at);
}
void MessageHandler::process_message(noodles::TableCreateUpdate const& m) {
    auto at = noo::convert_id(m.id());

    TableData td;

    EXIST_EXE(m.name(), { td.name = VALUE.string_view(); })
    EXIST_EXE(m.methods_list(), { td.method_list = make_v(m_state, VALUE); })
    EXIST_EXE(m.signals_list(), { td.signal_list = make_v(m_state, VALUE); })

    m_state.table_list().handle_new(at, std::move(td));
}
void MessageHandler::process_message(noodles::TableDelete const& m) {
    auto at = noo::convert_id(m.id());

    m_state.table_list().handle_delete(at);
}
void MessageHandler::process_message(noodles::DocumentUpdate const& m) {

    DocumentData dd;

    EXIST_EXE(m.methods_list(), { dd.method_list = make_v(m_state, VALUE); })
    EXIST_EXE(m.signals_list(), { dd.signal_list = make_v(m_state, VALUE); })

    m_state.document().update(dd);
}
void MessageHandler::process_message(noodles::DocumentReset const&) {
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
void MessageHandler::process_message(noodles::SignalInvoke const& m) {
    auto sig = lookup(m_state, *m.id());

    if (!sig) {
        qWarning() << "Unknown signal being invoked!";
        return;
    }

    noo::AnyVarList av;
    if (m.signal_data()) { noo::read_to_array(m.signal_data(), av); }

    MethodContext   ctx;
    AttachedSignal* attached = nullptr;

    if (m.on_object()) {
        auto obj = lookup(m_state, *m.on_object());
        if (!obj) {
            qWarning() << "Unknown object for signal!";
            return;
        }
        ctx = obj.get();
        // listener notification
        attached = obj->attached_signals().find_by_delegate(sig);

    } else if (m.on_table()) {
        auto tbl = lookup(m_state, *m.on_table());
        if (!tbl) {
            qWarning() << "Unknown table for signal!";
            return;
        }
        ctx      = tbl.get();
        attached = tbl->attached_signals().find_by_delegate(sig);
    } else {
        ctx      = std::monostate();
        attached = m_state.document().attached_signals().find_by_delegate(sig);
    }

    // global notification
    emit sig->fired(ctx, av);

    // local notification
    if (attached) emit attached->fired(av);
}
void MessageHandler::process_message(noodles::MethodReply const& m) {
    auto ident = m.invoke_ident()->str();

    auto iter = m_state.inflight_methods().find(ident);

    if (iter == m_state.inflight_methods().end()) {
        qWarning() << "Reply for method we did not send!";
        return;
    }

    auto& reply_ptr = iter->second;

    Q_ASSERT(reply_ptr);

    noo::AnyVar av;
    QString     err;

    if (m.method_data()) { noo::read_to(m.method_data(), av); }

    if (m.method_exception()) {
        err = QString::fromLocal8Bit(m.method_exception()->c_str());
    }

    qDebug() << "Completing reply...";
    reply_ptr->complete(std::move(av), err);

    qDebug() << "Clean up...";
    reply_ptr->deleteLater();

    m_state.inflight_methods().erase(iter);
}


void MessageHandler::process_message(noodles::ServerMessage const& message) {
    qDebug() << Q_FUNC_INFO
             << noodles::EnumNameServerMessageType(message.message_type());
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

MessageHandler::MessageHandler(QWebSocket& s, ClientState& state, QByteArray m)
    : m_socket(s), m_state(state), m_bin_message(m) { }

void MessageHandler::process() {
    flatbuffers::Verifier v((uint8_t const*)m_bin_message.data(),
                            m_bin_message.size());

    if (!noodles::VerifyServerMessagesBuffer(v)) {
        qCritical() << "Malformed message from server, disconnecting!";
        m_socket.close();
        return;
    }

    auto* server_messages = noodles::GetServerMessages(m_bin_message.data());

    if (!server_messages) return;

    auto* message_list = server_messages->messages();

    if (!message_list) return;

    for (auto* message : *message_list) {
        if (!message) continue;
        process_message(*message);
    }
    qDebug() << Q_FUNC_INFO << "Done";
}

} // namespace nooc
