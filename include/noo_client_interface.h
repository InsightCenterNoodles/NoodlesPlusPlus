#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H

#include "noo_anyref.h"
#include "noo_include_glm.h"
#include "noo_interface_types.h"

#include <span>

#include <QObject>
#include <QPointer>
#include <QUrl>

namespace nooc {

class DocumentDelegate;

class TextureDelegate;
class BufferDelegate;
class TableDelegate;
class LightDelegate;
class MaterialDelegate;
class MeshDelegate;
class ObjectDelegate;
class SignalDelegate;
class MethodDelegate;

using DocumentDelegatePtr = std::shared_ptr<DocumentDelegate>;

using TextureDelegatePtr  = std::shared_ptr<TextureDelegate>;
using BufferDelegatePtr   = std::shared_ptr<BufferDelegate>;
using TableDelegatePtr    = std::shared_ptr<TableDelegate>;
using LightDelegatePtr    = std::shared_ptr<LightDelegate>;
using MaterialDelegatePtr = std::shared_ptr<MaterialDelegate>;
using MeshDelegatePtr     = std::shared_ptr<MeshDelegate>;
using ObjectDelegatePtr   = std::shared_ptr<ObjectDelegate>;
using SignalDelegatePtr   = std::shared_ptr<SignalDelegate>;
using MethodDelegatePtr   = std::shared_ptr<MethodDelegate>;

// Not to be stored by users!
using MethodContext =
    std::variant<std::monostate, ObjectDelegate*, TableDelegate*>;

using MethodContextPtr = std::
    variant<std::monostate, QPointer<ObjectDelegate>, QPointer<TableDelegate>>;

class MessageHandler;

///
/// \brief The PendingMethodReply class
///
/// Will delete itself automatically. Use a translator to change the type. This
/// is a bit more overhead, but is more safe if things are deleted out from
/// under your feet!
///
class PendingMethodReply : public QObject {
    Q_OBJECT
    friend class MessageHandler;
    friend class AttachedMethodList;

    bool                     m_called = false;
    QPointer<MethodDelegate> m_method;
    MethodContextPtr         m_context;


protected:
    noo::AnyVarRef m_var;
    virtual void   interpret();

public:
    PendingMethodReply(MethodDelegate*, MethodContext);

    void call_direct(noo::AnyVarList&&);

    template <class... Args>
    void call(Args&&... args) {
        call_direct(noo::marshall_to_any(std::forward<Args>(args)...));
    }

private slots:
    void complete(noo::AnyVarRef, QString);

signals:
    void recv_data(noo::AnyVarRef);
    void recv_fail(QString);
};

namespace translators {

class GetIntegerReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;

signals:
    void recv(int64_t);
};

class GetStringReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;
signals:
    void recv(QString);
};

class GetStringListReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;

signals:
    void recv(QStringList);
};

class GetRealsReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;
signals:
    void recv(noo::PossiblyOwnedView<double const> const&);
};


// special translators


class GetSelectionReply : public PendingMethodReply {
    Q_OBJECT

public:
    GetSelectionReply(MethodDelegate* d, MethodContext c, QString _selection_id)
        : PendingMethodReply(d, c), selection_id(_selection_id) { }

    QString selection_id;

    void interpret() override;

signals:
    void recv(QString, noo::SelectionRef const&);
};

class GetTableRowReply : public GetRealsReply {
public:
    GetTableRowReply(MethodDelegate*    d,
                     MethodContext      c,
                     int64_t            _row,
                     std::span<int64_t> _cols)
        : GetRealsReply(d, c), row(_row), cols(_cols.begin(), _cols.end()) { }

    int64_t              row;
    std::vector<int64_t> cols;
};

class GetTableBlockReply : public GetRealsReply {
public:
    GetTableBlockReply(MethodDelegate*    d,
                       MethodContext      c,
                       int64_t            _row_from,
                       int64_t            _row_to,
                       std::span<int64_t> _cols)
        : GetRealsReply(d, c),
          row_from(_row_from),
          row_to(_row_to),
          cols(_cols.begin(), _cols.end()) { }

    int64_t              row_from;
    int64_t              row_to;
    std::vector<int64_t> cols;
};

class GetTableSelectionReply : public GetRealsReply {
public:
    GetTableSelectionReply(MethodDelegate* d,
                           MethodContext   c,
                           QString         _selection_id)
        : GetRealsReply(d, c), selection_id(_selection_id) { }

    QString selection_id;
};


} // namespace translators


