#ifndef NOO_SERVER_INTERFACE_H
#define NOO_SERVER_INTERFACE_H

#include "noo_common.h"
#include "noo_include_glm.h"
#include "noo_interface_types.h"

#include <QColor>
#include <QObject>
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
    convert_from_cbor(at_i, ret);
    return ret;

    //    if constexpr (std::is_same_v<T, int64_t>) {
    //        return at_i.to_int();
    //    } else if constexpr (std::is_same_v<T, double>) {
    //        return at_i.to_real();
    //    } else if constexpr (std::is_same_v<T, std::string_view>) {
    //        return at_i.to_string();
    //    } else if constexpr (std::is_same_v<T, QCborArray>) {
    //        return at_i.to_vector();
    //    } else if constexpr (std::is_same_v<T, std::span<std::byte const>>) {
    //        return at_i.to_data();
    //    } else if constexpr (std::is_same_v<T, QCborValueMapRef>) {
    //        return at_i.to_map();
    //    } else if constexpr (std::is_same_v<T, std::span<double const>>) {
    //        return at_i.to_real_list();
    //    } else if constexpr (std::is_same_v<T, std::span<int64_t const>>) {
    //        return at_i.to_int_list();
    //    } else if constexpr (std::is_same_v<T, AnyID>) {
    //        return at_i.to_id();
    //    } else if constexpr (std::is_same_v<T, QCborValue>) {
    //        return at_i;
    //    } else {
    //        return T(at_i);
    //    }
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
    int         m_code;
    std::string m_reason;
    QCborValue  m_data;

public:
    MethodException(int code, std::string_view message, QCborValue data = {});

    int               code() const { return m_code; }
    std::string_view  reason() const { return m_reason; }
    QCborValue const& data() const { return m_data; }

    char const* what() const noexcept override;

    std::string to_string() const;
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
    std::string name;
    std::string documentation;
    std::string hint;
    std::string editor_hint;
};

///
/// \brief The MethodData struct defines a noodles method.
///
struct MethodData {
    std::string      method_name;
    std::string      documentation;
    std::string      return_documentation;
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
    std::string      signal_name;
    std::string      documentation;
    std::vector<Arg> argument_documentation;
};

class SignalT;
using SignalTPtr = std::shared_ptr<SignalT>;

/// Create a new signal.
SignalTPtr create_signal(DocumentTPtrRef, SignalData const&);
SignalTPtr create_signal(DocumentT*, SignalData const&);

// Server ======================================================================

struct ServerOptions {
    uint16_t port = 50000;
};

/// Create a new server, which uses a WebSocket to listen on the given port.
std::shared_ptr<ServerT> create_server(ServerOptions const&);

// Document ====================================================================

/// Get the document of a server.
std::shared_ptr<DocumentT> get_document(ServerT*);

///
/// \brief The DocumentData struct is used to update the noodles document
///
struct DocumentData {
    std::vector<MethodTPtr> method_list;
    std::vector<SignalTPtr> signal_list;
};

/// Update the document with new methods and signals
void update_document(DocumentTPtrRef, DocumentData const&);

/// Issue a signal (by name or by object), on this document.
void issue_signal_direct(DocumentT*, SignalT*, QCborArray);
void issue_signal_direct(DocumentT*, std::string const&, QCborArray);

// Buffer ======================================================================

/// Instruct the buffer system to own the given bytes
struct BufferOwningSource {
    QByteArray to_move;
};

/// Instruct the buffer system to reference the URL for a buffer
struct BufferURLSource {
    QUrl   url_source;
    size_t source_byte_size;
};

struct BufferData {

    std::string name;

    bool force_inline = false;

    std::variant<BufferOwningSource, BufferURLSource> source;
};

class BufferT;
using BufferTPtr = std::shared_ptr<BufferT>;

/// Create a new buffer
BufferTPtr create_buffer(DocumentTPtrRef, BufferData const&);

// Buffer ======================================================================

enum class ViewType : uint8_t {
    UNKNOWN,
    GEOMETRY_INFO,
    IMAGE_INFO,
};

struct BufferViewData {
    std::string name;

    BufferTPtr source_buffer;

    ViewType type   = ViewType::UNKNOWN;
    uint64_t offset = 0;
    uint64_t length = 0;
};

class BufferViewT;
using BufferViewTPtr = std::shared_ptr<BufferViewT>;

/// Create a new view
BufferViewTPtr create_bufferview(DocumentTPtrRef, BufferViewData const&);

// Image =======================================================================

struct ImageData {
    std::string name;

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

struct SamplerData {
    std::string name;

