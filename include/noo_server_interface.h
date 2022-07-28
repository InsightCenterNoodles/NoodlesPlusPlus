#pragma once

#include "noo_common.h"
#include "noo_include_glm.h"
#include "noo_interface_types.h"

#include <QAbstractTableModel>
#include <QColor>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

class QCommandLineParser;

namespace noo {

///
/// \brief Get a view of the underlying bytes of a QByteArray
///
std::span<std::byte> span_from_byte_array(QByteArray);


class ServerT;
class ClientT;

using ServerTPtr = std::shared_ptr<ServerT>;

class DocumentT;

using DocumentTPtr    = std::shared_ptr<DocumentT>;
using DocumentTPtrRef = std::shared_ptr<DocumentT> const&;

class TableT;
using TableTPtr = std::shared_ptr<TableT>;

class ObjectT;
using ObjectTPtr = std::shared_ptr<ObjectT>;

class PlotT;
using PlotTPtr = std::shared_ptr<PlotT>;

// =============================================================================

struct MethodContext;

template <class T>
T _any_call_getter(QCborArray const& source, size_t& loc) {
    auto at_i = source[loc];
    loc++;

    T ret;
    from_cbor(at_i, ret);
    return ret;
}


template <class Func, class... Args>
auto _call(Func&& f, MethodContext const& c, QCborArray const& source) {
    size_t i = 0;
    return f(c, std::move(_any_call_getter<Args>(source, i))...);
}

template <class Lambda>
struct AnyCallHelper : AnyCallHelper<decltype(&Lambda::operator())> { };

template <class R, class... Args>
struct AnyCallHelper<R(MethodContext const&, Args...)> {
    template <class Func>
    static auto
    call(Func&& f, MethodContext const& c, QCborArray const& source) {
        return _call<Func, Args...>(f, c, source);
    }
};

template <class R, class... Args>
struct AnyCallHelper<R (*)(MethodContext const&, Args...)> {
    template <class Func>
    static auto
    call(Func&& f, MethodContext const& c, QCborArray const& source) {
        return _call<Func, Args...>(f, c, source);
    }
};

template <class R, class C, class... Args>
struct AnyCallHelper<R (C::*)(MethodContext const&, Args...)> {
    template <class Func>
    static auto
    call(Func&& f, MethodContext const& c, QCborArray const& source) {
        return _call<Func, Args...>(f, c, source);
    }
};

template <class R, class C, class... Args>
struct AnyCallHelper<R (C::*)(MethodContext const&, Args...) const> {
    template <class Func>
    static auto
    call(Func&& f, MethodContext const& c, QCborArray const& source) {
        return _call<Func, Args...>(f, c, source);
    }
};

///
/// Take a list of Any vars, decode them to compatible arguments for a given
/// function, and then call that function.
///
template <class Func>
auto any_call_helper(Func&&               f,
                     MethodContext const& c,
                     QCborArray const&    source) {
    using FType = std::remove_cvref_t<Func>;
    return AnyCallHelper<FType>::call(f, c, source);
}

/// \brief The base exception type for methods.
///
/// This should only be thrown inside code that handles method requests, and is
/// used to communicate disappointment to calling clients.
struct MethodException : public std::exception {
    int        m_code;
    QString    m_reason;
    QCborValue m_data;

public:
    MethodException(int code, QString message, QCborValue data = {});

    int               code() const { return m_code; }
    auto              reason() const { return m_reason; }
    QCborValue const& data() const { return m_data; }

    char const* what() const noexcept override;

    QString to_string() const;
};

// Methods =====================================================================

///
/// \brief The MethodContext struct helps method code know which component the
/// method is being called on.
///
struct MethodContext
    : public std::variant<std::monostate, TableTPtr, ObjectTPtr, PlotTPtr> {
    using variant::variant;

    /// Used to track the calling client. Internal ONLY.
    ClientT* client = nullptr;

    TableTPtr  get_table() const;
    ObjectTPtr get_object() const;
    PlotTPtr   get_plot() const;
};

struct Arg {
    QString name;
    QString documentation;
    QString editor_hint;
};

///
/// \brief The MethodData struct defines a noodles method.
///
struct MethodData {
    QString          method_name;
    QString          documentation;
    QString          return_documentation;
    std::vector<Arg> argument_documentation;

