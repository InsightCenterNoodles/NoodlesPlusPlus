#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H

#include "noo_common.h"
#include "noo_include_glm.h"
#include "noo_interface_types.h"

#include <cstdint>
#include <memory>
#include <span>

#include <QCborMap>
#include <QCborValue>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QUrl>

namespace noo::messages {
struct MethodArg;
struct MsgMethodCreate;
struct MsgMethodDelete;
struct MsgSignalCreate;
struct MsgSignalDelete;
struct TextRepresentation;
struct WebRepresentation;
struct InstanceSource;
struct RenderRepresentation;
struct MsgEntityCreate;
struct MsgEntityUpdate;
struct MsgEntityDelete;
struct MsgPlotCreate;
struct MsgPlotUpdate;
struct MsgPlotDelete;
struct MsgBufferCreate;
struct MsgBufferDelete;
struct MsgBufferViewCreate;
struct MsgBufferViewDelete;
struct TextureRef;
struct PBRInfo;
struct MsgMaterialCreate;
struct MsgMaterialUpdate;
struct MsgMaterialDelete;
struct MsgImageCreate;
struct MsgImageDelete;
struct MsgTextureCreate;
struct MsgTextureDelete;
struct MsgSamplerCreate;
struct MsgSamplerDelete;
struct PointLight;
struct SpotLight;
struct DirectionalLight;
struct MsgLightCreate;
struct MsgLightUpdate;
struct MsgLightDelete;
struct Attribute;
struct Index;
struct GeometryPatch;
struct MsgGeometryCreate;
struct MsgGeometryDelete;
struct MsgTableCreate;
struct MsgTableUpdate;
struct MsgTableDelete;
struct MsgDocumentUpdate;
struct MsgDocumentReset;
struct MsgSignalInvoke;
struct MethodException;
struct MsgMethodReply;
struct ServerMessage;
struct MsgIntroduction;
struct MsgInvokeMethod;
} // namespace noo::messages

namespace nooc {

class InternalClientState;

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

    MethodException() = default;
    MethodException(noo::messages::MethodException const&,
                    InternalClientState&);
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
        (arr.push_back(noo::to_cbor(std::forward<Args>(args))), ...);
        call_direct(arr);
    }

public slots:
    /// Internal use. do not touch!
    void complete(QCborValue, nooc::MethodException*);

signals:
    /// Issued when a non-error reply has been received
    void recv_data(QCborValue);

    /// Issued when the method raised an exception on the server side.
    void recv_fail(QString);

    /// Issued when the method raised an exception on the server side.
    void recv_exception(nooc::MethodException);
};

///
/// Handy classes for translating a reply to a common non-noodles type.
///
namespace replies {

class GetBoolReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;

signals:
    void recv(bool);
};

class GetIntegerReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;

signals:
    void recv(int64_t);
};

class ArrayReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;

signals:
    void recv(QCborArray);
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
    noo::from_cbor(source, ret);
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
    MethodContextPtr         m_context;
    QVector<MethodDelegate*> m_list;

    MethodDelegate* find_direct_by_name(QString) const;
    bool            check_direct_by_delegate(MethodDelegate*) const;

public:
    /// Internal use only
    AttachedMethodList(MethodContextPtr);

    /// Internal use only
    AttachedMethodList& operator=(QVector<MethodDelegate*> const& l) {
        m_list = l;
        return *this;
    }

    QVector<MethodDelegate*> const& list() const { return m_list; }


    /// Find a method by name. This returns a reply object that is actually used
    /// to call the method. Returns null if the delegate is not attached to this
    /// object/table/document.
    template <class Reply = PendingMethodReply, class... Args>
    Reply* new_call_by_name(QString v, Args&&... args) const {
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
    MethodContextPtr                         m_context;
    QVector<std::shared_ptr<AttachedSignal>> m_list;

public:
    // Internal use
    AttachedSignalList(MethodContextPtr);

    // Internal use
    AttachedSignalList& operator=(QVector<SignalDelegate*> const& l);

    /// Find the attached signal by a name. Returns null if name is not found.
    AttachedSignal* find_by_name(QString) const;

    /// Find the attached signal by a delegate ptr. Returns null i the name is
    /// not found.
    AttachedSignal* find_by_delegate(SignalDelegate*) const;
};

