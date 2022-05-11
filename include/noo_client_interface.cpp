#include "noo_client_interface.h"

#include "noo_common.h"
#include "src/client/client_common.h"
#include "src/client/clientstate.h"
#include "src/common/variant_tools.h"

#include <QWebSocket>

#include <unordered_set>

namespace nooc {

QString to_string(MethodException const& me) {
    auto str = me.additional.dump_string();
    return QString("Code %1: %2 Additional: %3")
        .arg(me.code)
        .arg(me.message)
        .arg(str.c_str());
}

void PendingMethodReply::interpret() { }

PendingMethodReply::PendingMethodReply(MethodDelegate* d, MethodContext ctx)
    : m_method(d) {

    std::visit([&](auto x) { m_context = x; }, ctx);
}

void PendingMethodReply::call_direct(QCborValueList&& l) {
    if (m_called) {
        MethodException exception {
            .code    = -10000,
            .message = "Create a new invocation object instead of re-calling "
                       "this one",
        };

        return complete({}, &exception);
    }

    m_called = true;


    if (!m_method) {
        MethodException exception {
            .code    = noo::ErrorCodes::METHOD_NOT_FOUND,
            .message = "Method does not exist on this object",
        };
        return complete({}, &exception);
    }

    // qDebug() << "Invoking" << noo::to_qstring(m_method->name());

    VMATCH(
        m_context,
        VCASE(std::monostate) {
            emit m_method->invoke(m_method->id(), std::monostate(), l, this);
        },
        VCASE(QPointer<EntityDelegate> & p) {
            if (!p) {
                MethodException exception {
                    .code    = noo::ErrorCodes::METHOD_NOT_FOUND,
                    .message = "Method does not exist on this object anymore",
                };
                return complete({}, &exception);
            }

            emit m_method->invoke(m_method->id(), p.data(), l, this);
        },
        VCASE(QPointer<TableDelegate> & p) {
            if (!p) {
                MethodException exception {
                    .code    = noo::ErrorCodes::METHOD_NOT_FOUND,
                    .message = "Method does not exist on this table anymore",
                };
                return complete({}, &exception);
            }

            emit m_method->invoke(m_method->id(), p.data(), l, this);
        },
        VCASE(QPointer<PlotDelegate> & p) {
            if (!p) {
                MethodException exception {
                    .code    = noo::ErrorCodes::METHOD_NOT_FOUND,
                    .message = "Method does not exist on this table anymore",
                };
                return complete({}, &exception);
            }

            emit m_method->invoke(m_method->id(), p.data(), l, this);
        }

    );
}

void PendingMethodReply::complete(QCborValueRef    v,
                                  MethodException* exception_info) {
    qDebug() << Q_FUNC_INFO;

    if (exception_info) {
        emit recv_exception(*exception_info);
        emit recv_fail(to_string(*exception_info));
        return;
    }

    m_var = std::move(v);
    emit recv_data(m_var);
    interpret();
    this->deleteLater();
}

namespace replies {

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
        VCASE(QCborValueListRef const& l) {
            QStringList to_ret;

            l.for_each([&](auto, QCborValueRef r) {
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

} // namespace replies

// =============================================================================

AttachedMethodList::AttachedMethodList(MethodContext c) : m_context(c) { }

MethodDelegate*
AttachedMethodList::find_direct_by_name(std::string_view name) const {
    // try to find it

    auto iter =
        std::find_if(m_list.begin(), m_list.end(), [name](auto const& v) {
            return v->name() == name;
        });

    if (iter == m_list.end()) { return nullptr; }

    return (*iter);
}

bool AttachedMethodList::check_direct_by_delegate(MethodDelegate* p) const {
    // try to find it

    auto iter = std::find(m_list.begin(), m_list.end(), p);

    if (iter == m_list.end()) { return false; }

    return true;
}

// =============================================================================

AttachedSignal::AttachedSignal(SignalDelegate* m) : m_signal(m) { }

SignalDelegate* AttachedSignal::delegate() const {
    return m_signal;
}

AttachedSignalList::AttachedSignalList(MethodContext c) : m_context(c) { }

AttachedSignalList&
AttachedSignalList::operator=(std::vector<SignalDelegate*> const& l) {
    // we want to preserve signals already attached. so we want to check if the
    // new list has signals we already have.

    std::unordered_set<SignalDelegate*> new_list;
    for (auto const& p : l) {
        if (!p) continue;
        new_list.insert(p);
    }


    std::vector<std::unique_ptr<AttachedSignal>> to_keep;

    for (auto& p : m_list) {
        auto* ptr = p->delegate();
        assert(ptr);
        if (new_list.contains(ptr)) {
            to_keep.emplace_back(std::move(p));
            new_list.erase(ptr);
        }
    }

    m_list = std::move(to_keep);

    for (auto const& p : new_list) {
        m_list.emplace_back(std::make_unique<AttachedSignal>(p));
    }
    return *this;
}

AttachedSignal* AttachedSignalList::find_by_name(std::string_view name) const {
    for (auto const& p : m_list) {
        if (p->delegate()->name() == name) return p.get();
    }
    return nullptr;
}

AttachedSignal*
AttachedSignalList::find_by_delegate(SignalDelegate* ptr) const {
    for (auto const& p : m_list) {
        if (p->delegate() == ptr) return p.get();
    }
    return nullptr;
}

// =============================================================================

#define BASIC_DELEGATE_IMPL(C, ID, DATA)                                       \
    C::C(noo::ID i, DATA const&) : m_id(i) { }                                 \
    C::~C() = default;                                                         \
    noo::ID C::id() const {                                                    \
        return m_id;                                                           \
    }


MethodDelegate::MethodDelegate(noo::MethodID i, MethodInit const& d)
    : m_id(i), m_method_name(d.method_name) { }
MethodDelegate::~MethodDelegate() = default;
noo::MethodID MethodDelegate::id() const {
    return m_id;
}
std::string_view MethodDelegate::name() const {
    return m_method_name;
}

// =============================================================================

SignalDelegate::SignalDelegate(noo::SignalID i, SignalInit const& d)
    : m_id(i), m_signal_name(d.signal_name) { }
SignalDelegate::~SignalDelegate() = default;
noo::SignalID SignalDelegate::id() const {
    return m_id;
}
std::string_view SignalDelegate::name() const {
    return m_signal_name;
}

// =============================================================================

BASIC_DELEGATE_IMPL(BufferDelegate, BufferID, BufferInit)

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

void TableDelegate::on_table_initialize(QCborValueListRef const&,
                                        QCborValueRef,
                                        QCborValueListRef const&,
                                        QCborValueListRef const&) { }

void TableDelegate::on_table_reset() { }
void TableDelegate::on_table_updated(QCborValueRef, QCborValueRef) { }
void TableDelegate::on_table_rows_removed(QCborValueRef) { }
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
TableDelegate::request_row_insert(QCborValueList&& row) const {
    auto new_list = QCborValueList(row.size());

    for (size_t i = 0; i < row.size(); i++) {
        new_list[i] = QCborValueList({ row[i] });
    }

    return request_rows_insert(std::move(new_list));
}

PendingMethodReply*
TableDelegate::request_rows_insert(QCborValueList&& columns) const {
    auto* p = attached_methods().new_call_by_name("tbl_insert");

    p->call(columns);

    return p;
}

PendingMethodReply*
TableDelegate::request_row_update(int64_t key, QCborValueList&& row) const {
    auto new_list = QCborValueList(row.size());

    for (size_t i = 0; i < row.size(); i++) {
        new_list[i] = QCborValueList({ row[i] });
    }

    return request_rows_update({ key }, std::move(new_list));
}

PendingMethodReply*
TableDelegate::request_rows_update(std::vector<int64_t>&& keys,
                                   QCborValueList&&       columns) const {
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

void TableDelegate::interp_table_reset(QCborValueListRef const&) {
    this->on_table_reset();
}

void TableDelegate::interp_table_update(QCborValueListRef const& ref) {
    // row of keys, then row of columns
    if (ref.size() < 2) {
        qWarning() << Q_FUNC_INFO << "Malformed signal from server";
        return;
    }

    auto keylist = ref[0];
    auto cols    = ref[1];

    this->on_table_updated(keylist, cols);
}

void TableDelegate::interp_table_remove(QCborValueListRef const& ref) {
    if (ref.size() < 1) {
        qWarning() << Q_FUNC_INFO << "Malformed signal from server";
        return;
    }

    this->on_table_rows_removed(ref[0]);
}

void TableDelegate::interp_table_sel_update(QCborValueListRef const& ref) {
    if (ref.size() < 2) {
        qWarning() << Q_FUNC_INFO << "Malformed signal from server";
        return;
    }

    auto str     = ref[0].to_string();
    auto sel_ref = noo::SelectionRef(ref[1]);

    this->on_table_selection_updated(str, sel_ref);
}


// =============================================================================

BASIC_DELEGATE_IMPL(TextureDelegate, TextureID, TextureInit)
void TextureDelegate::update(TextureInit const& d) {
    this->on_update(d);
}
void TextureDelegate::on_update(TextureInit const&) { }

// =============================================================================

BASIC_DELEGATE_IMPL(LightDelegate, LightID, LightData)
void LightDelegate::update(LightData const& d) {
    this->on_update(d);
}
void LightDelegate::on_update(LightData const&) { }

// =============================================================================

BASIC_DELEGATE_IMPL(MaterialDelegate, MaterialID, MaterialInit)
void MaterialDelegate::update(MaterialInit const& d) {
    this->on_update(d);
}
void MaterialDelegate::on_update(MaterialInit const&) { }

// =============================================================================

BASIC_DELEGATE_IMPL(MeshDelegate, GeometryID, MeshData)

void MeshDelegate::prepare_delete() { }

// =============================================================================

EntityDelegate::EntityDelegate(noo::EntityID i, EntityUpdateData const& data)
    : m_id(i), m_attached_methods(this), m_attached_signals(this) {

    if (data.name) {
        m_name = *data.name;
    } else {
        m_name = i.to_string();
    }

    if (data.parent) { m_parent = *data.parent; }

    if (data.method_list) m_attached_methods = *data.method_list;
    if (data.signal_list) m_attached_signals = *data.signal_list;
}
EntityDelegate::~EntityDelegate() = default;
void EntityDelegate::update(EntityUpdateData const& data) {
    if (data.method_list) m_attached_methods = *data.method_list;
    if (data.signal_list) m_attached_signals = *data.signal_list;

    this->on_update(data);
}
void                      EntityDelegate::on_update(EntityUpdateData const&) { }
AttachedMethodList const& EntityDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& EntityDelegate::attached_signals() const {
    return m_attached_signals;
}
noo::EntityID EntityDelegate::id() const {
    return m_id;
}

void EntityDelegate::prepare_delete() { }

// =============================================================================

PlotDelegate::PlotDelegate(noo::PlotID i, PlotData const& data)
    : m_id(i), m_attached_methods(this), m_attached_signals(this) {

    if (data.type) { m_type = *data.type; }
    if (data.table) { m_table = *data.table; }

    if (data.method_list) m_attached_methods = *data.method_list;
    if (data.signal_list) m_attached_signals = *data.signal_list;
}
PlotDelegate::~PlotDelegate() = default;
void PlotDelegate::update(PlotData const& data) {
    if (data.type) { m_type = *data.type; }
    if (data.table) { m_table = *data.table; }
    if (data.method_list) m_attached_methods = *data.method_list;
    if (data.signal_list) m_attached_signals = *data.signal_list;

    this->on_update(data);
}
void                      PlotDelegate::on_update(PlotData const&) { }
AttachedMethodList const& PlotDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& PlotDelegate::attached_signals() const {
    return m_attached_signals;
}
noo::PlotID PlotDelegate::id() const {
    return m_id;
}

void PlotDelegate::prepare_delete() { }

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

    TextureDelegate* get(noo::TextureID id) {
        if (!m_state) return {};

        return m_state->texture_list().comp_at(id);
    }
    BufferDelegate* get(noo::BufferID id) {
        if (!m_state) return {};

        return m_state->buffer_list().comp_at(id);
    }
    TableDelegate* get(noo::TableID id) {
        if (!m_state) return {};

        return m_state->table_list().comp_at(id);
    }
    LightDelegate* get(noo::LightID id) {
        if (!m_state) return {};

        return m_state->light_list().comp_at(id);
    }
    MaterialDelegate* get(noo::MaterialID id) {
        if (!m_state) return {};

        return m_state->material_list().comp_at(id);
    }
    MeshDelegate* get(noo::GeometryID id) {
        if (!m_state) return {};

        return m_state->mesh_list().comp_at(id);
    }
    EntityDelegate* get(noo::EntityID id) {
        if (!m_state) return {};

        return m_state->object_list().comp_at(id);
    }
    SignalDelegate* get(noo::SignalID id) {
        if (!m_state) return {};

        return m_state->signal_list().comp_at(id);
    }
    MethodDelegate* get(noo::MethodID id) {
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
            return std::make_unique<DEL>(id, d);                               \
        };                                                                     \
    }

    CREATE_DEFAULT(tex_maker, TextureID, TextureInit, TextureDelegate);
    CREATE_DEFAULT(buffer_maker, BufferID, BufferInit, BufferDelegate);
    CREATE_DEFAULT(table_maker, TableID, TableData, TableDelegate);
    CREATE_DEFAULT(light_maker, LightID, LightData, LightDelegate);
    CREATE_DEFAULT(mat_maker, MaterialID, MaterialInit, MaterialDelegate);
    CREATE_DEFAULT(mesh_maker, GeometryID, MeshData, MeshDelegate);
    CREATE_DEFAULT(object_maker, EntityID, EntityUpdateData, EntityDelegate);
    CREATE_DEFAULT(sig_maker, SignalID, SignalInit, SignalDelegate);
    CREATE_DEFAULT(method_maker, MethodID, MethodInit, MethodDelegate);


    if (!d.doc_maker) {
        d.doc_maker = []() { return std::make_unique<DocumentDelegate>(); };
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

TextureDelegate* ClientConnection::get(noo::TextureID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
BufferDelegate* ClientConnection::get(noo::BufferID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
TableDelegate* ClientConnection::get(noo::TableID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
LightDelegate* ClientConnection::get(noo::LightID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MaterialDelegate* ClientConnection::get(noo::MaterialID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MeshDelegate* ClientConnection::get(noo::GeometryID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
EntityDelegate* ClientConnection::get(noo::EntityID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
SignalDelegate* ClientConnection::get(noo::SignalID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MethodDelegate* ClientConnection::get(noo::MethodID id) {
    if (!m_data) return {};

    return m_data->get(id);
}

} // namespace nooc