    MagFilter mag_filter;
    MinFilter min_filter;

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
    std::string name;
    ImageTPtr   image;   // may not be blank.
    SamplerTPtr sampler; // may be blank.
};

class TextureT;
using TextureTPtr = std::shared_ptr<TextureT>;

/// Create a new texture.
TextureTPtr create_texture(DocumentTPtrRef, TextureData const&);

/// Create a new texture from a span of bytes. Bytes should be the disk
/// representation of an image, A new buffer will be automatically created.
TextureTPtr create_texture_from_file(DocumentTPtrRef, std::span<std::byte>);

/// Update a texture with a new byte range.
void update_texture(TextureTPtr, TextureData const&);


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
    std::string               name;
    std::optional<PBRInfo>    pbr_info;
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
    std::string name;
    QColor      color     = Qt::white;
    float       intensity = 1;

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
    BufferViewTPtr    view;
    AttributeSemantic semantic;
    uint8_t           channel = 0;

    uint64_t stride = 0;
    Format   format;

    glm::vec4 minimum_value;
    glm::vec4 maximum_value;

    bool normalized = false;
};

struct Index {
    BufferViewTPtr view;

    uint64_t stride = 0;
    Format   format;
};

struct MeshPatch {
    std::vector<Attribute> attributes;

    Index indicies;

    PrimitiveType type;

    MaterialTPtr material;
};

///
/// \brief The MeshData struct defines a new mesh as a series of optional vertex
/// components
///
struct MeshData {
    std::string name;

    std::vector<MeshPatch> patches;
};

class MeshT;
using MeshTPtr = std::shared_ptr<MeshT>;

/// Create a new mesh.
MeshTPtr create_mesh(DocumentTPtrRef, MeshData const&);

/// Update a mesh
void update_mesh(MeshT*, MeshData const&);

// Geometry Construction =======================================================

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
struct BufferMeshDataRef {
    // Vertex data
    std::span<glm::vec3 const>    positions;
    std::span<glm::vec3 const>    normals;
    std::span<glm::u16vec2 const> textures;
    std::span<glm::u8vec4 const>  colors;

    // Index data
    std::span<glm::u16vec2 const> lines;
    std::span<glm::u16vec3 const> triangles;
};

///
/// \brief The PackedMeshDataResult struct provides the result of a pack
/// operation of mesh data. It can be used to help create a new mesh in a
/// buffer.
///
struct PackedMeshDataResult {
    BoundingBox bounding_box;

    struct Ref {
        size_t start  = 0;
        size_t size   = 0;
        size_t stride = 0;
    };

    Ref                positions;
    std::optional<Ref> normals;
    std::optional<Ref> textures;
    std::optional<Ref> colors;
    std::optional<Ref> lines;
    std::optional<Ref> triangles;
};

/// Take your mesh data and pack it into a byte buffer.
PackedMeshDataResult pack_mesh_to_vector(BufferMeshDataRef const&,
                                         std::vector<std::byte>&);


/// Take an image from disk and pack it into a byte buffer.
PackedMeshDataResult::Ref
pack_image_to_vector(std::filesystem::path const& path,
                     std::vector<std::byte>&);

// Plot ========================================================================

using PlotDef = std::variant<std::string, QUrl>;

struct PlotData {
    std::string             name;
    PlotDef                 definition;
    TableTPtr               table_link;
    std::vector<MethodTPtr> method_list;
    std::vector<SignalTPtr> signal_list;
};

struct PlotUpdateData {
    std::optional<PlotDef>                 definition;
    std::optional<TableTPtr>               table_link;
    std::optional<std::vector<MethodTPtr>> method_list;
    std::optional<std::vector<SignalTPtr>> signal_list;
};

/// Create a new plot.
MeshTPtr create_plot(DocumentTPtrRef, PlotData const&);

/// Update a plot.
void update_plot(DocumentTPtrRef, PlotUpdateData const&);

// Table =======================================================================

// The table system here is just preliminary

// sketch for better query...
struct TableQuery {
    size_t num_cols = 0;
    size_t num_rows = 0;

    virtual bool is_column_string(size_t col) const; // either real or string

    virtual bool get_reals_to(size_t col, std::span<double>) const;
    virtual bool get_cell_to(size_t col, size_t row, std::string_view&) const;

    virtual bool get_keys_to(std::span<int64_t>) const;
};

using TableQueryPtr = std::shared_ptr<TableQuery const>;


class TableColumn
    : public std::variant<std::vector<double>, std::vector<std::string>> {
public:
    std::string name;

    using variant::variant;

    size_t size() const;
    bool   is_string() const;

    std::span<double const>      as_doubles() const;
    std::span<std::string const> as_string() const;

    void append(std::span<double const>);
    void append(QCborArray const&);
    void append(double);
    void append(std::string_view);

    void set(size_t row, double);
    void set(size_t row, QCborValue);
    void set(size_t row, std::string_view);

    void erase(size_t row);

    void clear();
};

///
/// \brief The TableSource class is the base type for tables. Users should
/// inherit from this and override the functionality they desire.
///
class TableSource : public QObject {
    Q_OBJECT

protected:
    std::vector<TableColumn> m_columns;
    uint64_t                 m_counter = 0;

    std::unordered_map<int64_t, uint64_t> m_key_to_row_map;
    std::vector<int64_t>                  m_row_to_key_map;