/// Represents an argument name/documentation pair for a method/signal.
struct ArgDoc {
    QString name;
    QString doc;
    QString editor_hint;

    ArgDoc() = default;
    ArgDoc(noo::messages::MethodArg const&, InternalClientState&);
};

/// Describes a noodles method
struct MethodInit {
    QString         method_name;
    QString         documentation;
    QString         return_documentation;
    QVector<ArgDoc> argument_documentation;

    MethodInit();
    MethodInit(noo::messages::MsgMethodCreate const&, InternalClientState&);
};

/// The base delegate class for methods. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new methods from the
/// server.
class MethodDelegate : public QObject {
    Q_OBJECT
    noo::MethodID m_id;
    MethodInit    m_data;

public:
    MethodDelegate(noo::MethodID, MethodInit const&);
    virtual ~MethodDelegate();

    void post_create(InternalClientState&);

    noo::MethodID id() const;
    QString       name() const { return m_data.method_name; }
    auto const&   info() const { return m_data; }

    virtual void on_complete();

signals:
    // private
    void invoke(noo::MethodID,
                nooc::MethodContextPtr,
                QCborArray const&,
                nooc::PendingMethodReply*);
};

// =============================================================================

struct SignalInit {
    QString         name;
    QString         documentation;
    QVector<ArgDoc> argument_documentation;

    SignalInit() = default;
    SignalInit(noo::messages::MsgSignalCreate const&, InternalClientState&);
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

    void post_create() { on_complete(); }

    noo::SignalID id() const;
    QString       name() const { return m_data.name; }
    auto const&   info() const { return m_data; }

    virtual void on_complete();

signals:
    /// Issued when this signal has been recv'ed.
    void fired(nooc::MethodContextPtr, QCborArray const&);
};

// =============================================================================

struct BufferInit {
    QString name;

    size_t byte_count;

    QByteArray inline_bytes;
    QUrl       url;

    BufferInit(noo::messages::MsgBufferCreate const&, InternalClientState&);
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

    void post_create(InternalClientState&);

    noo::BufferID id() const;
    QString       name() const { return m_data.name; }
    auto const&   info() const { return m_data; }

    bool       is_data_ready() const { return m_buffer_bytes.size(); }
    QByteArray data() const { return m_buffer_bytes; }

    virtual void on_complete();


private slots:
    void ready_read(QByteArray);
    void on_error(QString);
    void on_data_progress(qint64 received, qint64 total);

signals:
    void data_ready(QByteArray);
    void data_progress(qint64 received, qint64 total);
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

    BufferViewInit(noo::messages::MsgBufferViewCreate const&,
                   InternalClientState&);
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

    void post_create();

    noo::BufferViewID id() const;
    QString           name() const { return m_init.name; }
    auto const&       info() const { return m_init; }

    bool            is_data_ready() const;
    BufferDelegate* source_buffer() const;
    QByteArray      get_sub_range() const;

    virtual void on_complete();

private slots:
    void ready_read(QByteArray);
signals:
    void data_ready(QByteArray);
};

// =============================================================================

struct ImageInit {
    QString name;

    QPointer<BufferViewDelegate> local_image;
    QUrl                         remote_image;

    ImageInit(noo::messages::MsgImageCreate const&, InternalClientState&);
};

class ImageDelegate : public QObject {
    Q_OBJECT
    noo::ImageID m_id;
    ImageInit    m_init;

    QImage m_image;

public:
    ImageDelegate(noo::ImageID, ImageInit const&);
    virtual ~ImageDelegate();

    void post_create(InternalClientState&);

    noo::ImageID id() const;
    QString      name() const { return m_init.name; }
    auto const&  info() const { return m_init; }

    bool is_data_ready() const { return !m_image.isNull(); }

    QImage const& image() const { return m_image; }

    virtual void on_complete();

private slots:
    void ready_read(QByteArray);
    void on_error(QString);

signals:
    void data_ready(QImage);
    void data_progress(qint64 received, qint64 total);
};

// =============================================================================

enum class MagFilter : uint8_t {
    NEAREST,
    LINEAR,
};

enum class MinFilter : uint8_t {
    NEAREST,
    LINEAR,
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