    std::function<QCborValue(MethodContext const&, QCborArray const&)> code;

    /// Set the code to be called when the method is invoked. The function f can
    /// be any function and the parameters will be decoded. See noo::RealListArg
    /// as an example of constraining what parameters you want Anys to be
    /// decoded to.
    template <class Function>
    void set_code(Function&& f) {
        code = [lf = std::move(f)](MethodContext const& c,
                                   QCborArray const&    v) {
            return any_call_helper(lf, c, v);
        };
    }
};

class MethodT;
using MethodTPtr = std::shared_ptr<MethodT>;

/// Create a new method.
MethodTPtr create_method(DocumentTPtrRef, MethodData const&);
MethodTPtr create_method(DocumentT*, MethodData const&);


// Signals =====================================================================

///
/// \brief The SignalData struct describes a new signal
///
struct SignalData {
    QString          signal_name;
    QString          documentation;
    std::vector<Arg> argument_documentation;
};

class SignalT;
using SignalTPtr = std::shared_ptr<SignalT>;

/// Create a new signal.
SignalTPtr create_signal(DocumentTPtrRef, SignalData const&);
SignalTPtr create_signal(DocumentT*, SignalData const&);

// Server ======================================================================

struct ServerOptions {
    uint16_t port       = 50000;
    uint16_t asset_port = 50001;
    QString  asset_hostname; //< If blank, automatic
};

/// Create a new server, which uses a WebSocket to listen on the given port.
std::shared_ptr<ServerT> create_server(ServerOptions const&);

/// Create a server from common command line options
/// Set up the parser with application information first.
/// This function will process arguments, and can then be checked afterwards for
/// custom arguments.
std::shared_ptr<ServerT> create_server(QCommandLineParser&);

// Document ====================================================================

/// Get the document of a server.
std::shared_ptr<DocumentT> get_document(ServerT*);

///
/// \brief The DocumentData struct is used to update the noodles document
///
struct DocumentData {
    std::optional<QVector<MethodTPtr>> method_list;
    std::optional<QVector<SignalTPtr>> signal_list;
};

/// Update the document with new methods and signals
void update_document(DocumentTPtrRef, DocumentData const&);

/// Issue a signal (by name or by object), on this document.
void issue_signal_direct(DocumentT*, SignalT*, QCborArray);
void issue_signal_direct(DocumentT*, std::string const&, QCborArray);

// Buffer ======================================================================

/// Instruct the buffer system to own the given bytes
struct BufferInlineSource {
    QByteArray data;
};

/// Instruct the buffer system to reference the URL for a buffer
struct BufferURLSource {
    QUrl   url_source;
    size_t source_byte_size;
};

struct BufferData {
    QString name;

    std::variant<BufferInlineSource, BufferURLSource> source;
};

class BufferT;
using BufferTPtr = std::shared_ptr<BufferT>;

/// Create a new buffer
BufferTPtr create_buffer(DocumentTPtrRef, BufferData const&);

/// Create a new buffer from a file
BufferTPtr create_buffer_from_file(DocumentTPtrRef, QString path);

// Buffer ======================================================================

enum class ViewType : uint8_t {
    UNKNOWN,
    GEOMETRY_INFO,
    IMAGE_INFO,
};

struct BufferViewData {
    QString name;

    BufferTPtr source_buffer;

    ViewType type   = ViewType::UNKNOWN;
    uint64_t offset = 0;
    uint64_t length = 0;
};

class BufferViewT;
using BufferViewTPtr = std::shared_ptr<BufferViewT>;

/// Create a new view
BufferViewTPtr create_buffer_view(DocumentTPtrRef, BufferViewData const&);

// Image =======================================================================

struct ImageData {
    QString name;