template <class T>
T _any_call_getter(noo::AnyVar& source) {
    if constexpr (!noo::is_in_variant<T, noo::AnyVarBase>::value) {

        if constexpr (noo::is_vector<T>::value) {
            // extract a vector
            auto nv = source.steal_vector();

            T ret;

            for (auto& local_a : nv) {
                ret.emplace_back(std::move(local_a));
            }

            return ret;

        } else {
            return T(std::move(source));
        }

    } else {
        T* c = std::get_if<T>(&source);
        if (c) return std::move(*c);
        return T();
    }
}

template <class Lambda>
struct ReplyCallHelper : ReplyCallHelper<decltype(&Lambda::operator())> { };

template <class R, class Arg>
struct ReplyCallHelper<R(Arg)> {
    template <class Func>
    static auto call(Func&& f, noo::AnyVar& source) {
        return f(std::move(_any_call_getter<Arg>(source)));
    }
};

template <class R, class Arg>
struct ReplyCallHelper<R (*)(Arg)> {
    template <class Func>
    static auto call(Func&& f, noo::AnyVar& source) {
        return f(std::move(_any_call_getter<Arg>(source)));
    }
};

template <class R, class C, class Arg>
struct ReplyCallHelper<R (C::*)(Arg)> {
    template <class Func>
    static auto call(Func&& f, noo::AnyVar& source) {
        return f(std::move(_any_call_getter<Arg>(source)));
    }
};

template <class R, class C, class Arg>
struct ReplyCallHelper<R (C::*)(Arg) const> {
    template <class Func>
    static auto call(Func&& f, noo::AnyVar& source) {
        return f(std::move(_any_call_getter<Arg>(source)));
    }
};

template <class Func>
auto reply_call_helper(Func&& f, noo::AnyVar& source) {
    using FType = std::remove_cvref_t<Func>;

    return ReplyCallHelper<FType>::call(f, source);
}


// =============================================================================

class AttachedMethodList {
    MethodContext                  m_context;
    std::vector<MethodDelegatePtr> m_list;

    std::shared_ptr<MethodDelegate> find_direct_by_name(std::string_view) const;
    bool check_direct_by_delegate(MethodDelegatePtr const&) const;

public:
    AttachedMethodList(MethodContext);

    AttachedMethodList& operator=(std::vector<MethodDelegatePtr> const& l) {
        m_list = l;
        return *this;
    }

    std::vector<MethodDelegatePtr> const& list() const { return m_list; }


    template <class Reply = PendingMethodReply, class... Args>
    Reply* new_call_by_name(std::string_view v, Args&&... args) const {
        static_assert(std::is_base_of_v<PendingMethodReply, Reply>);

        auto mptr = find_direct_by_name(v);

        if (!mptr) return nullptr;

        auto* reply_ptr =
            new Reply(mptr.get(), m_context, std::forward<Args>(args)...);

        reply_ptr->setParent(mptr.get());

        return reply_ptr;
    }

    template <class Reply = PendingMethodReply, class... Args>
    Reply* new_call_by_delegate(MethodDelegatePtr const& p,
                                Args&&... args) const {
        static_assert(std::is_base_of_v<PendingMethodReply, Reply>);

        if (!check_direct_by_delegate(p)) { return nullptr; }

        auto* reply_ptr =
            new Reply(p.get(), m_context, std::forward<Args>(args)...);

        reply_ptr->setParent(p.get());

        return reply_ptr;
    }
};

class AttachedSignal : public QObject {
    Q_OBJECT
    SignalDelegatePtr m_signal;

public:
    AttachedSignal(SignalDelegatePtr);

    SignalDelegatePtr const& delegate() const;

signals:
    void fired(noo::AnyVar const&);
};

class AttachedSignalList {
    MethodContext                                m_context;
    std::vector<std::unique_ptr<AttachedSignal>> m_list;

public:
    AttachedSignalList(MethodContext);

    AttachedSignalList& operator=(std::vector<SignalDelegatePtr> const& l) {
        m_list.clear();
        for (auto const& p : l) {
            m_list.emplace_back(std::make_unique<AttachedSignal>(p));
        }
        return *this;
    }

    AttachedSignal* find_by_name(std::string_view) const;
    AttachedSignal* find_by_delegate(SignalDelegatePtr const&) const;
};

#define NOODLES_CAN_UPDATE(B) static constexpr bool CAN_UPDATE = B;

struct ArgDoc {
    std::string_view name;
    std::string_view doc;
};

struct MethodData {
    std::string_view  method_name;
    std::string_view  documentation;
    std::string_view  return_documentation;
    std::span<ArgDoc> argument_documentation;
};

class MethodDelegate : public QObject {
    Q_OBJECT
    noo::MethodID m_id;
    std::string   m_method_name;


public:
    MethodDelegate(noo::MethodID, MethodData const&);
    virtual ~MethodDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::MethodID    id() const;
    std::string_view name() const;