    SamplerInit(noo::messages::MsgSamplerCreate const&, InternalClientState&);
};

class SamplerDelegate : public QObject {
    Q_OBJECT
    noo::SamplerID m_id;
    SamplerInit    m_init;

public:
    SamplerDelegate(noo::SamplerID, SamplerInit const&);
    virtual ~SamplerDelegate();

    void post_create() { on_complete(); }

    noo::SamplerID id() const;
    QString        name() const { return m_init.name; }
    auto const&    info() const { return m_init; }

    virtual void on_complete();
};

// =============================================================================

struct TextureInit {
    QString                   name;
    QPointer<ImageDelegate>   image;
    QPointer<SamplerDelegate> sampler;

    TextureInit(noo::messages::MsgTextureCreate const&, InternalClientState&);
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

    void post_create();

    noo::TextureID id() const;
    QString        name() const { return m_init.name; }
    auto const&    info() const { return m_init; }

    bool is_data_ready() const { return !m_init.image->is_data_ready(); }

    virtual void on_complete();

private slots:
    void ready_read(QImage);

signals:
    void data_ready(QImage);
};

// =============================================================================

struct TextureRef {
    QPointer<TextureDelegate> texture;
    glm::mat3                 transform          = glm::mat3(1);
    uint8_t                   texture_coord_slot = 0;

    TextureRef(noo::messages::TextureRef const&, InternalClientState&);
};

struct PBRInfo {
    QColor                    base_color = Qt::white;
    std::optional<TextureRef> base_color_texture;

    float                     metallic  = 1.0;
    float                     roughness = 1.0;
    std::optional<TextureRef> metal_rough_texture;

    PBRInfo(noo::messages::PBRInfo const&, InternalClientState&);
};

struct MaterialInit {
    QString name;

    PBRInfo                   pbr_info;
    std::optional<TextureRef> normal_texture;

    std::optional<TextureRef> occlusion_texture;
    float                     occlusion_texture_factor = 1;

    std::optional<TextureRef> emissive_texture;
    glm::vec3                 emissive_factor = glm::vec3(1);

    bool  use_alpha    = false;
    float alpha_cutoff = .5;
    bool  double_sided = false;

    MaterialInit(noo::messages::MsgMaterialCreate const&, InternalClientState&);
};

struct MaterialUpdate { };

/// The base delegate class for materials. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new materials from
/// the server. Material delegates can also be updated.
class MaterialDelegate : public QObject {
    Q_OBJECT
    noo::MaterialID m_id;
    MaterialInit    m_init;

    size_t m_unready_textures = 0;

public:
    MaterialDelegate(noo::MaterialID, MaterialInit const&);
    virtual ~MaterialDelegate();

    void post_create();

    noo::MaterialID id() const;
    QString         name() const { return m_init.name; }
    auto const&     info() const { return m_init; }

    bool is_data_ready() const { return !m_unready_textures; }

    void         update(MaterialUpdate const&);
    virtual void on_update(MaterialUpdate const&);
    virtual void on_complete();

private slots:
    void texture_ready(QImage);

signals:
    void updated();
    void all_textures_ready();
};

// =============================================================================

struct PointLight {
    float range = -1;

    PointLight() = default;
    PointLight(noo::messages::PointLight const&, InternalClientState&);
};

struct SpotLight {
    float range                = -1;
    float inner_cone_angle_rad = 0;
    float outer_cone_angle_rad = M_PI / 4.0;

    SpotLight() = default;
    SpotLight(noo::messages::SpotLight const&, InternalClientState&);
};

struct DirectionLight {
    float range = -1;

    DirectionLight() = default;
    DirectionLight(noo::messages::DirectionalLight const&,
                   InternalClientState&);
};


struct LightInit {
    using LT = std::variant<PointLight, SpotLight, DirectionLight>;

    QString name;
    LT      type;
    QColor  color     = Qt::white;
    float   intensity = 1;

    LightInit(noo::messages::MsgLightCreate const&, InternalClientState&);
};

struct LightUpdate {
    std::optional<QColor> color;
    std::optional<float>  intensity;

    LightUpdate(noo::messages::MsgLightUpdate const&, InternalClientState&);
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

    void post_create() { on_complete(); }

    noo::LightID id() const;
    QString      name() const { return m_init.name; }
    auto const&  info() const { return m_init; }