    std::variant<QUrl, BufferViewTPtr> source;
};

class ImageT;
using ImageTPtr = std::shared_ptr<ImageT>;

/// Create a new view
ImageTPtr create_image(DocumentTPtrRef, ImageData const&);

// Sampler =====================================================================

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

struct SamplerData {
    QString name;

    MagFilter mag_filter = MagFilter::LINEAR;
    MinFilter min_filter = MinFilter::LINEAR_MIPMAP_LINEAR;

    SamplerMode wrap_s = SamplerMode::REPEAT;
    SamplerMode wrap_t = SamplerMode::REPEAT;
};

class SamplerT;
using SamplerTPtr = std::shared_ptr<SamplerT>;

/// Create a new view
SamplerTPtr create_sampler(DocumentTPtrRef, SamplerData const&);

// Texture =====================================================================

///
/// \brief The TextureData struct describes a new texture as a byte range of a
/// given buffer.
///
struct TextureData {
    QString     name;
    ImageTPtr   image;   // may not be blank.
    SamplerTPtr sampler; // may be blank.
};

class TextureT;
using TextureTPtr = std::shared_ptr<TextureT>;

/// Create a new texture.
TextureTPtr create_texture(DocumentTPtrRef, TextureData const&);

/// Create a new texture. Automatically creates buffers and views
TextureTPtr create_texture(DocumentTPtrRef, QImage img);


// Material ====================================================================

struct TextureRef {
    TextureTPtr source;
    glm::mat3   transform          = glm::mat3(1);
    uint8_t     texture_coord_slot = 0;
};

struct PBRInfo {
    QColor                    base_color;
    std::optional<TextureRef> base_color_texture;

    float                     metallic  = 1.0;
    float                     roughness = 1.0;
    std::optional<TextureRef> metal_rough_texture;
};

///
/// \brief The MaterialData struct defines a new material.
///
struct MaterialData {
    QString                   name;
    PBRInfo                   pbr_info;
    std::optional<TextureRef> normal_texture;

    std::optional<TextureRef> occlusion_texture;
    std::optional<float>      occlusion_texture_factor = 1.0;

    std::optional<TextureRef> emissive_texture;
    std::optional<glm::vec3>  emissive_factor;

    std::optional<bool>  use_alpha    = false;
    std::optional<float> alpha_cutoff = .5;

    std::optional<bool> double_sided = false;
};

class MaterialT;
using MaterialTPtr = std::shared_ptr<MaterialT>;

/// Create a new material
MaterialTPtr create_material(DocumentTPtrRef, MaterialData const&);

/// Update a material
void update_material(MaterialTPtr, MaterialData const&);

// Light =======================================================================

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


///
/// \brief The LightData struct defines a new light
///
struct LightData {
    QString name;
    QColor  color     = Qt::white;
    float   intensity = 1;

    std::variant<PointLight, SpotLight, DirectionLight> type;
};

struct LightUpdateData {
    // todo, add more
    std::optional<QColor> color;
    std::optional<float>  intensity;
};

class LightT;
using LightTPtr = std::shared_ptr<LightT>;

/// Create a new light
LightTPtr create_light(DocumentTPtrRef, LightData const&);

/// Update a light
void update_light(LightTPtr const&, LightUpdateData const&);

// Mesh ========================================================================


struct Attribute {
    BufferViewTPtr    view;
    AttributeSemantic semantic;
    uint8_t           channel = 0;

    uint64_t offset = 0;
    uint64_t stride = 0;
    Format   format;

    QVector<float> minimum_value;
    QVector<float> maximum_value;

    bool normalized = false;
};

struct Index {
    BufferViewTPtr view;

    uint64_t count = 0;

    uint64_t offset = 0;
    uint64_t stride = 0;
    Format   format;
};

struct MeshPatch {
    std::vector<Attribute> attributes;

    uint64_t vertex_count = 0;

    std::optional<Index> indicies;

    PrimitiveType type;

    MaterialTPtr material;
};

///
/// \brief The MeshData struct defines a new mesh as a series of optional vertex
/// components
///
struct MeshData {
    QString name;

