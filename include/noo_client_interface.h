#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H

#include "noo_include_glm.h"
#include "noo_interface_types.h"

#include <span>

#include <QCborValue>
#include <QImage>
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
class EntityDelegate;
class SignalDelegate;
class MethodDelegate;
class PlotDelegate;
class ImageDelegate;
class SamplerDelegate;

using DocumentDelegatePtr = std::unique_ptr<DocumentDelegate>;

using TextureDelegatePtr  = std::unique_ptr<TextureDelegate>;
using BufferDelegatePtr   = std::unique_ptr<BufferDelegate>;
using TableDelegatePtr    = std::unique_ptr<TableDelegate>;
using LightDelegatePtr    = std::unique_ptr<LightDelegate>;
using MaterialDelegatePtr = std::unique_ptr<MaterialDelegate>;
using MeshDelegatePtr     = std::unique_ptr<MeshDelegate>;
using EntityDelegatePtr   = std::unique_ptr<EntityDelegate>;
using SignalDelegatePtr   = std::unique_ptr<SignalDelegate>;
using MethodDelegatePtr   = std::unique_ptr<MethodDelegate>;
using PlotDelegatePtr     = std::unique_ptr<PlotDelegate>;
using ImageDelegatePtr    = std::unique_ptr<ImageDelegate>;
using SamplerDelegatePtr  = std::unique_ptr<SamplerDelegate>;


using MethodContextPtr = std::variant<std::monostate,
                                      QPointer<EntityDelegate>,
                                      QPointer<TableDelegate>,
                                      QPointer<PlotDelegate>>;

class MessageHandler;

/// Represents exception information as provided by the server
struct MethodException {
    int        code;
    QString    message;
    QCborValue additional;
};

QString to_string(MethodException const&);

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
    QCborValue   m_var;
    virtual void interpret();

public:
    PendingMethodReply(MethodDelegate*, MethodContextPtr);

    /// \brief Call this method directly, ie, you have marshalled the arguments
    /// to the Any type manually.
    void call_direct(QCborArray);

    /// \brief Call this method, with automatic marshalling of arguments.
    template <class... Args>
    void call(Args&&... args) {
        QCborArray arr;
        (arr.push_back(convert_to_cbor(std::forward<Args>(args))), ...);
        call_direct(arr);
    }

private slots:
    /// Internal use
    void complete(QCborValue, MethodException*);

signals:
    /// Issued when a non-error reply has been received
    void recv_data(QCborValue);

    /// Issued when the method raised an exception on the server side.
    void recv_fail(QString);

    /// Issued when the method raised an exception on the server side.
    void recv_exception(MethodException);
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
    void recv(QVector<double> const&);
};

} // namespace replies


// Helper machinery to turn an any list into arguments required by a function.
template <class T>
T _any_call_getter(QCborValue& source) {
    T ret;
    noo::convert_from_cbor(source, ret);
    return ret;
}

template <class Lambda>
struct ReplyCallHelper : ReplyCallHelper<decltype(&Lambda::operator())> { };

template <class R, class Arg>
struct ReplyCallHelper<R(Arg)> {
    template <class Func>
    static auto call(Func&& f, QCborValue& source) {
        return f(std::move(_any_call_getter<Arg>(source)));
    }
};

template <class R, class Arg>
struct ReplyCallHelper<R (*)(Arg)> {
    template <class Func>
    static auto call(Func&& f, QCborValue& source) {
        return f(std::move(_any_call_getter<Arg>(source)));
    }
};

template <class R, class C, class Arg>
struct ReplyCallHelper<R (C::*)(Arg)> {
    template <class Func>
    static auto call(Func&& f, QCborValue& source) {
        return f(std::move(_any_call_getter<Arg>(source)));
    }
};

template <class R, class C, class Arg>
struct ReplyCallHelper<R (C::*)(Arg) const> {
    template <class Func>
    static auto call(Func&& f, QCborValue& source) {
        return f(std::move(_any_call_getter<Arg>(source)));
    }
};