    void         update(LightUpdate const&);
    virtual void on_update(LightUpdate const&);
    virtual void on_complete();

signals:
    void updated();
};

// =============================================================================

struct Attribute {
    QPointer<BufferViewDelegate> view;
    noo::AttributeSemantic       semantic;
    uint8_t                      channel = 0;

    uint64_t    offset = 0;
    uint64_t    stride = 0;
    noo::Format format;

    QVector<float> minimum_value;
    QVector<float> maximum_value;

    bool normalized = false;

    Attribute() = default;
    Attribute(noo::messages::Attribute const&, InternalClientState&);
};

class Index : public QObject {
    Q_OBJECT
public:
    QPointer<BufferViewDelegate> view;

    uint64_t count;

    uint64_t    offset = 0;
    uint64_t    stride = 0;
    noo::Format format;

    Index() = default;
    Index(noo::messages::Index const&, InternalClientState&);

    bool is_ready();

signals:
    void ready();
};

class MeshPatch : public QObject {
    Q_OBJECT

    uint64_t m_unready_buffers   = 0;
    bool     m_is_material_ready = false;

public:
    QVector<Attribute> attributes;

    uint64_t count;

    QPointer<Index> indicies;

    noo::PrimitiveType type;

    QPointer<MaterialDelegate> material;

    MeshPatch() = default;
    MeshPatch(noo::messages::GeometryPatch const&, InternalClientState&);

    bool is_ready() const { return !m_unready_buffers and m_is_material_ready; }

private slots:
    void on_buffer_ready();
    void on_material_ready();
    void invalidate_material();

signals:
    void ready();
    void invalidated();
};

struct MeshInit : public QObject {
    Q_OBJECT
public:
    QString                      name;
    QVector<QPointer<MeshPatch>> patches;

    MeshInit(noo::messages::MsgGeometryCreate const&, InternalClientState&);
};

class MeshDelegate : public QObject {
    Q_OBJECT
    noo::GeometryID m_id;
    MeshInit const* m_init;

    uint64_t m_patch_unready = 0;

public:
    MeshDelegate(noo::GeometryID, MeshInit const&);
    virtual ~MeshDelegate();

    void post_create();

    noo::GeometryID id() const;
    QString         name() const { return m_init->name; }
    auto const&     info() const { return m_init; }

    bool is_complete() { return !m_patch_unready; }

    virtual void on_complete();

private slots:
    void on_patch_ready();
    void on_patch_invalidated();

signals:
    void ready();
};

// =============================================================================

struct EntityTextDefinition {
    QString text;
    QString font   = "Arial";
    float   height = .25;
    float   width  = -1;

    EntityTextDefinition(noo::messages::TextRepresentation const&,
                         InternalClientState&);
};

struct EntityWebpageDefinition {
    QUrl  url;
    float height = .5;
    float width  = .5;

    EntityWebpageDefinition(noo::messages::WebRepresentation const&,
                            InternalClientState&);
};

struct InstanceSource {
    QPointer<BufferViewDelegate>    view;
    uint64_t                        stride = 0;
    std::optional<noo::BoundingBox> instance_bb;

    InstanceSource(noo::messages::InstanceSource const&, InternalClientState&);
};

struct EntityRenderableDefinition {
    QPointer<MeshDelegate>        mesh;
    std::optional<InstanceSource> instances;

    EntityRenderableDefinition(noo::messages::RenderRepresentation const&,
                               InternalClientState&);
};

struct EntityDefinition : std::variant<std::monostate,
                                       EntityTextDefinition,
                                       EntityWebpageDefinition,
                                       EntityRenderableDefinition> {
    using variant::variant;
};

struct EntityInit {
    QString name;

    QPointer<EntityDelegate> parent;
    glm::mat4                transform = glm::mat4(1);
    EntityDefinition         definition;
    QVector<LightDelegate*>  lights;
    QVector<TableDelegate*>  tables;
    QVector<PlotDelegate*>   plots;
    QStringList              tags;
    QVector<MethodDelegate*> methods_list;
    QVector<SignalDelegate*> signals_list;
    noo::BoundingBox         influence;
    bool                     visible;

    EntityInit(noo::messages::MsgEntityCreate const&, InternalClientState&);
};