    std::vector<MeshPatch> patches;
};

class MeshT;
using MeshTPtr = std::shared_ptr<MeshT>;

/// Create a new mesh.
MeshTPtr create_mesh(DocumentTPtrRef, MeshData const&);

// Buffer Construction =========================================================

///
/// \brief The BufferMeshDataRef struct is a helper type to define mesh
/// information.
///
/// Vertex arrays should either be size zero or exactly equal to the size of
/// positions; these are per-vertex arrays.
///
/// Index arrays: only lines OR triangles should be used. These are indexes of
/// vertex information.
///
struct MeshSource {
    MaterialTPtr material;

    // Vertex data
    std::span<glm::vec3 const>    positions;
    std::span<glm::vec3 const>    normals;
    std::span<glm::u16vec2 const> textures;
    std::span<glm::u8vec4 const>  colors;

    // Index data
    std::span<std::byte const> indicies;

    Format index_format = Format::U16;

    enum Type {
        LINE,
        TRIANGLE,
    } type = Type::TRIANGLE;
};

struct BuilderBytes {
    QByteArray bytes;
    ViewType   type;
};

using BufferSources =
    QHash<QString, std::variant<BuilderBytes, QImage, MeshSource>>;

using BufferDirectory = QHash<QString, std::variant<BufferViewTPtr, MeshTPtr>>;

BufferDirectory create_directory(DocumentTPtrRef, BufferSources);

noo::MeshTPtr create_mesh(DocumentTPtrRef, noo::MeshSource const& src);

// Plot ========================================================================

using PlotDef = std::variant<QString, QUrl>;

struct PlotData {
    QString                            name;
    std::optional<PlotDef>             definition;
    std::optional<TableTPtr>           table_link;
    std::optional<QVector<MethodTPtr>> method_list;
    std::optional<QVector<SignalTPtr>> signal_list;
};

struct PlotUpdateData {
    std::optional<PlotDef>             definition;
    std::optional<TableTPtr>           table_link;
    std::optional<QVector<MethodTPtr>> method_list;
    std::optional<QVector<SignalTPtr>> signal_list;
};

/// Create a new plot.
MeshTPtr create_plot(DocumentTPtrRef, PlotData const&);

/// Update a plot.
void update_plot(DocumentTPtrRef, PlotUpdateData const&);

// Table =======================================================================

///
/// \brief The ServerTableDelegate class is the base type for tables. Users
/// should inherit from this and override the functionality they desire.
///
class ServerTableDelegate : public QObject {
    Q_OBJECT

public:
    ServerTableDelegate(QObject* p);
    virtual ~ServerTableDelegate();

    virtual QStringList                       get_headers();
    virtual std::pair<QCborArray, QCborArray> get_all_data(); // keys, rows
    virtual QList<Selection>                  get_all_selections();

    // list of rows, return new keys.
    virtual void handle_insert(QCborArray const& new_rows);

    // list of keys, list of columns
    virtual void handle_update(QCborArray const& keys, QCborArray const& rows);

    // list of keys
    virtual void handle_deletion(QCborArray const& keys);

    // ask to clear
    virtual void handle_reset();
    virtual void handle_set_selection(Selection const&);

signals:
    void table_reset();
    void table_selection_updated(noo::Selection const&);
    void table_row_updated(QCborArray keys, QCborArray rows);
    void table_row_deleted(QCborArray keys);
};

// Useful Table Delegates ====

// Abstract Table Delegate; this is an interface to an abstract table model,
// with optional modification provided through another inherited class.
// This can be used to interface with SQL tables, etc.

struct AbstractTableInsertable {
    virtual void handle_insert(QCborArray const& new_rows) = 0;
    virtual void handle_update(QCborArray const& keys,
                               QCborArray const& rows)     = 0;
    virtual void handle_deletion(QCborArray const& keys)   = 0;
    virtual void handle_reset()                            = 0;
};

class AbstractTableDelegate : public ServerTableDelegate {
    Q_OBJECT

    QVector<QMetaObject::Connection> m_connections;

    QPointer<QAbstractTableModel> m_table_model;
    AbstractTableInsertable*      m_insertable;

    uint64_t m_counter = 0;

    uint64_t next_counter();

