#ifndef NOO_SERVER_INTERFACE_H
#define NOO_SERVER_INTERFACE_H

#include "include/noo_common.h"
#include "include/noo_include_glm.h"
#include "include/noo_interface_types.h"

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

// We mirror a lot of the noodles objects in order to hide deps.
// We can revisit this later.

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

// =============================================================================

struct MethodContext;

template <class T>
T _any_call_getter(AnyVarList& source, size_t& loc) {
    auto at_i = steal_or_default(source, loc);
    loc++;

    if constexpr (!is_in_variant<T, AnyVarBase>::value) {

        if constexpr (is_vector<T>::value) {
            // extract a vector
            auto nv = at_i.steal_vector();

            T ret;

            for (auto& local_a : nv) {
                ret.emplace_back(std::move(local_a));
            }

            return ret;

        } else {
            return T(std::move(at_i));
        }

    } else {
        T* c = std::get_if<T>(&at_i);
        if (c) return std::move(*c);
        return T();
    }
}


template <class Func, class... Args>
auto _call(Func&& f, MethodContext const& c, AnyVarList& source) {
    size_t i = 0;
    return f(c, std::move(_any_call_getter<Args>(source, i))...);
}

template <class Lambda>
struct AnyCallHelper : AnyCallHelper<decltype(&Lambda::operator())> { };

template <class R, class... Args>
struct AnyCallHelper<R(MethodContext const&, Args...)> {
    template <class Func>
    static auto call(Func&& f, MethodContext const& c, AnyVarList& source) {
        return _call<Func, Args...>(f, c, source);
    }
};

template <class R, class... Args>
struct AnyCallHelper<R (*)(MethodContext const&, Args...)> {
    template <class Func>
    static auto call(Func&& f, MethodContext const& c, AnyVarList& source) {
        return _call<Func, Args...>(f, c, source);
    }
};

template <class R, class C, class... Args>
struct AnyCallHelper<R (C::*)(MethodContext const&, Args...)> {
    template <class Func>
    static auto call(Func&& f, MethodContext const& c, AnyVarList& source) {
        return _call<Func, Args...>(f, c, source);
    }
};

template <class R, class C, class... Args>
struct AnyCallHelper<R (C::*)(MethodContext const&, Args...) const> {
    template <class Func>
    static auto call(Func&& f, MethodContext const& c, AnyVarList& source) {
        return _call<Func, Args...>(f, c, source);
    }
};

///
/// Take a list of Any vars, decode them to compatible arguments for a given
/// function, and then call that function.
///
template <class Func>
auto any_call_helper(Func&& f, MethodContext const& c, AnyVarList& source) {
    using FType = std::remove_cvref_t<Func>;
    return AnyCallHelper<FType>::call(f, c, source);
}

/// \brief The base exception type for methods.
///
/// This should only be thrown inside code that handles method requests, and is
/// used to communicate disappointment to calling clients.
struct MethodException : public std::exception {
    std::string m_reason;

public:
    enum Responsibility {
        UNKNOWN,
        SERVER,      // i.e. the library writer needs to fix this
        APPLICATION, // i.e. you called it correctly, but we cant do that
        CLIENT       // i.e. you didnt call this correctly
    };

    MethodException(std::string_view reason)
        : MethodException(CLIENT, reason) { }

    MethodException(Responsibility r, std::string_view reason);

    char const* what() const noexcept override { return m_reason.c_str(); }

    std::string const& reason() const { return m_reason; }
};

// Methods =====================================================================

///
/// \brief The MethodContext struct helps method code know which component the
/// method is being called on.
///
struct MethodContext
    : public std::variant<std::monostate, TableTPtr, ObjectTPtr> {
    using variant::variant;

    /// Used to track the calling client. Internal ONLY.
    ClientT* client = nullptr;

    TableTPtr  get_table() const;
    ObjectTPtr get_object() const;
};

struct Arg {
    std::string name;
    std::string doc;
};

///
/// \brief The MethodData struct defines a noodles method.
///
struct MethodData {
    std::string      method_name;
    std::string      documentation;
    std::string      return_documentation;
    std::vector<Arg> argument_documentation;

    std::function<AnyVar(MethodContext const&, AnyVarList&)> code;