/// Call a function with any vars, automatically translated to the parameter
/// type
template <class Func>
auto reply_call_helper(Func&& f, QCborValue& source) {
    using FType = std::remove_cvref_t<Func>;

    return ReplyCallHelper<FType>::call(f, source);
}


// =============================================================================

/// Contains methods attached to an object/table/document.
class AttachedMethodList {
    MethodContextPtr             m_context;
    std::vector<MethodDelegate*> m_list;

    MethodDelegate* find_direct_by_name(std::string_view) const;
    bool            check_direct_by_delegate(MethodDelegate*) const;

public:
    /// Internal use only
    AttachedMethodList(MethodContextPtr);

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
    void fired(QCborArray const&);
};

/// Contains signals attached to an object/table/document.
class AttachedSignalList {
    MethodContextPtr                             m_context;
    std::vector<std::unique_ptr<AttachedSignal>> m_list;

public:
    // Internal use
    AttachedSignalList(MethodContextPtr);

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
    QString name;
    QString doc;
    QString editor_hint;
};

/// Describes a noodles method
struct MethodInit {
    QString         method_name;
    QString         documentation;
    QString         return_documentation;
    QVector<ArgDoc> argument_documentation;

    MethodInit(QCborValue);
};

/// The base delegate class for methods. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new methods from the
/// server.
class MethodDelegate : public QObject {
    Q_OBJECT
    noo::MethodID m_id;
    QString       m_method_name;

public:
    MethodDelegate(noo::MethodID, MethodInit const&);
    virtual ~MethodDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::MethodID id() const;
    QString       name() const;

signals:
    // private
    void invoke(noo::MethodID,
                MethodContextPtr,
                QCborArray const&,
                PendingMethodReply*);
};

// =============================================================================

struct SignalInit {
    QString           name;
    std::string_view  documentation;
    std::span<ArgDoc> argument_documentation;
};

/// The base delegate class for signals. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new signals from the
/// server.
class SignalDelegate : public QObject {
    Q_OBJECT
    noo::SignalID m_id;
    SignalInit    m_data;

public:
    SignalDelegate(noo::SignalID, SignalInit const&);
    virtual ~SignalDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::SignalID id() const;
    QString       name() const;

signals:
    /// Issued when this signal has been recv'ed.
    void fired(MethodContextPtr, QCborArray const&);
};

// =============================================================================

struct BufferInit {
    QString name;
    QUrl    url;
    size_t  url_size;
};

/// The base delegate class for buffers. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new buffers from the
/// server.
class BufferDelegate : public QObject {
    Q_OBJECT
    noo::BufferID m_id;
    BufferInit    m_data;

    QByteArray m_buffer_bytes;

public:
    BufferDelegate(noo::BufferID, BufferInit const&);
    virtual ~BufferDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::BufferID id() const;

private slots:
    void store_data(QByteArray);

signals:
    void data_ready(QByteArray);
};

// =============================================================================

struct BufferViewInit {
    enum ViewType : uint8_t {
        UNKNOWN,
        GEOMETRY_INFO,
        IMAGE_INFO,
    };

    QString name;

    QPointer<BufferDelegate> source_buffer;
    ViewType                 type;
    uint64_t                 offset;
    uint64_t                 length;
};

/// The base delegate class for buffers. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new buffers from the
/// server.
class BufferViewDelegate : public QObject {
    Q_OBJECT
    noo::BufferViewID m_id;
    BufferViewInit    m_init;

public:
    BufferViewDelegate(noo::BufferViewID, BufferViewInit const&);
    virtual ~BufferViewDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::BufferID id() const;

signals:
    void data_ready(QByteArray);
};

// =============================================================================

struct ImageInit {
    QString name;

    QPointer<BufferViewDelegate> local_image;
    QUrl                         remote_image;
};

struct Image : public QObject {
    Q_OBJECT
    noo::ImageID m_id;
    ImageInit    m_init;

    QImage m_image;

public:
    Image(noo::ImageID, ImageInit const&);
    virtual ~Image();

    NOODLES_CAN_UPDATE(false)