struct EntityUpdateData {
    std::optional<QPointer<EntityDelegate>> parent;
    std::optional<glm::mat4>                transform;
    std::optional<EntityDefinition>         definition;
    std::optional<QVector<LightDelegate*>>  lights;
    std::optional<QVector<TableDelegate*>>  tables;
    std::optional<QVector<PlotDelegate*>>   plots;
    std::optional<QStringList>              tags;
    std::optional<QVector<MethodDelegate*>> methods_list;
    std::optional<QVector<SignalDelegate*>> signals_list;
    std::optional<noo::BoundingBox>         influence;
    std::optional<bool>                     visible;

    EntityUpdateData(noo::messages::MsgEntityUpdate const&,
                     InternalClientState&);
};


/// The base delegate class for objects. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new objects from the
/// server. Object delegates can also be updated.
class EntityDelegate : public QObject {
    Q_OBJECT
    noo::EntityID m_id;
    EntityInit    m_data;

    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

public:
    EntityDelegate(noo::EntityID, EntityInit const&);
    virtual ~EntityDelegate();

    void post_create();

    noo::EntityID id() const;
    QString       name() const { return m_data.name; }
    auto const&   info() const { return m_data; }

    void update(EntityUpdateData const&);

    virtual void on_update(EntityUpdateData const&);

    virtual void on_complete();

    AttachedMethodList const& attached_methods() const;
    AttachedSignalList const& attached_signals() const;

signals:
    void updated();

public slots:
    void                         activate_str(QString);
    void                         activate_int(int slot = 0);
    replies::GetStringListReply* get_activate_choices();


    replies::GetStringListReply* get_var_keys();
    replies::ArrayReply*         get_var_options(QString key = QString());
    nooc::PendingMethodReply*    get_var(QString key = QString());
    replies::GetBoolReply* set_var(QCborValue value, QString key = QString());


    void set_position(glm::vec3);
    void set_rotation(glm::quat);
    void set_scale(glm::vec3);
};

// =============================================================================

struct TableInit {
    QString name;

    QVector<MethodDelegate*> methods_list;
    QVector<SignalDelegate*> signals_list;

    TableInit(noo::messages::MsgTableCreate const&, InternalClientState&);
};

struct TableUpdate {
    std::optional<QVector<MethodDelegate*>> methods_list;
    std::optional<QVector<SignalDelegate*>> signals_list;

    TableUpdate(noo::messages::MsgTableUpdate const&, InternalClientState&);
};

struct TableDataInit {
    struct ColumnInfo {
        QString name;
        QString type;

        ColumnInfo() = default;
        ColumnInfo(QCborValue);
    };

    TableDataInit() = default;
    TableDataInit(QCborValue);

    QVector<ColumnInfo>     names;
    QVector<int64_t>        keys;
    QVector<QCborArray>     rows;
    QVector<noo::Selection> selections;
};

/// The base delegate class for tables. Users can inherit from this to add
/// their own functionality. Delegates are instantiated on new tables from the
/// server. Table delegates can also be updated.
class TableDelegate : public QObject {
    Q_OBJECT
    noo::TableID m_id;

    TableInit m_init;

    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

    std::vector<QMetaObject::Connection> m_spec_signals;

    void update_from(std::optional<QVector<MethodDelegate*>> methods_list,
                     std::optional<QVector<SignalDelegate*>> signals_list);

public:
    TableDelegate(noo::TableID, TableInit const&);
    virtual ~TableDelegate();

    void post_create();

    noo::TableID id() const;
    QString      name() const { return m_init.name; }
    auto const&  info() const { return m_init; }


    void update(TableUpdate const&);

    virtual void on_update(TableUpdate const&);
    virtual void on_complete();

    AttachedMethodList const& attached_methods() const;
    AttachedSignalList const& attached_signals() const;

public slots:
    virtual void on_table_subscribed(TableDataInit const&);
    virtual void on_table_reset(TableDataInit const&);
    virtual void on_table_rows_updated(QVector<int64_t> keys, QCborArray rows);
    virtual void on_table_rows_removed(QVector<int64_t> keys);
    virtual void on_table_selection_updated(noo::Selection const&);

public:
    PendingMethodReply* subscribe() const;
    PendingMethodReply* request_row_insert(QCborArray row) const;
    PendingMethodReply* request_rows_insert(QCborArray rows) const;