    QHash<uint64_t, QPersistentModelIndex> m_key_to_model_map;
    QHash<QPersistentModelIndex, uint64_t> m_model_to_key_map;

    QHash<QString, Selection> m_selections;

    void reconnect_standard(QAbstractTableModel*);

protected:
    void handle_insert(QCborArray const& new_rows) override;
    void handle_update(QCborArray const& keys, QCborArray const& rows) override;
    void handle_deletion(QCborArray const& keys) override;
    void handle_reset() override;
    void handle_set_selection(Selection const&) override;

public:
    template <class Model>
    AbstractTableDelegate(Model* model, QObject* parent)
        : ServerTableDelegate(parent) {
        set_model(model);
    }
    ~AbstractTableDelegate() override;

    QStringList                       get_headers() override;
    std::pair<QCborArray, QCborArray> get_all_data() override;
    QList<Selection>                  get_all_selections() override;

    template <class Model>
    void set_model(Model* model = nullptr) {
        static_assert(std::is_base_of<QAbstractTableModel, Model>::value);

        if (m_table_model == model) return;

        for (auto const& c : m_connections) {
            disconnect(c);
        }

        m_key_to_model_map.clear();
        m_model_to_key_map.clear();
        m_selections.clear();

        m_table_model = model;

        if (!m_table_model) return;

        reconnect_standard(model);

        if constexpr (std::is_base_of<AbstractTableInsertable, Model>::value) {
            m_insertable = model;
        }

        if (model->columnCount() != 0) {
            for (auto i = 0; i < model->rowCount(); i++) {
                auto key = next_counter();
                auto mi  = QPersistentModelIndex(model->index(i, 0));
                m_key_to_model_map[key] = mi;
                m_model_to_key_map[mi]  = key;
            }
        }

        emit table_reset();
    }

private slots:
    void act_model_destroy();
    void act_model_reset();
    void act_model_rows_inserted(QModelIndex, int, int);
    void act_model_rows_removed(QModelIndex, int, int);
    void act_model_data_changed(QModelIndex, QModelIndex, QVector<int>);
};

// This class is a delegate around simple variants. It's not very efficient, but
// does the job.
class VariantTableDelegate : public ServerTableDelegate {
    Q_OBJECT

    QStringList                 m_headers;
    QHash<uint64_t, QCborArray> m_rows;
    uint64_t                    m_counter = 0;

    uint64_t next_counter();

    void normalize_row(QCborArray&);
    // new keys, new rows
    std::pair<QCborArray, QCborArray> common_insert(QCborArray const& new_rows);


    QHash<QString, Selection> m_selections;

public:
    VariantTableDelegate(QStringList column_names,
                         QCborArray  initial_rows,
                         QObject*    p);
    virtual ~VariantTableDelegate();

    QStringList                       get_headers() override;
    std::pair<QCborArray, QCborArray> get_all_data() override;
    QList<Selection>                  get_all_selections() override;

    void handle_insert(QCborArray const& new_rows) override;
    void handle_update(QCborArray const& keys, QCborArray const& rows) override;
    void handle_deletion(QCborArray const& keys) override;
    void handle_reset() override;
    void handle_set_selection(Selection const&) override;
};

///
/// \brief The TableData struct helps define a new table
///
struct TableData {
    QString name;
    QString meta;

    std::shared_ptr<ServerTableDelegate> source;
};

/// Create a new table
TableTPtr create_table(DocumentTPtrRef, TableData const&);

void issue_signal_direct(TableT*, SignalT*, QCborArray);

// Object ======================================================================

class EntityCallbacks : public QObject {
    Q_OBJECT

protected:
    ObjectT* get_host();

public:
    enum SelAction : int8_t {
        DESELECT = -1,
        REPLACE  = 0,
        SELECT   = 1,
    };

    struct EnableCallback {
        bool activation         = false;
        bool options            = false;
        bool transform_position = false;
        bool transform_rotation = false;
        bool transform_scale    = false;
        bool selection          = false;
        bool probing            = false;
        bool attention_signals  = false;
    };

