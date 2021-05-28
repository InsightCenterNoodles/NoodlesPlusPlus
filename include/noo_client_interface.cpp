#include "noo_client_interface.h"

#include "noo_common.h"
#include "src/client/client_common.h"
#include "src/client/clientstate.h"
#include "src/common/variant_tools.h"

#include <QWebSocket>

namespace nooc {

void PendingMethodReply::interpret() { }

PendingMethodReply::PendingMethodReply(MethodDelegate* d, MethodContext ctx)
    : m_method(d) {

    std::visit([&](auto x) { m_context = x; }, ctx);
}

void PendingMethodReply::call_direct(noo::AnyVarList&& l) {
    if (m_called)
        return complete(
            {},
            "Create a new invocation object instead of re-calling this one.");

    m_called = true;


    if (!m_method) return complete({}, "Tried to call a missing method");

    qDebug() << "Invoking" << noo::to_qstring(m_method->name());

    VMATCH(
        m_context,
        VCASE(std::monostate) {
            emit m_method->invoke(m_method->id(), std::monostate(), l, this);
        },
        VCASE(QPointer<ObjectDelegate> & p) {
            if (!p) {
                return complete(
                    {},
                    "Tried to call method on object that no longer exists!");
            }

            emit m_method->invoke(m_method->id(), p.data(), l, this);
        },
        VCASE(QPointer<TableDelegate> & p) {
            if (!p) {
                return complete(
                    {}, "Tried to call method on table that no longer exists!");
            }

            emit m_method->invoke(m_method->id(), p.data(), l, this);
        });
}

void PendingMethodReply::complete(noo::AnyVarRef v, QString string) {
    qDebug() << Q_FUNC_INFO;

    if (!string.isEmpty()) {
        emit recv_fail(string);
        return;
    }

    m_var = std::move(v);
    emit recv_data(m_var);
    interpret();
    this->deleteLater();
}

namespace translators {

void GetIntegerReply::interpret() {
    VMATCH_W(
        visit,
        m_var,
        VCASE(int64_t i) { emit recv(i); },
        VCASE(auto const&) {
            auto s = m_var.dump_string();

            emit recv_fail("Wrong result type expected" +
                           QString::fromStdString(s));
        });
}

void GetStringReply::interpret() {

    VMATCH_W(
        visit,
        m_var,
        VCASE(std::string_view str) { emit recv(noo::to_qstring(str)); },
        VCASE(auto const&) {
            auto s = m_var.dump_string();

            emit recv_fail("Wrong result type expected" +
                           QString::fromStdString(s));
        });
}

void GetStringListReply::interpret() {
    VMATCH_W(
        visit,
        m_var,
        VCASE(noo::AnyVarListRef const& l) {
            QStringList to_ret;

            l.for_each([&](auto, noo::AnyVarRef r) {
                to_ret << noo::to_qstring(r.to_string());
            });

            emit recv(to_ret);
        },
        VCASE(auto const&) {
            auto s = m_var.dump_string();

            emit recv_fail("Wrong result type expected" +
                           QString::fromStdString(s));
        });
}

void GetRealsReply::interpret() {
    auto rlist = m_var.coerce_real_list();
    emit recv(rlist);
}

void GetSelectionReply::interpret() {
    auto sel = noo::SelectionRef(m_var);
    emit recv(selection_id, sel);
}


} // namespace translators

// =============================================================================

AttachedMethodList::AttachedMethodList(MethodContext c) : m_context(c) { }

std::shared_ptr<MethodDelegate>
AttachedMethodList::find_direct_by_name(std::string_view name) const {
    // try to find it

    auto iter =
        std::find_if(m_list.begin(), m_list.end(), [name](auto const& v) {
            return v->name() == name;
        });

    if (iter == m_list.end()) { return nullptr; }

    return (*iter);
}

bool AttachedMethodList::check_direct_by_delegate(
    MethodDelegatePtr const& p) const {
    // try to find it

    auto iter = std::find(m_list.begin(), m_list.end(), p);

    if (iter == m_list.end()) { return false; }

    return true;
}

// =============================================================================

AttachedSignal::AttachedSignal(SignalDelegatePtr m) : m_signal(m) { }

SignalDelegatePtr const& AttachedSignal::delegate() const {
    return m_signal;
}

AttachedSignalList::AttachedSignalList(MethodContext c) : m_context(c) { }

AttachedSignal* AttachedSignalList::find_by_name(std::string_view name) const {
    for (auto const& p : m_list) {
        if (p->delegate()->name() == name) return p.get();
    }
    return nullptr;
}
AttachedSignal*
AttachedSignalList::find_by_delegate(SignalDelegatePtr const& ptr) const {
    for (auto const& p : m_list) {
        if (p->delegate() == ptr) return p.get();
    }
    return nullptr;
}

// =============================================================================

#define BASIC_DELEGATE_IMPL(C, ID, DATA)                                       \
    C::C(noo::ID i, DATA const&) : m_id(i) { }                                 \
    C::~C() = default;                                                         \
    noo::ID C::id() const { return m_id; }


MethodDelegate::MethodDelegate(noo::MethodID i, MethodData const& d)
    : m_id(i), m_method_name(d.method_name) { }
MethodDelegate::~MethodDelegate() = default;
noo::MethodID MethodDelegate::id() const {
    return m_id;
}
std::string_view MethodDelegate::name() const {
    return m_method_name;
}

void MethodDelegate::prepare_delete() { }

// =============================================================================

SignalDelegate::SignalDelegate(noo::SignalID i, SignalData const& d)
    : m_id(i), m_signal_name(d.signal_name) { }
SignalDelegate::~SignalDelegate() = default;
noo::SignalID SignalDelegate::id() const {
    return m_id;
}
std::string_view SignalDelegate::name() const {
    return m_signal_name;
}

void SignalDelegate::prepare_delete() { }

// =============================================================================

BASIC_DELEGATE_IMPL(BufferDelegate, BufferID, BufferData)

void BufferDelegate::prepare_delete() { }

// =============================================================================

TableDelegate::TableDelegate(noo::TableID i, TableData const& data)
    : m_id(i), m_attached_methods(this), m_attached_signals(this) {

    update(data);
}
TableDelegate::~TableDelegate() = default;

void TableDelegate::update(TableData const& data) {
    if (data.method_list) m_attached_methods = *data.method_list;
    if (data.signal_list) {

        for (auto c : m_spec_signals) {
            disconnect(c);
        }

        m_attached_signals = *data.signal_list;

        // find specific signals

        auto find_sig = [&](std::string_view n, auto targ) {
            auto* p = m_attached_signals.find_by_name(n);

            if (p) {
                auto c = connect(p, &AttachedSignal::fired, this, targ);
                m_spec_signals.push_back(c);
            }
        };

        find_sig("tbl_reset", &TableDelegate::interp_table_reset);
        find_sig("tbl_updated", &TableDelegate::interp_table_update);
        find_sig("tbl_rows_removed", &TableDelegate::interp_table_remove);
        find_sig("tbl_selection_updated",
                 &TableDelegate::interp_table_sel_update);
    }

    this->on_update(data);
}

void TableDelegate::on_update(TableData const&) { }

AttachedMethodList const& TableDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& TableDelegate::attached_signals() const {
    return m_attached_signals;
}
noo::TableID TableDelegate::id() const {
    return m_id;
}

void TableDelegate::prepare_delete() { }

void TableDelegate::on_table_initialize(noo::AnyVarListRef const&,
                                        noo::AnyVarRef,
                                        noo::AnyVarListRef const&,
                                        noo::AnyVarListRef const&) { }

void TableDelegate::on_table_reset() { }
void TableDelegate::on_table_updated(noo::AnyVarRef, noo::AnyVarRef) { }
void TableDelegate::on_table_rows_removed(noo::AnyVarRef) { }
void TableDelegate::on_table_selection_updated(std::string_view,
                                               noo::SelectionRef const&) { }

PendingMethodReply* TableDelegate::subscribe() const {
    auto* p = attached_methods().new_call_by_name<SubscribeInitReply>(
        "tbl_subscribe");

    connect(p,
            &SubscribeInitReply::recv,
            this,
            &TableDelegate::on_table_initialize);

    p->call();

    return p;
}


PendingMethodReply*
TableDelegate::request_row_insert(noo::AnyVarList&& row) const {
    auto new_list = noo::AnyVarList(row.size());

    for (size_t i = 0; i < row.size(); i++) {
        new_list[i] = noo::AnyVarList({ row[i] });
    }

    return request_rows_insert(std::move(new_list));
}

PendingMethodReply*
TableDelegate::request_rows_insert(noo::AnyVarList&& columns) const {
    auto* p = attached_methods().new_call_by_name("tbl_insert");

    p->call(columns);

    return p;
}

PendingMethodReply*
TableDelegate::request_row_update(int64_t key, noo::AnyVarList&& row) const {
    auto new_list = noo::AnyVarList(row.size());

    for (size_t i = 0; i < row.size(); i++) {
        new_list[i] = noo::AnyVarList({ row[i] });
    }

    return request_rows_update({ key }, std::move(new_list));
}

PendingMethodReply*
TableDelegate::request_rows_update(std::vector<int64_t>&& keys,
                                   noo::AnyVarList&&      columns) const {
    auto* p = attached_methods().new_call_by_name("tbl_update");

    p->call(keys, columns);

    return p;
}

PendingMethodReply*
TableDelegate::request_deletion(std::span<int64_t> keys) const {
    auto* p = attached_methods().new_call_by_name("tbl_remove");

    p->call(keys);

    return p;
}

PendingMethodReply* TableDelegate::request_clear() const {
    auto* p = attached_methods().new_call_by_name("tbl_clear");

    p->call();

    return p;
}

PendingMethodReply*
TableDelegate::request_selection_update(std::string_view name,
                                        noo::Selection   selection) const {
    auto* p = attached_methods().new_call_by_name("tbl_update_selection");

    p->call(name, selection.to_any());

    return p;
}

void TableDelegate::interp_table_reset(noo::AnyVarListRef const&) {
    this->on_table_reset();
}

void TableDelegate::interp_table_update(noo::AnyVarListRef const& ref) {
    // row of keys, then row of columns
    if (ref.size() < 2) {
        qWarning() << Q_FUNC_INFO << "Malformed signal from server";
        return;
    }

    auto keylist = ref[0];
    auto cols    = ref[1];

    this->on_table_updated(keylist, cols);
}

void TableDelegate::interp_table_remove(noo::AnyVarListRef const& ref) {
    if (ref.size() < 1) {
        qWarning() << Q_FUNC_INFO << "Malformed signal from server";
        return;
    }

    this->on_table_rows_removed(ref[0]);
}

void TableDelegate::interp_table_sel_update(noo::AnyVarListRef const& ref) {
    if (ref.size() < 2) {
        qWarning() << Q_FUNC_INFO << "Malformed signal from server";
        return;
    }

    auto str     = ref[0].to_string();
    auto sel_ref = noo::SelectionRef(ref[1]);

    this->on_table_selection_updated(str, sel_ref);
}


// =============================================================================

BASIC_DELEGATE_IMPL(TextureDelegate, TextureID, TextureData)
void TextureDelegate::update(TextureData const&) { }
void TextureDelegate::on_update(TextureData const&) { }

void TextureDelegate::prepare_delete() { }

// =============================================================================

BASIC_DELEGATE_IMPL(LightDelegate, LightID, LightData)
void LightDelegate::update(LightData const&) { }
void LightDelegate::on_update(LightData const&) { }

void LightDelegate::prepare_delete() { }

// =============================================================================

BASIC_DELEGATE_IMPL(MaterialDelegate, MaterialID, MaterialData)
void MaterialDelegate::update(MaterialData const&) { }
void MaterialDelegate::on_update(MaterialData const&) { }

void MaterialDelegate::prepare_delete() { }

// =============================================================================

BASIC_DELEGATE_IMPL(MeshDelegate, MeshID, MeshData)

void MeshDelegate::prepare_delete() { }

// =============================================================================

ObjectDelegate::ObjectDelegate(noo::ObjectID i, ObjectUpdateData const& data)
    : m_id(i), m_attached_methods(this), m_attached_signals(this) {

    std::string       m_name;
    ObjectDelegatePtr m_parent;

    if (data.name) {
        m_name = *data.name;
    } else {
        m_name = i.to_string();
    }

    if (data.parent) { m_parent = *data.parent; }

    if (data.method_list) m_attached_methods = *data.method_list;
    if (data.signal_list) m_attached_signals = *data.signal_list;
}
ObjectDelegate::~ObjectDelegate() = default;
void ObjectDelegate::update(ObjectUpdateData const& data) {
    if (data.method_list) m_attached_methods = *data.method_list;
    if (data.signal_list) m_attached_signals = *data.signal_list;

    this->on_update(data);
}
void                      ObjectDelegate::on_update(ObjectUpdateData const&) { }
AttachedMethodList const& ObjectDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& ObjectDelegate::attached_signals() const {
    return m_attached_signals;
}
noo::ObjectID ObjectDelegate::id() const {
    return m_id;
}

void ObjectDelegate::prepare_delete() { }

// =============================================================================

DocumentDelegate::DocumentDelegate()
    : m_attached_methods(std::monostate()),
      m_attached_signals(std::monostate()) {
    // m_attached_methods = data.method_list;
    // m_attached_signals = data.signal_list;
}
DocumentDelegate::~DocumentDelegate() = default;
void DocumentDelegate::update(DocumentData const& data) {
    m_attached_methods = data.method_list;
    m_attached_signals = data.signal_list;
    this->on_update(data);
    emit updated();
}
void DocumentDelegate::clear() {
    m_attached_methods = {};
    m_attached_signals = {};
    this->on_clear();
    emit updated();
}

void DocumentDelegate::on_update(DocumentData const&) { }
void DocumentDelegate::on_clear() { }

AttachedMethodList const& DocumentDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& DocumentDelegate::attached_signals() const {
    return m_attached_signals;
}

// =============================================================================

class ClientCore : public QObject {
    ClientConnection* m_owning_connection;