    /// Set the code to be called when the method is invoked. The function f can
    /// be any function and the parameters will be decoded. See noo::RealListArg
    /// as an example of constraining what parameters you want Anys to be
    /// decoded to.
    template <class Function>
    void set_code(Function&& f) {
        code = [lf = std::move(f)](MethodContext const& c, AnyVarList& v) {
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

/// Create a new server, which uses a WebSocket to listen on the given port.
std::shared_ptr<ServerT> create_server(uint16_t port);

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

void issue_signal_direct(DocumentT*, SignalT*, AnyVarList);
void issue_signal_direct(DocumentT*, std::string const&, AnyVarList);

// Buffer ======================================================================

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
    glm::vec3 extent_min;
    glm::vec3 extent_max;

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

/// Instruct the buffer system to copy the given bytes
struct BufferCopySource {
    std::span<std::byte const> to_copy;
};

/// Instruct the buffer system to reference the URL for a buffer
struct BufferURLSource {
    QUrl   url_source;
    size_t source_byte_size;
};

struct BufferData : std::variant<BufferCopySource, BufferURLSource> {
    using variant::variant;
};

class BufferT;
using BufferTPtr = std::shared_ptr<BufferT>;

/// Create a new buffer
BufferTPtr create_buffer(DocumentTPtrRef, BufferData);

// Texture =====================================================================

///
/// \brief The TextureData struct describes a new texture as a byte range of a
/// given buffer.
///
struct TextureData {
    BufferTPtr buffer;
    size_t     start;
    size_t     size;
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

///
/// \brief The MaterialData struct defines a new material.
///
struct MaterialData {
    glm::vec4   color        = { 1, 1, 1, 1 };
    float       metallic     = 0;
    float       roughness    = 1;
    bool        use_blending = false;
    TextureTPtr texture;
};

class MaterialT;
using MaterialTPtr = std::shared_ptr<MaterialT>;

/// Create a new material
MaterialTPtr create_material(DocumentTPtrRef, MaterialData const&);

/// Update a material
void update_material(MaterialTPtr, MaterialData const&);

// Light =======================================================================

///
/// \brief The LightData struct defines a new light
///
struct LightData {
    glm::vec3 color     = { 1, 1, 1 };
    float     intensity = 0;
};

class LightT;
using LightTPtr = std::shared_ptr<LightT>;

/// Create a new light
LightTPtr create_light(DocumentTPtrRef, LightData const&);

/// Update a light
void update_light(LightTPtr const&, LightData const&);

// Mesh ========================================================================

///
/// \brief The ComponentRef struct models a vertex component as a range of
/// bytes, as well as a stride between those components.
///
struct ComponentRef {
    BufferTPtr buffer;
    size_t     start  = 0;
    size_t     size   = 0;
    size_t     stride = 0;
};

///
/// \brief The MeshData struct defines a new mesh as a series of optional vertex
/// components
///
struct MeshData {
    glm::vec3 extent_min;
    glm::vec3 extent_max;

    ComponentRef                positions;
    std::optional<ComponentRef> normals;
    std::optional<ComponentRef> textures;
    std::optional<ComponentRef> colors;

    std::optional<ComponentRef> lines;
    std::optional<ComponentRef> triangles;

    MeshData() = default;

    /// Construct new data using a packed mesh result and assuming that all the
    /// data resides in the given buffer.
    MeshData(PackedMeshDataResult const&, BufferTPtr);
};

class MeshT;
using MeshTPtr = std::shared_ptr<MeshT>;

/// Create a new mesh.
MeshTPtr create_mesh(DocumentTPtrRef, MeshData const&);

/// Create a new mesh from a user-supplied list of raw components. A new buffer
/// is created under the hood.
MeshTPtr create_mesh(DocumentTPtrRef, BufferMeshDataRef const&);

/// Update a mesh
void update_mesh(MeshT*, MeshData const&);

// Table =======================================================================

struct TableQuery {
    virtual ~TableQuery();

    /// Ask how many reals are waiting to be written.
    virtual size_t next() const;

    /// Write available reals to the given span.
    virtual void write_next_to(std::span<double>);
};

using QueryPtr = std::unique_ptr<TableQuery>;

///
/// \brief The TableSource class is the base type for tables. Users should
/// inherit from this and override the functionality they desire.
///
class TableSource : public QObject {
    Q_OBJECT


protected:
    /// Request a selection be updated. If you return true, a signal will be
    /// emitted
    virtual bool request_set_selection(std::string_view selection_id,
                                       SelectionRef const&);

    virtual bool request_row_insert(int64_t row, std::span<double> data);
    virtual bool request_row_update(int64_t row, std::span<double> data);
    virtual bool request_row_append(std::span<std::span<double>>);
    virtual bool request_deletion(Selection const&);

    virtual bool request_reset();

public:
    virtual ~TableSource();

    /// Provide the names of your table columns
    virtual std::vector<std::string> get_headers();
    virtual QueryPtr                 get_all_rows();
    // virtual std::vector<std::pair<std::string, >>

    /// Get a row
    virtual QueryPtr get_row(int64_t row, std::span<int64_t> columns);

    /// Get a block
    virtual QueryPtr get_block(std::pair<int64_t, int64_t> rows,
                               std::span<int64_t>          columns);

    /// Get the data from a selection
    virtual QueryPtr get_selection_data(std::string_view selection_id);

    /// Get selection information
    virtual SelectionRef get_selection(std::string_view selection_id);

    /// Get all available selections
    virtual std::vector<std::string> get_all_selections();

    // These will automatically call the virtual versions of the function; if
    // successfull they will also emit signals
    bool trigger_set_selection(std::string_view selection_id,
                               SelectionRef const&);
    bool trigger_row_insert(int64_t row, std::span<double> data);
    bool trigger_row_update(int64_t row, std::span<double> data);
    bool trigger_row_append(std::span<std::span<double>>);
    bool trigger_deletion(Selection const&);

    bool trigger_reset();

signals:
    void table_selection_updated(std::string);
    void table_row_added(int64_t at, int64_t count);
    void table_row_deleted(Selection);
    void table_row_updated(Selection);
    void sig_reset();
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

void issue_signal_direct(TableT*, SignalT*, AnyVarList);

// Object ======================================================================

class ObjectCallbacks : public QObject {
    Q_OBJECT

    ObjectT* m_host;

protected:
    ObjectT* get_host();

public:
    enum SelAction : uint8_t {
        DESELECT,
        SELECT,
    };

    explicit ObjectCallbacks(ObjectT*);

    virtual void                     on_activate_str(std::string);
    virtual void                     on_activate_int(int);
    virtual std::vector<std::string> get_activation_choices();

    virtual std::vector<std::string> get_option_choices();
    virtual std::string              get_current_option();
    virtual void                     set_current_option(std::string);

    virtual void set_position(glm::vec3);
    virtual void set_rotation(glm::quat);
    virtual void set_scale(glm::vec3);

    virtual void select_region(glm::vec3 min, glm::vec3 max, SelAction select);
    virtual void
    select_sphere(glm::vec3 point, float distance, SelAction select);
    virtual void
    select_plane(glm::vec3 point, glm::vec3 normal, SelAction select);

    virtual std::pair<glm::vec3, std::string> probe_at(glm::vec3);

signals:

    // User signals
    void signal_attention_plain();
    void signal_attention_at(glm::vec3);
    void signal_attention_anno(glm::vec3, std::string);
};

struct ObjectTextDefinition {
    std::string text;
    std::string font;
    float       height;
    float       width = -1;
};

struct ObjectData {
    ObjectTPtr                          parent;
    std::string                         name;
    glm::mat4                           transform = glm::mat4(1);
    MaterialTPtr                        material;
    MeshTPtr                            mesh;
    std::vector<LightTPtr>              lights;
    std::vector<TableTPtr>              tables;
    std::vector<glm::mat4>              instances;
    std::vector<std::string>            tags;
    std::vector<MethodTPtr>             method_list;
    std::vector<SignalTPtr>             signal_list;
    std::optional<ObjectTextDefinition> text;

    std::function<std::unique_ptr<ObjectCallbacks>(ObjectT*)> create_callbacks;
};

struct ObjectUpdateData {
    std::optional<ObjectTPtr>               parent;
    std::optional<std::string>              name;
    std::optional<glm::mat4>                transform;
    std::optional<MaterialTPtr>             material;
    std::optional<MeshTPtr>                 mesh;
    std::optional<std::vector<LightTPtr>>   lights;
    std::optional<std::vector<TableTPtr>>   tables;
    std::optional<std::vector<glm::mat4>>   instances;
    std::optional<std::vector<std::string>> tags;
    std::optional<std::vector<MethodTPtr>>  method_list;
    std::optional<std::vector<SignalTPtr>>  signal_list;
    std::optional<ObjectTextDefinition>     text;
};

ObjectTPtr create_object(DocumentTPtrRef, ObjectData const&);

void             update_object(ObjectT*, ObjectUpdateData&);
void             update_object(ObjectTPtr, ObjectUpdateData&);
ObjectCallbacks* get_callbacks_from(ObjectT*);

void issue_signal_direct(ObjectT*, SignalT*, AnyVarList);
void issue_signal_direct(ObjectT*, std::string const&, AnyVarList);

// Other =======================================================================

template <class C, class S, class... Args>
void issue_signal(C* ctx, S const& s, Args&&... args) {
    return issue_signal_direct(
        ctx, s, marshall_to_any(std::forward<Args>(args)...));
}

} // namespace noo

#endif // INTERFACE_H