    // how should selections handle key deletion?
    std::unordered_map<std::string, Selection> m_selections;

protected:
    virtual TableQueryPtr handle_insert(QCborArray const& cols);
    virtual TableQueryPtr handle_update(QCborValue const& keys,
                                        QCborArray const& cols);
    virtual TableQueryPtr handle_deletion(QCborValue const& keys);
    virtual bool          handle_reset();
    virtual bool handle_set_selection(std::string_view, SelectionRef const&);

public:
    TableSource(QObject* p) : QObject(p) { }
    virtual ~TableSource();

    std::vector<std::string> get_headers();
    TableQueryPtr            get_all_data();

    auto const& get_columns() const { return m_columns; }
    auto const& get_all_selections() const { return m_selections; }
    auto const& get_key_to_row_map() const { return m_key_to_row_map; }
    auto const& get_row_to_key_map() const { return m_row_to_key_map; }


    bool ask_insert(QCborArray const&); // list of lists
    bool ask_update(QCborValue const& keys,
                    QCborArray const&); // list of lists
    bool ask_delete(QCborValue const& keys);
    bool ask_clear();
    bool ask_update_selection(std::string_view, SelectionRef const&);

signals:
    void table_reset();
    void table_selection_updated(std::string, SelectionRef const&);
    void table_row_updated(TableQueryPtr);
    void table_row_deleted(TableQueryPtr);
};

///
/// \brief The TableData struct helps define a new table
///
struct TableData {
    std::string name;
    std::string meta;

    std::shared_ptr<TableSource> source;
};

/// Create a new table
TableTPtr create_table(DocumentTPtrRef, TableData const&);

void issue_signal_direct(TableT*, SignalT*, QCborArray);

// Object ======================================================================

class ObjectCallbacks : public QObject {
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

    ObjectCallbacks(ObjectT*, EnableCallback);

    EnableCallback const& callbacks_enabled() const;

    virtual void                     on_activate_str(std::string_view);
    virtual void                     on_activate_int(int);
    virtual std::vector<std::string> get_activation_choices();

    virtual std::vector<std::string> get_option_choices();
    virtual std::string              get_current_option();
    virtual void                     set_current_option(std::string_view);

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

    virtual std::pair<std::string, glm::vec3> probe_at(glm::vec3);

signals:
    // Issue these signals to emit attention getting
    void signal_attention_plain();
    void signal_attention_at(glm::vec3);
    void signal_attention_anno(glm::vec3, std::string);

private:
    ObjectT*       m_host;
    EnableCallback m_enabled;
};

struct ObjectTextDefinition {
    std::string text;
    std::string font;
    float       height;
    float       width = -1;
};

struct ObjectWebpageDefinition {
    QUrl  url;
    float height;
    float width;
};

struct ObjectRenderableDefinition {
    MaterialTPtr               material;
    MeshTPtr                   mesh;
    std::vector<glm::mat4>     instances;
    std::optional<BoundingBox> instance_bb;
};

using ObjectDefinition = std::variant<std::monostate,
                                      ObjectTextDefinition,
                                      ObjectWebpageDefinition,
                                      ObjectRenderableDefinition>;


struct ObjectData {
    ObjectTPtr                 parent;
    std::string                name;
    glm::mat4                  transform = glm::mat4(1);
    ObjectDefinition           definition;
    std::vector<LightTPtr>     lights;
    std::vector<TableTPtr>     tables;
    std::vector<PlotTPtr>      plots;
    std::vector<std::string>   tags;
    std::vector<MethodTPtr>    method_list;
    std::vector<SignalTPtr>    signal_list;
    std::optional<BoundingBox> influence;
    bool                       visibility = true;

    std::function<std::unique_ptr<ObjectCallbacks>(ObjectT*)> create_callbacks;
};

struct ObjectUpdateData {
    std::optional<ObjectTPtr>                 parent;
    std::optional<std::string>                name;
    std::optional<glm::mat4>                  transform;
    std::optional<ObjectDefinition>           definition;
    std::optional<std::vector<LightTPtr>>     lights;
    std::optional<std::vector<TableTPtr>>     tables;
    std::optional<std::vector<PlotTPtr>>      plots;
    std::optional<std::vector<std::string>>   tags;
    std::optional<std::vector<MethodTPtr>>    method_list;
    std::optional<std::vector<SignalTPtr>>    signal_list;
    std::optional<std::optional<BoundingBox>> influence;
    std::optional<bool>                       visibility;
};

ObjectTPtr create_object(DocumentTPtrRef, ObjectData const&);

void             update_object(ObjectT*, ObjectUpdateData&);
void             update_object(ObjectTPtr, ObjectUpdateData&);
ObjectCallbacks* get_callbacks_from(ObjectT*);

void issue_signal_direct(ObjectT*, SignalT*, QCborArray);
void issue_signal_direct(ObjectT*, std::string const&, QCborArray);

// Other =======================================================================

template <class C, class S, class... Args>
void issue_signal(C* ctx, S const& s, Args&&... args) {
    return issue_signal_direct(
        ctx, s, marshall_to_any(std::forward<Args>(args)...));
}

} // namespace noo

#endif // INTERFACE_H