    PendingMethodReply* request_row_update(int64_t key, QCborArray row) const;
    PendingMethodReply* request_rows_update(QVector<int64_t> keys,
                                            QCborArray       rows) const;

    PendingMethodReply* request_deletion(QVector<int64_t> keys) const;

    PendingMethodReply* request_clear() const;
    PendingMethodReply* request_selection_update(noo::Selection) const;

private slots:
    void interp_table_reset(QCborArray const&);
    void interp_table_update(QCborArray const&);
    void interp_table_remove(QCborArray const&);
    void interp_table_sel_update(QCborArray const&);

signals:
    void updated();
};


// =============================================================================

using PlotType = std::variant<QString, QUrl>;

struct PlotInit {
    QString name;

    QPointer<TableDelegate> table;
    PlotType                type;

    QVector<MethodDelegate*> methods_list;
    QVector<SignalDelegate*> signals_list;

    PlotInit(noo::messages::MsgPlotCreate const&, InternalClientState&);
};

struct PlotUpdate {
    std::optional<QPointer<TableDelegate>> table;
    std::optional<PlotType>                type;

    std::optional<QVector<MethodDelegate*>> methods_list;
    std::optional<QVector<SignalDelegate*>> signals_list;

    PlotUpdate(noo::messages::MsgPlotUpdate const&, InternalClientState&);
};

class PlotDelegate : public QObject {
    Q_OBJECT
    noo::PlotID m_id;

    PlotInit m_init;

    QPointer<TableDelegate> m_table;
    PlotType                m_type;

    AttachedMethodList m_attached_methods;
    AttachedSignalList m_attached_signals;

public:
    PlotDelegate(noo::PlotID, PlotInit const&);
    virtual ~PlotDelegate();

    void post_create();

    noo::PlotID id() const;
    QString     name() const { return m_init.name; }
    auto const& info() const { return m_init; }

    void update(PlotUpdate const&);

    virtual void on_update(PlotUpdate const&);
    virtual void on_complete();

    AttachedMethodList const& attached_methods() const;
    AttachedSignalList const& attached_signals() const;

signals:
    void updated();
};


// =============================================================================

struct DocumentData {
    std::optional<QVector<MethodDelegate*>> methods_list;
    std::optional<QVector<SignalDelegate*>> signals_list;

    DocumentData(noo::messages::MsgDocumentUpdate const&, InternalClientState&);
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

    DispatchFn<TextureDelegate, noo::TextureID, TextureInit> tex_maker;
    DispatchFn<BufferDelegate, noo::BufferID, BufferInit>    buffer_maker;
    DispatchFn<BufferViewDelegate, noo::BufferViewID, BufferViewInit>
                                                       buffer_view_maker;
    DispatchFn<TableDelegate, noo::TableID, TableInit> table_maker;
    DispatchFn<LightDelegate, noo::LightID, LightInit> light_maker;
    DispatchFn<MaterialDelegate, noo::MaterialID, MaterialInit> mat_maker;
    DispatchFn<MeshDelegate, noo::GeometryID, MeshInit>         mesh_maker;
    DispatchFn<EntityDelegate, noo::EntityID, EntityInit>       object_maker;
    DispatchFn<SignalDelegate, noo::SignalID, SignalInit>       sig_maker;
    DispatchFn<MethodDelegate, noo::MethodID, MethodInit>       method_maker;
    DispatchFn<PlotDelegate, noo::PlotID, PlotInit>             plot_maker;
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
class Client : public QObject {
    Q_OBJECT
    std::unique_ptr<ClientCore> m_data;

public:
    Client(QObject* parent = nullptr);
    ~Client();

    /// (re)open a new connection to a server.
    /// Existing delegates are destroyed.
    void open(QUrl server, ClientDelegates&&);

    /// Disconnect from the server
    void disconnect();

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

    /// Issued when the server has sent all the initial scene state
    void initialized();

    /// Issued when we (or the server) close the connection.
    void disconnected();

    /// Provides access to raw messages from the server
    void on_raw_message(QByteArray);
};

} // namespace nooc

#endif // CLIENT_INTERFACE_H