    virtual void prepare_delete();

signals:
    // private
    void invoke(noo::MethodID, // this is weird, but its a signal.
                MethodContext,
                noo::AnyVarList const&,
                PendingMethodReply*);
};

// =============================================================================

struct SignalData {
    std::string_view  signal_name;
    std::string_view  documentation;
    std::span<ArgDoc> argument_documentation;
};

class SignalDelegate : public QObject {
    Q_OBJECT
    noo::SignalID m_id;
    std::string   m_signal_name;

public:
    SignalDelegate(noo::SignalID, SignalData const&);
    virtual ~SignalDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::SignalID    id() const;
    std::string_view name() const;

    virtual void prepare_delete();

signals:
    void fired(MethodContext, noo::AnyVarList);
};

// =============================================================================

struct BufferData {
    std::span<std::byte const> data;
    QUrl                       url;
    size_t                     url_size;
};

class BufferDelegate : public QObject {
    Q_OBJECT
    noo::BufferID m_id;

public:
    BufferDelegate(noo::BufferID, BufferData const&);
    virtual ~BufferDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::BufferID id() const;

    virtual void prepare_delete();
};

// =============================================================================

struct TextureData {
    BufferDelegatePtr buffer = nullptr;
    size_t            start  = 0;
    size_t            size   = 0;
};

class TextureDelegate : public QObject {
    Q_OBJECT
    noo::TextureID m_id;

public:
    TextureDelegate(noo::TextureID, TextureData const&);
    virtual ~TextureDelegate();

    NOODLES_CAN_UPDATE(true)
    noo::TextureID id() const;


    void         update(TextureData const&);
    virtual void on_update(TextureData const&);

    virtual void prepare_delete();

signals:
    void updated();
};

// =============================================================================

struct MaterialData {
    glm::vec4          color        = { 1, 1, 1, 1 };
    float              metallic     = 0;
    float              roughness    = 1;
    bool               use_blending = false;
    TextureDelegatePtr texture      = nullptr;
};

class MaterialDelegate : public QObject {
    Q_OBJECT
    noo::MaterialID m_id;

public:
    MaterialDelegate(noo::MaterialID, MaterialData const&);
    virtual ~MaterialDelegate();

    NOODLES_CAN_UPDATE(true)

    noo::MaterialID id() const;

    void         update(MaterialData const&);
    virtual void on_update(MaterialData const&);

    virtual void prepare_delete();

signals:
    void updated();
};

// =============================================================================

struct LightData {
    glm::vec3 color     = { 1, 1, 1 };
    float     intensity = 0;
};

class LightDelegate : public QObject {
    Q_OBJECT
    noo::LightID m_id;

public:
    LightDelegate(noo::LightID, LightData const&);
    virtual ~LightDelegate();

    NOODLES_CAN_UPDATE(true)

    noo::LightID id() const;

    void         update(LightData const&);
    virtual void on_update(LightData const&);

    virtual void prepare_delete();

signals:
    void updated();
};

// =============================================================================

struct ComponentRef {
    BufferDelegatePtr buffer;
    size_t            start  = 0;
    size_t            size   = 0;
    size_t            stride = 0;
};

struct MeshData {
    glm::vec3 extent_min;
    glm::vec3 extent_max;

    std::optional<ComponentRef> positions;
    std::optional<ComponentRef> normals;
    std::optional<ComponentRef> textures;
    std::optional<ComponentRef> colors;
    std::optional<ComponentRef> lines;
    std::optional<ComponentRef> triangles;
};

class MeshDelegate : public QObject {
    Q_OBJECT
    noo::MeshID m_id;

public:
    MeshDelegate(noo::MeshID, MeshData const&);
    virtual ~MeshDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::MeshID id() const;

    virtual void prepare_delete();
};

// =============================================================================

struct ObjectText {
    std::string text;
    std::string font;
    float       height;
    float       width = -1;
};

struct ObjectUpdateData {
    std::optional<std::string_view>               name;
    std::optional<ObjectDelegatePtr>              parent;
    std::optional<glm::mat4>                      transform;
    std::optional<MaterialDelegatePtr>            material;
    std::optional<MeshDelegatePtr>                mesh;
    std::optional<std::vector<LightDelegatePtr>>  lights;
    std::optional<std::vector<TableDelegatePtr>>  tables;
    std::optional<std::span<glm::mat4 const>>     instances;
    std::optional<std::vector<std::string_view>>  tags;
    std::optional<std::vector<MethodDelegatePtr>> method_list;
    std::optional<std::vector<SignalDelegatePtr>> signal_list;
    std::optional<ObjectText>                     text;
};

class ObjectDelegate : public QObject {
    Q_OBJECT
    noo::ObjectID     m_id;
    std::string       m_name;
    ObjectDelegatePtr m_parent;

    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

public:
    ObjectDelegate(noo::ObjectID, ObjectUpdateData const&);
    virtual ~ObjectDelegate();