    noo::ImageID id() const;

private slots:
    void store_data(QByteArray);

signals:
    void data_ready(QImage);
};

// =============================================================================

enum class MagFilter : uint8_t {
    NEAREST,
    LINEAR,
};

enum class MinFilter : uint8_t {
    NEAREST,
    LINEAR,
    NEAREST_MIPMAP_NEAREST,
    LINEAR_MIPMAP_NEAREST,
    NEAREST_MIPMAP_LINEAR,
    LINEAR_MIPMAP_LINEAR,
};

enum class SamplerMode : uint8_t {
    CLAMP_TO_EDGE,
    MIRRORED_REPEAT,
    REPEAT,
};

struct SamplerInit {
    QString name;

    MagFilter mag_filter = MagFilter::LINEAR;
    MinFilter min_filter = MinFilter::LINEAR_MIPMAP_LINEAR;

    SamplerMode wrap_s = SamplerMode::REPEAT;
    SamplerMode wrap_t = SamplerMode::REPEAT;
};

struct Sampler : public QObject {
    Q_OBJECT
    noo::SamplerID m_id;
    SamplerInit    m_init;

public:
    Sampler(noo::SamplerID, SamplerInit const&);
    virtual ~Sampler();

    NOODLES_CAN_UPDATE(false)

    noo::SamplerID id() const;
};

// =============================================================================

struct TextureInit {
    QString                   name;
    QPointer<ImageDelegate>   image;
    QPointer<SamplerDelegate> sampler;
};


/// The base delegate class for textures. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new textures from the
/// server. Texture delegates can also be updated.
class TextureDelegate : public QObject {
    Q_OBJECT
    noo::TextureID m_id;
    TextureInit    m_init;

public:
    TextureDelegate(noo::TextureID, TextureInit const&);
    virtual ~TextureDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::TextureID id() const;
};

// =============================================================================

struct TextureRef {
    QPointer<TextureDelegate> texture;
    glm::mat3                 transform          = glm::mat3(1);
    uint8_t                   texture_coord_slot = 0;
};

struct PBRInfo {
    QColor                    base_color;
    std::optional<TextureRef> base_color_texture;

    float                     metallic  = 1.0;
    float                     roughness = 1.0;
    std::optional<TextureRef> metal_rough_texture;
};

struct MaterialInit {
    std::optional<QString> name;

    std::optional<PBRInfo>    pbr_info;
    std::optional<TextureRef> normal_texture;

    std::optional<TextureRef> occlusion_texture;
    std::optional<float>      occlusion_texture_factor;

    std::optional<TextureRef> emissive_texture;
    std::optional<glm::vec3>  emissive_factor;

    std::optional<bool>  use_alpha;
    std::optional<float> alpha_cutoff;
    std::optional<bool>  double_sided;
};

/// The base delegate class for materials. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new materials from
/// the server. Material delegates can also be updated.
class MaterialDelegate : public QObject {
    Q_OBJECT
    noo::MaterialID m_id;
    MaterialInit    m_init;

public:
    MaterialDelegate(noo::MaterialID, MaterialInit const&);
    virtual ~MaterialDelegate();

    NOODLES_CAN_UPDATE(true)

    noo::MaterialID id() const;

    void         update(MaterialInit const&);
    virtual void on_update(MaterialInit const&);

signals:
    void updated();
};

// =============================================================================

struct PointLight {
    float range = -1;
};

struct SpotLight {
    float range                = -1;
    float inner_cone_angle_rad = 0;
    float outer_cone_angle_rad = M_PI / 4.0;
};

struct DirectionLight {
    float range = -1;
};


struct LightInit {
    using LT = std::variant<PointLight, SpotLight, DirectionLight>;

    QString                  name;
    LT                       type;
    std::optional<QColor>    color;
    std::optional<float>     intensity;
    std::optional<glm::vec4> spatial;
};

/// The base light class for buffers. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new lights from the
/// server. Light delegates can also be updated.
class LightDelegate : public QObject {
    Q_OBJECT
    noo::LightID m_id;
    LightInit    m_init;

public:
    LightDelegate(noo::LightID, LightInit const&);
    virtual ~LightDelegate();

