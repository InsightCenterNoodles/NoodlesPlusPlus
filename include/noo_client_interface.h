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

using DocumentDelegatePtr = std::unique_ptr<DocumentDelegate>;

using TextureDelegatePtr  = std::unique_ptr<TextureDelegate>;
using BufferDelegatePtr   = std::unique_ptr<BufferDelegate>;
using TableDelegatePtr    = std::unique_ptr<TableDelegate>;
using LightDelegatePtr    = std::unique_ptr<LightDelegate>;
using MaterialDelegatePtr = std::unique_ptr<MaterialDelegate>;
using MeshDelegatePtr     = std::unique_ptr<MeshDelegate>;
using ObjectDelegatePtr   = std::unique_ptr<ObjectDelegate>;
using SignalDelegatePtr   = std::unique_ptr<SignalDelegate>;
using MethodDelegatePtr   = std::unique_ptr<MethodDelegate>;

// Not to be stored by users!
using MethodContext =
    std::variant<std::monostate, ObjectDelegate*, TableDelegate*>;

using MethodContextPtr = std::
    variant<std::monostate, QPointer<ObjectDelegate>, QPointer<TableDelegate>>;

class MessageHandler;

///
/// The PendingMethodReply base class is a handle for you to call a noodles
/// method. Thus the 'pending' part. As part of the Qt system, this will issue a
/// signal when the method returns.
///
/// Only invoke this method once!
///
/// Will delete itself automatically after a reply or failure is reported. Use a
/// translator to interpret the returned type.
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

    /// \brief Call this method directly, ie, you have marshalled the arguments
    /// to the Any type manually.
    void call_direct(noo::AnyVarList&&);

    /// \brief Call this method, with automatic marshalling of arguments.
    template <class... Args>
    void call(Args&&... args) {
        call_direct(noo::marshall_to_any(std::forward<Args>(args)...));
    }

private slots:
    /// Internal use
    void complete(noo::AnyVarRef, QString);

signals:
    /// Issued when a non-error reply has been received
    void recv_data(noo::AnyVarRef);

    /// Issued when the method raised an exception on the server side.
    void recv_fail(QString);
};

///
/// Handy classes for translating a reply to a common non-noodles type.
///
namespace replies {

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

} // namespace replies


// Helper machinery to turn an any list into arguments required by a function.
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

/// Call a function with any vars, automatically translated to the parameter
/// type
template <class Func>
auto reply_call_helper(Func&& f, noo::AnyVar& source) {
    using FType = std::remove_cvref_t<Func>;

    return ReplyCallHelper<FType>::call(f, source);
}


// =============================================================================

/// Contains methods attached to an object/table/document.
class AttachedMethodList {
    MethodContext                m_context;
    std::vector<MethodDelegate*> m_list;

    MethodDelegate* find_direct_by_name(std::string_view) const;
    bool            check_direct_by_delegate(MethodDelegate*) const;

public:
    /// Internal use only
    AttachedMethodList(MethodContext);

    /// Internal use only
    AttachedMethodList& operator=(std::vector<MethodDelegate*> const& l) {
        m_list = l;
        return *this;
    }

    std::vector<MethodDelegate*> const& list() const { return m_list; }


    /// Find a method by name. This returns a reply object that is actually used
    /// to call the method. Returns null if the delegate is not attached to this
    /// object/table/document.
    template <class Reply = PendingMethodReply, class... Args>
    Reply* new_call_by_name(std::string_view v, Args&&... args) const {
        static_assert(std::is_base_of_v<PendingMethodReply, Reply>);

        auto mptr = find_direct_by_name(v);

        if (!mptr) return nullptr;

        auto* reply_ptr =
            new Reply(mptr, m_context, std::forward<Args>(args)...);

        reply_ptr->setParent(mptr);

        return reply_ptr;
    }


    /// Find a method by delegate. Returns a reply object to actually make the
    /// call. Returns null if the delegate is not attached to this
    /// object/table/document.
    template <class Reply = PendingMethodReply, class... Args>
    Reply* new_call_by_delegate(MethodDelegate* p, Args&&... args) const {
        if (!p) return nullptr;
        static_assert(std::is_base_of_v<PendingMethodReply, Reply>);

        if (!check_direct_by_delegate(p)) { return nullptr; }

        auto* reply_ptr = new Reply(p, m_context, std::forward<Args>(args)...);

        reply_ptr->setParent(p);

        return reply_ptr;
    }
};