    NOODLES_CAN_UPDATE(true)

    noo::ObjectID id() const;

    void update(ObjectUpdateData const&);

    virtual void on_update(ObjectUpdateData const&);

    virtual void prepare_delete();

    AttachedMethodList const& attached_methods() const;
    AttachedSignalList const& attached_signals() const;

signals:
    void updated();
};

// =============================================================================

struct TableData {
    std::optional<std::string_view>               name;
    std::optional<std::vector<MethodDelegatePtr>> method_list;
    std::optional<std::vector<SignalDelegatePtr>> signal_list;
};

class TableDelegate : public QObject {
    Q_OBJECT
    noo::TableID       m_id;
    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

public:
    TableDelegate(noo::TableID, TableData const&);
    virtual ~TableDelegate();

    NOODLES_CAN_UPDATE(true)

    noo::TableID id() const;

    void update(TableData const&);

    virtual void on_update(TableData const&);

    virtual void prepare_delete();

    AttachedMethodList const& attached_methods() const;
    AttachedSignalList const& attached_signals() const;

public:
    void subscribe() const;
    void get_columns() const;
    void get_num_rows() const;


    translators::GetTableRowReply* get_row(int64_t            row,
                                           std::span<int64_t> columns) const;

    translators::GetTableBlockReply*
    get_block(int64_t            row_from,
              int64_t            row_to,
              std::span<int64_t> columns) const;

    translators::GetTableSelectionReply*
    get_selection_data(QString selection_id) const;


    void request_row_insert(int64_t row, std::span<double>) const;
    void request_row_update(int64_t row, std::span<double>) const;
    void request_row_append(std::span<double>) const;
    void request_deletion(noo::SelectionRef const&) const;

    void request_selection(QString) const;
    void request_set_selection(QString, noo::SelectionRef const&) const;
    void request_all_selections() const;

signals:
    void updated();

    void on_get_columns(QStringList);
    void on_get_num_rows(int);

    void on_request_selection(QString, noo::SelectionRef const&) const;
    void on_request_all_selections() const;
};


// =============================================================================

struct DocumentData {
    std::vector<MethodDelegatePtr> method_list;
    std::vector<SignalDelegatePtr> signal_list;
};


class DocumentDelegate : public QObject {
    Q_OBJECT
    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

public:
    DocumentDelegate();
    virtual ~DocumentDelegate();

    virtual void on_update(DocumentData const&);
    virtual void on_clear();

    void update(DocumentData const&);
    void clear();

    AttachedMethodList const& attached_methods() const;
    AttachedSignalList const& attached_signals() const;


signals:
    void updated();
};


// =============================================================================


struct ClientDelegates {
    QString client_name;

    std::function<TextureDelegatePtr(noo::TextureID, TextureData const&)>
        tex_maker;
    std::function<BufferDelegatePtr(noo::BufferID, BufferData const&)>
        buffer_maker;
    std::function<TableDelegatePtr(noo::TableID, TableData const&)> table_maker;
    std::function<LightDelegatePtr(noo::LightID, LightData const&)> light_maker;
    std::function<MaterialDelegatePtr(noo::MaterialID, MaterialData const&)>
                                                                 mat_maker;
    std::function<MeshDelegatePtr(noo::MeshID, MeshData const&)> mesh_maker;
    std::function<ObjectDelegatePtr(noo::ObjectID, ObjectUpdateData const&)>
        object_maker;
    std::function<SignalDelegatePtr(noo::SignalID, SignalData const&)>
        sig_maker;
    std::function<MethodDelegatePtr(noo::MethodID, MethodData const&)>
        method_maker;

    std::function<std::shared_ptr<DocumentDelegate>()> doc_maker;
};

class ClientCore;
class ClientConnection : public QObject {
    Q_OBJECT
    std::unique_ptr<ClientCore> m_data;

public:
    ClientConnection(QObject* parent = nullptr);
    ~ClientConnection();

    void open(QUrl server, ClientDelegates&&);

    TextureDelegatePtr  get(noo::TextureID);
    BufferDelegatePtr   get(noo::BufferID);
    TableDelegatePtr    get(noo::TableID);
    LightDelegatePtr    get(noo::LightID);
    MaterialDelegatePtr get(noo::MaterialID);
    MeshDelegatePtr     get(noo::MeshID);
    ObjectDelegatePtr   get(noo::ObjectID);
    SignalDelegatePtr   get(noo::SignalID);
    MethodDelegatePtr   get(noo::MethodID);

signals:
    void socket_error(QString);

    void connected();
    void disconnected();
};

} // namespace nooc

#endif // CLIENT_INTERFACE_H