    NOODLES_CAN_UPDATE(true)

    noo::LightID id() const;

    void         update(LightInit const&);
    virtual void on_update(LightInit const&);

signals:
    void updated();
};

// =============================================================================

enum class Format : uint8_t {
    U8,
    U16,
    U32,

    U8VEC4,

    U16VEC2,

    VEC2,
    VEC3,
    VEC4,

    MAT3,
    MAT4,
};

enum class PrimitiveType : uint8_t {
    POINTS,
    LINES,
    LINE_LOOP,
    LINE_STRIP,
    TRIANGLES,
    TRIANGLE_STRIP,
    TRIANGLE_FAN // Not recommended, some hardware support is lacking
};

enum class AttributeSemantic : uint8_t {
    POSITION, // for the moment, must be a vec3.
    NORMAL,   // for the moment, must be a vec3.
    TANGENT,  // for the moment, must be a vec3.
    TEXTURE,  // for the moment, is either a vec2, or normalized u16vec2
    COLOR,    // normalized u8vec4, or vec4
};

struct Attribute {
    QPointer<BufferViewDelegate> view;
    AttributeSemantic            semantic;
    uint8_t                      channel = 0;

    uint64_t stride = 0;
    Format   format;

    glm::vec4 minimum_value;
    glm::vec4 maximum_value;

    bool normalized = false;
};

struct Index {
    QPointer<BufferViewDelegate> view;

    uint64_t stride = 0;
    Format   format;
};

struct MeshPatch {
    std::vector<Attribute> attributes;

    Index indicies;

    PrimitiveType type;

    QPointer<MaterialDelegate> material;
};

struct MeshInit {
    QString                               name;
    std::optional<std::vector<MeshPatch>> patches;
};

class MeshDelegate : public QObject {
    Q_OBJECT
    noo::GeometryID m_id;
    MeshInit        m_init;

public:
    MeshDelegate(noo::GeometryID, MeshInit const&);
    virtual ~MeshDelegate();

    NOODLES_CAN_UPDATE(false)

    noo::GeometryID id() const;

    virtual void prepare_delete();
};

// =============================================================================

struct EntityTextDefinition {
    std::string text;
    std::string font;
    float       height = .25;
    float       width  = -1;
};

struct EntityWebpageDefinition {
    QUrl  url;
    float height = .5;
    float width  = .5;
};

struct EntityRenderableDefinition {
    QPointer<MeshDelegate>          mesh;
    std::span<glm::mat4 const>      instances;
    std::optional<noo::BoundingBox> instance_bb;
};

using EntityDefinition = std::variant<std::monostate,
                                      EntityTextDefinition,
                                      EntityWebpageDefinition,
                                      EntityRenderableDefinition>;

struct EntityUpdateData {
    std::optional<QString>                         name;
    std::optional<EntityDelegate*>                 parent;
    std::optional<glm::mat4>                       transform;
    std::optional<EntityDefinition>                definition;
    std::optional<std::vector<LightDelegate*>>     lights;
    std::optional<std::vector<TableDelegate*>>     tables;
    std::optional<std::vector<PlotDelegate*>>      plots;
    std::optional<std::vector<QString>>            tags;
    std::optional<std::vector<MethodDelegate*>>    method_list;
    std::optional<std::vector<SignalDelegate*>>    signal_list;
    std::optional<std::optional<noo::BoundingBox>> influence;
    std::optional<bool>                            visibility;
};


/// The base delegate class for objects. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new objects from the
/// server. Object delegates can also be updated.
class EntityDelegate : public QObject {
    Q_OBJECT
    noo::EntityID    m_id;
    EntityUpdateData m_data;

    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

public:
    EntityDelegate(noo::EntityID, EntityUpdateData const&);
    virtual ~EntityDelegate();

    NOODLES_CAN_UPDATE(true)

    noo::EntityID id() const;

    void update(EntityUpdateData const&);

    virtual void on_update(EntityUpdateData const&);