/// Represents a signal that is attached to an object/table/document. Allows a
/// user to connect a Qt signal.
class AttachedSignal : public QObject {
    Q_OBJECT
    QPointer<SignalDelegate> m_signal;

public:
    AttachedSignal(SignalDelegate*);

    SignalDelegate* delegate() const;

signals:
    void fired(noo::AnyVarListRef const&);
};

/// Contains signals attached to an object/table/document.
class AttachedSignalList {
    MethodContext                                m_context;
    std::vector<std::unique_ptr<AttachedSignal>> m_list;

public:
    // Internal use
    AttachedSignalList(MethodContext);

    // Internal use
    AttachedSignalList& operator=(std::vector<SignalDelegate*> const& l);

    /// Find the attached signal by a name. Returns null if name is not found.
    AttachedSignal* find_by_name(std::string_view) const;

    /// Find the attached signal by a delegate ptr. Returns null i the name is
    /// not found.
    AttachedSignal* find_by_delegate(SignalDelegate*) const;
};

#define NOODLES_CAN_UPDATE(B) static constexpr bool CAN_UPDATE = B;

/// Represents an argument name/documentation pair for a method/signal.
struct ArgDoc {
    std::string_view name;
    std::string_view doc;
};

/// Describes a noodles method
struct MethodData {
    std::string_view  method_name;
    std::string_view  documentation;
    std::string_view  return_documentation;
    std::span<ArgDoc> argument_documentation;
};

/// The base delegate class for methods. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new methods from the
/// server.
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

signals:
    // private
    void invoke(noo::MethodID,
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

/// The base delegate class for signals. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new signals from the
/// server.
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

signals:
    /// Issued when this signal has been recv'ed.
    void fired(MethodContext, noo::AnyVarListRef const&);
};

// =============================================================================

struct BufferData {
    std::span<std::byte const> data;
    QUrl                       url;
    size_t                     url_size;
};

/// The base delegate class for buffers. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new buffers from the
/// server.
class BufferDelegate : public QObject {
    Q_OBJECT
    noo::BufferID m_id;

public:
    BufferDelegate(noo::BufferID, BufferData const&);
    virtual ~BufferDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::BufferID id() const;
};

// =============================================================================

struct TextureData {
    std::optional<BufferDelegate*> buffer;
    std::optional<size_t>          start;
    std::optional<size_t>          size;
};


/// The base delegate class for textures. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new textures from the
/// server. Texture delegates can also be updated.
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

signals:
    void updated();
};

// =============================================================================

struct MaterialData {
    std::optional<glm::vec4>        color;
    std::optional<float>            metallic;
    std::optional<float>            roughness;
    std::optional<bool>             use_blending;
    std::optional<TextureDelegate*> texture;
};

/// The base delegate class for materials. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new materials from
/// the server. Material delegates can also be updated.
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

signals:
    void updated();
};

// =============================================================================

struct LightData {
    glm::vec3 color     = { 1, 1, 1 };
    float     intensity = 0;
};

/// The base light class for buffers. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new lights from the
/// server. Light delegates can also be updated.
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

signals:
    void updated();
};

// =============================================================================

struct ComponentRef {
    BufferDelegate* buffer;
    size_t          start  = 0;
    size_t          size   = 0;
    size_t          stride = 0;
};

/// The base delegate class for meshes. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new meshes from the
/// server. Mesh delegates can also be updated.
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
    std::optional<std::string_view>              name;
    std::optional<ObjectDelegate*>               parent;
    std::optional<glm::mat4>                     transform;
    std::optional<MaterialDelegate*>             material;
    std::optional<MeshDelegate*>                 mesh;
    std::optional<std::vector<LightDelegate*>>   lights;
    std::optional<std::vector<TableDelegate*>>   tables;
    std::optional<std::span<glm::mat4 const>>    instances;
    std::optional<std::vector<std::string_view>> tags;
    std::optional<std::vector<MethodDelegate*>>  method_list;
    std::optional<std::vector<SignalDelegate*>>  signal_list;
    std::optional<ObjectText>                    text;
};