    EntityCallbacks(ObjectT*, EnableCallback);

    EnableCallback const& callbacks_enabled() const;

    virtual void        on_activate_str(QString);
    virtual void        on_activate_int(int);
    virtual QStringList get_activation_choices();


    virtual QStringList get_var_keys();
    virtual QCborArray  get_var_options(QString key = QString());
    virtual QCborValue  get_var_value(QString key = QString());
    virtual bool set_var_value(QCborValue value, QString key = QString());

    virtual void set_position(glm::vec3);
    virtual void set_rotation(glm::quat);
    virtual void set_scale(glm::vec3);

    virtual void select_region(glm::vec3 min, glm::vec3 max, SelAction select);
    virtual void
    select_sphere(glm::vec3 point, float distance, SelAction select);
    virtual void
    select_plane(glm::vec3 point, glm::vec3 normal, SelAction select);
    virtual void select_hull(std::span<glm::vec3 const> point_list,
                             std::span<int64_t const>   index_list,
                             SelAction                  select);

    virtual std::pair<QString, glm::vec3> probe_at(glm::vec3);

signals:
    // Issue these signals to emit attention getting
    void signal_attention_plain();
    void signal_attention_at(glm::vec3);
    void signal_attention_anno(glm::vec3, QString);

private:
    ObjectT*       m_host;
    EnableCallback m_enabled;
};

struct ObjectTextDefinition {
    QString text;
    QString font;
    float   height;
    float   width = -1;
};

struct ObjectWebpageDefinition {
    QUrl  url;
    float height;
    float width;
};

struct InstanceInfo {
    BufferViewTPtr             view;
    uint64_t                   stride = 0;
    std::optional<BoundingBox> instance_bb;
};

struct ObjectRenderableDefinition {
    MeshTPtr mesh;

    std::optional<InstanceInfo> instances;
};

using ObjectDefinition = std::variant<std::monostate,
                                      ObjectTextDefinition,
                                      ObjectWebpageDefinition,
                                      ObjectRenderableDefinition>;


struct ObjectData {
    QString                                   name;
    std::optional<ObjectTPtr>                 parent;
    std::optional<glm::mat4>                  transform;
    std::optional<ObjectDefinition>           definition;
    std::optional<QVector<LightTPtr>>         lights;
    std::optional<QVector<TableTPtr>>         tables;
    std::optional<QVector<PlotTPtr>>          plots;
    std::optional<QStringList>                tags;
    std::optional<QVector<MethodTPtr>>        method_list;
    std::optional<QVector<SignalTPtr>>        signal_list;
    std::optional<std::optional<BoundingBox>> influence;
    std::optional<bool>                       visible;

    std::function<std::unique_ptr<EntityCallbacks>(ObjectT*)> create_callbacks;
};

struct ObjectUpdateData {
    std::optional<ObjectTPtr>                 parent;
    std::optional<glm::mat4>                  transform;
    std::optional<ObjectDefinition>           definition;
    std::optional<QVector<LightTPtr>>         lights;
    std::optional<QVector<TableTPtr>>         tables;
    std::optional<QVector<PlotTPtr>>          plots;
    std::optional<QStringList>                tags;
    std::optional<QVector<MethodTPtr>>        method_list;
    std::optional<QVector<SignalTPtr>>        signal_list;
    std::optional<std::optional<BoundingBox>> influence;
    std::optional<bool>                       visible;
};

ObjectTPtr create_object(DocumentTPtrRef, ObjectData const&);

void             update_object(ObjectT*, ObjectUpdateData&);
void             update_object(ObjectTPtr, ObjectUpdateData&);
EntityCallbacks* get_callbacks_from(ObjectT*);

void issue_signal_direct(ObjectT*, SignalT*, QCborArray);
void issue_signal_direct(ObjectT*, QString const&, QCborArray);

// Other =======================================================================

template <class C, class S, class... Args>
void issue_signal(C* ctx, S const& s, Args&&... args) {
    return issue_signal_direct(
        ctx, s, convert_to_cbor_array(std::forward<Args>(args)...));
}

} // namespace noo