    virtual void prepare_delete();

    AttachedMethodList const& attached_methods() const;
    AttachedSignalList const& attached_signals() const;

signals:
    void updated();
};

// =============================================================================

struct TableData {
    std::optional<QString>                      name;
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
    virtual void on_table_initialize(QCborArray const& names,
                                     QCborValue        keys,
                                     QCborArray const& data_cols,
                                     QCborArray const& selections);

    virtual void on_table_reset();
    virtual void on_table_updated(QCborValue keys, QCborValue columns);
    virtual void on_table_rows_removed(QCborValue keys);
    virtual void on_table_selection_updated(std::string_view,
                                            noo::SelectionRef const&);

public:
    PendingMethodReply* subscribe() const;
    PendingMethodReply* request_row_insert(QCborArray&& row) const;
    PendingMethodReply* request_rows_insert(QCborArray&& columns) const;

    PendingMethodReply* request_row_update(int64_t key, QCborArray&& row) const;
    PendingMethodReply* request_rows_update(std::vector<int64_t>&& keys,
                                            QCborArray&& columns) const;

    PendingMethodReply* request_deletion(std::span<int64_t> keys) const;

    PendingMethodReply* request_clear() const;
    PendingMethodReply* request_selection_update(std::string_view,
                                                 noo::Selection) const;

private slots:
    void interp_table_reset(QCborArray const&);
    void interp_table_update(QCborArray const&);
    void interp_table_remove(QCborArray const&);
    void interp_table_sel_update(QCborArray const&);

signals:
    void updated();
};


// =============================================================================

struct PlotSimpleDelegate {
    QString definition;
};

struct PlotURLDelegate {
    QUrl url;
};

using PlotType = std::variant<PlotSimpleDelegate, PlotURLDelegate>;

struct PlotData {
    std::optional<noo::PlotID> id;

    std::optional<QPointer<TableDelegate>> table;
    std::optional<PlotType>                type;

    std::optional<std::vector<MethodDelegate*>> method_list;
    std::optional<std::vector<SignalDelegate*>> signal_list;
};

class PlotDelegate : public QObject {
    Q_OBJECT
    noo::PlotID m_id;

    PlotType m_type;

    QPointer<TableDelegate> m_table;

    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

public:
    PlotDelegate(noo::PlotID, PlotData const&);
    virtual ~PlotDelegate();

    NOODLES_CAN_UPDATE(true)

    noo::PlotID id() const;

    void update(PlotData const&);

    virtual void on_update(PlotData const&);

    virtual void prepare_delete();

    AttachedMethodList const& attached_methods() const;
    AttachedSignalList const& attached_signals() const;

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

    template <class T, class ID, class Init>
    using DispatchFn = std::function<std::unique_ptr<T>(ID, Init const&)>;

    DispatchFn<TextureDelegate, noo::TextureID, TextureInit>    tex_maker;
    DispatchFn<BufferDelegate, noo::BufferID, BufferInit>       buffer_maker;
    DispatchFn<TableDelegate, noo::TableID, TableData>          table_maker;
    DispatchFn<LightDelegate, noo::LightID, LightInit>          light_maker;
    DispatchFn<MaterialDelegate, noo::MaterialID, MaterialInit> mat_maker;
    DispatchFn<MeshDelegate, noo::GeometryID, MeshInit>         mesh_maker;
    DispatchFn<EntityDelegate, noo::EntityID, EntityUpdateData> object_maker;
    DispatchFn<SignalDelegate, noo::SignalID, SignalInit>       sig_maker;
    DispatchFn<MethodDelegate, noo::MethodID, MethodInit>       method_maker;
    DispatchFn<PlotDelegate, noo::PlotID, PlotData>             plot_maker;
    DispatchFn<ImageDelegate, noo::ImageID, ImageInit>          image_maker;
    DispatchFn<SamplerDelegate, noo::SamplerID, SamplerInit>    sampler_maker;

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
    MeshDelegate*     get(noo::GeometryID);
    EntityDelegate*   get(noo::EntityID);
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