    ClientDelegates m_makers;

    std::optional<ClientState> m_state;

    bool       m_connecting = true;
    QWebSocket m_socket;

public:
    ClientCore(ClientConnection* conn,
               QUrl const&       url,
               ClientDelegates&& makers)
        : m_owning_connection(conn), m_makers(std::move(makers)) {
        connect(&m_socket,
                &QWebSocket::connected,
                this,
                &ClientCore::socket_connected);
        connect(&m_socket,
                &QWebSocket::disconnected,
                this,
                &ClientCore::socket_closed);
        connect(&m_socket,
                QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
                [=](QAbstractSocket::SocketError) {
                    emit m_owning_connection->socket_error(
                        m_socket.errorString());
                });


        m_socket.open(url);
    }

    ~ClientCore() { qDebug() << Q_FUNC_INFO; }

    bool is_connecting() const { return m_connecting; }

    TextureDelegatePtr get(noo::TextureID id) {
        if (!m_state) return {};

        return m_state->texture_list().comp_at(id);
    }
    BufferDelegatePtr get(noo::BufferID id) {
        if (!m_state) return {};

        return m_state->buffer_list().comp_at(id);
    }
    TableDelegatePtr get(noo::TableID id) {
        if (!m_state) return {};

        return m_state->table_list().comp_at(id);
    }
    LightDelegatePtr get(noo::LightID id) {
        if (!m_state) return {};

        return m_state->light_list().comp_at(id);
    }
    MaterialDelegatePtr get(noo::MaterialID id) {
        if (!m_state) return {};

        return m_state->material_list().comp_at(id);
    }
    MeshDelegatePtr get(noo::MeshID id) {
        if (!m_state) return {};

        return m_state->mesh_list().comp_at(id);
    }
    ObjectDelegatePtr get(noo::ObjectID id) {
        if (!m_state) return {};

        return m_state->object_list().comp_at(id);
    }
    SignalDelegatePtr get(noo::SignalID id) {
        if (!m_state) return {};

        return m_state->signal_list().comp_at(id);
    }
    MethodDelegatePtr get(noo::MethodID id) {
        if (!m_state) return {};

        return m_state->method_list().comp_at(id);
    }

private slots:
    void socket_connected() {
        m_connecting = false;
        m_state.emplace(m_socket, m_makers);
        emit m_owning_connection->connected();
    }
    void socket_closed() {
        emit m_owning_connection->disconnected();
        m_state.reset();
    }
};


std::unique_ptr<ClientCore>
create_client(ClientConnection* conn, QUrl server, ClientDelegates&& d) {

#define CREATE_DEFAULT(E, ID, DATA, DEL)                                       \
    if (!d.E) {                                                                \
        d.E = [](noo::ID id, DATA const& d) {                                  \
            return std::make_shared<DEL>(id, d);                               \
        };                                                                     \
    }

    CREATE_DEFAULT(tex_maker, TextureID, TextureData, TextureDelegate);
    CREATE_DEFAULT(buffer_maker, BufferID, BufferData, BufferDelegate);
    CREATE_DEFAULT(table_maker, TableID, TableData, TableDelegate);
    CREATE_DEFAULT(light_maker, LightID, LightData, LightDelegate);
    CREATE_DEFAULT(mat_maker, MaterialID, MaterialData, MaterialDelegate);
    CREATE_DEFAULT(mesh_maker, MeshID, MeshData, MeshDelegate);
    CREATE_DEFAULT(object_maker, ObjectID, ObjectUpdateData, ObjectDelegate);
    CREATE_DEFAULT(sig_maker, SignalID, SignalData, SignalDelegate);
    CREATE_DEFAULT(method_maker, MethodID, MethodData, MethodDelegate);


    if (!d.doc_maker) {
        d.doc_maker = []() { return std::make_shared<DocumentDelegate>(); };
    }


    return std::make_unique<ClientCore>(conn, server, std::move(d));
}

ClientConnection::ClientConnection(QObject* parent) : QObject(parent) { }
ClientConnection::~ClientConnection() = default;

void ClientConnection::open(QUrl server, ClientDelegates&& delegates) {
    if (m_data) {
        if (m_data->is_connecting()) { return; }

        // disconnect current connection
        m_data.reset();
    }

    m_data = create_client(this, server, std::move(delegates));
}

TextureDelegatePtr ClientConnection::get(noo::TextureID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
BufferDelegatePtr ClientConnection::get(noo::BufferID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
TableDelegatePtr ClientConnection::get(noo::TableID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
LightDelegatePtr ClientConnection::get(noo::LightID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MaterialDelegatePtr ClientConnection::get(noo::MaterialID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MeshDelegatePtr ClientConnection::get(noo::MeshID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
ObjectDelegatePtr ClientConnection::get(noo::ObjectID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
SignalDelegatePtr ClientConnection::get(noo::SignalID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MethodDelegatePtr ClientConnection::get(noo::MethodID id) {
    if (!m_data) return {};

    return m_data->get(id);
}

} // namespace nooc