/// The base delegate class for objects. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new objects from the
/// server. Object delegates can also be updated.
class ObjectDelegate : public QObject {
    Q_OBJECT
    noo::ObjectID            m_id;
    std::string              m_name;
    QPointer<ObjectDelegate> m_parent;

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
    std::optional<std::string_view>             name;
    std::optional<std::vector<MethodDelegate*>> method_list;
    std::optional<std::vector<SignalDelegate*>> signal_list;
};

/// The base delegate class for tables. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new tables from the
/// server. Table delegates can also be updated.
class TableDelegate : public QObject {
    Q_OBJECT
    noo::TableID       m_id;
    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

    std::vector<QMetaObject::Connection> m_spec_signals;

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

public slots:
    virtual void on_table_initialize(noo::AnyVarListRef const& names,
                                     noo::AnyVarRef            keys,
                                     noo::AnyVarListRef const& data_cols,
                                     noo::AnyVarListRef const& selections);

    virtual void on_table_reset();
    virtual void on_table_updated(noo::AnyVarRef keys, noo::AnyVarRef columns);
    virtual void on_table_rows_removed(noo::AnyVarRef keys);
    virtual void on_table_selection_updated(std::string_view,
                                            noo::SelectionRef const&);

public:
    PendingMethodReply* subscribe() const;
    PendingMethodReply* request_row_insert(noo::AnyVarList&& row) const;
    PendingMethodReply* request_rows_insert(noo::AnyVarList&& columns) const;

    PendingMethodReply* request_row_update(int64_t           key,
                                           noo::AnyVarList&& row) const;
    PendingMethodReply* request_rows_update(std::vector<int64_t>&& keys,
                                            noo::AnyVarList&& columns) const;

    PendingMethodReply* request_deletion(std::span<int64_t> keys) const;

    PendingMethodReply* request_clear() const;
    PendingMethodReply* request_selection_update(std::string_view,
                                                 noo::Selection) const;

private slots:
    void interp_table_reset(noo::AnyVarListRef const&);
    void interp_table_update(noo::AnyVarListRef const&);
    void interp_table_remove(noo::AnyVarListRef const&);
    void interp_table_sel_update(noo::AnyVarListRef const&);

signals:
    void updated();
};


// =============================================================================

struct DocumentData {
    std::vector<MethodDelegate*> method_list;
    std::vector<SignalDelegate*> signal_list;
};


/// The base delegate class for the document. Users can inherit from this to add
/// their own functionality. This delegate is instantiated on connection. The
/// document delegate can also be updated.
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

///
/// The ClientDelegates struct allows users to specify the delegates they wish
/// to use. Each maker function below is called when a new delegate of that type
/// is needed. If the function object is empty, the default delegate will be
/// used instead.
///
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

    std::function<std::unique_ptr<DocumentDelegate>()> doc_maker;
};

class ClientCore;

///
/// The core client object. Create one of these to connect to a noodles server.
///
/// Delegates here should never be deleted manually; lifetimes are managed by
/// this library instead!
///
class ClientConnection : public QObject {
    Q_OBJECT
    std::unique_ptr<ClientCore> m_data;

public:
    ClientConnection(QObject* parent = nullptr);
    ~ClientConnection();

    /// Open a new connection to a server, with the supplied clients.
    void open(QUrl server, ClientDelegates&&);

    // translate given IDs to the noodles object.
    TextureDelegate*  get(noo::TextureID);
    BufferDelegate*   get(noo::BufferID);
    TableDelegate*    get(noo::TableID);
    LightDelegate*    get(noo::LightID);
    MaterialDelegate* get(noo::MaterialID);
    MeshDelegate*     get(noo::MeshID);
    ObjectDelegate*   get(noo::ObjectID);
    SignalDelegate*   get(noo::SignalID);
    MethodDelegate*   get(noo::MethodID);

signals:
    /// Issued when something goes wrong with the websocket, with a string about
    /// the error.
    void socket_error(QString);

    /// Issued when the connection is open and ready.
    void connected();

    /// Issued when we (or the server) close the connection.
    void disconnected();
};

} // namespace nooc

#endif // CLIENT_INTERFACE_H
