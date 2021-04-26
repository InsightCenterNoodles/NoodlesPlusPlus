#include "server_interface.h"

#include "noodlesserver.h"
#include "noodlesstate.h"

#include <QDebug>

#include <fstream>

#include <glm/gtx/component_wise.hpp>

namespace noo {

static std::string_view exception_postfix(MethodException::Responsibility r) {
    switch (r) {
    case MethodException::UNKNOWN: return " (Unknown exception type)";
    case MethodException::SERVER:
        return " (Server exception: ask server library developer to fix this "
               "error)";
    case MethodException::APPLICATION:
        return " (Application exception: ask application developer to fix this "
               "error)";
    case MethodException::CLIENT:
        return " (Client exception: Please check the calling documentation and "
               "try again)";
    }
}

MethodException::MethodException(Responsibility r, std::string_view reason)
    : m_reason(reason) {
    m_reason += exception_postfix(r);
}

TableTPtr MethodContext::get_table() const {
    auto const* ptr_ptr = std::get_if<TableTPtr>(this);

    if (!ptr_ptr) return {};

    return *ptr_ptr;
}

ObjectTPtr MethodContext::get_object() const {
    auto const* ptr_ptr = std::get_if<ObjectTPtr>(this);

    if (!ptr_ptr) return {};

    return *ptr_ptr;
}

// Methods
MethodTPtr create_method(DocumentT* server, MethodData const& data) {
    if (!data.method_name.size()) {
        qWarning() << "No name given to method";
        return nullptr;
    }
    if (!data.code) {
        qWarning() << "No code attached to method" << data.method_name.c_str();
        return nullptr;
    }
    return server->method_list().provision_next(data);
}

MethodTPtr create_method(DocumentTPtrRef server, MethodData const& data) {
    return create_method(server.get(), data);
}


// Signals
SignalTPtr create_signal(DocumentT* server, SignalData const& data) {
    return server->signal_list().provision_next(data);
}

// Server
std::shared_ptr<ServerT> create_server(uint16_t port) {
    return std::make_shared<ServerT>(port);
}

// Document ====================================================================
DocumentTPtr get_document(ServerT* server) {
    return server->state()->document();
}

void update_document(DocumentTPtrRef doc, DocumentData const& data) {
    return doc->update(data);
}

// Buffer ======================================================================

template <class T>
struct is_optional : std::bool_constant<false> { };

template <class T>
struct is_optional<std::optional<T>> : std::bool_constant<true> { };

PackedMeshDataResult pack_mesh_to_vector(BufferMeshDataRef const& refs,
                                         std::vector<std::byte>&  bytes) {

    qDebug() << Q_FUNC_INFO;

    if (refs.triangles.empty() and refs.lines.empty()) return {};
    if (!refs.triangles.empty() and !refs.lines.empty()) return {};
    if (refs.positions.empty()) return {};

    size_t start_byte = bytes.size();

    qDebug() << "Starting at" << start_byte;

    PackedMeshDataResult ret;

    // if the lists are not equal in size...well we can just duplicate

    glm::vec3 extent_min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 extent_max = glm::vec3(std::numeric_limits<float>::lowest());

    for (auto const& p : refs.positions) {
        extent_min = glm::min(extent_min, p);
        extent_max = glm::max(extent_max, p);
    }

    ret.extent_min = extent_min;
    ret.extent_max = extent_max;

    qDebug() << "Mesh extents" << ret.extent_min.x << ret.extent_min.y
             << ret.extent_min.z << "|" << ret.extent_max.x << ret.extent_max.y
             << ret.extent_max.z;

    // compute cell size
    const size_t cell_byte_size =
        (refs.positions.empty() ? 0 : sizeof(glm::vec3)) +
        (refs.normals.empty() ? 0 : sizeof(glm::vec3)) +
        (refs.textures.empty() ? 0 : sizeof(glm::u16vec2)) +
        (refs.colors.empty() ? 0 : sizeof(glm::u8vec4));


    qDebug() << "Cell size" << cell_byte_size;

    auto const num_verts =
        std::max(refs.positions.size(),
                 std::max(refs.normals.size(),
                          std::max(refs.textures.size(), refs.colors.size())));

    qDebug() << "Num verts" << num_verts;


    {
        size_t const total_vertex_bytes = num_verts * cell_byte_size;

        qDebug() << "New vert byte range" << total_vertex_bytes;
        std::vector<std::byte> vertex_portion;
        vertex_portion.resize(total_vertex_bytes);

        std::byte* comp_start = vertex_portion.data();

        auto add_component = [&]<class T, class U>(std::span<T const> vector,
                                                   U& dest_res) {
            if (vector.empty()) return;

            std::byte* write_at = comp_start;

            for (auto& component : refs.positions) {
                assert((write_at - vertex_portion.data()) <
                       vertex_portion.size());
                memcpy(write_at, &component, sizeof(component));
                write_at += cell_byte_size;
            }

            static_assert(std::is_same_v<glm::vec3, T> or
                          std::is_same_v<glm::vec2, T> or
                          std::is_same_v<glm::u16vec2, T> or
                          std::is_same_v<glm::u8vec4, T>);

            PackedMeshDataResult::Ref* ref;

            if constexpr (is_optional<U>::value) {
                ref = &(dest_res.emplace());
            } else {
                ref = &dest_res;
            }


            ref->start  = (comp_start - vertex_portion.data()) + start_byte;
            ref->size   = vertex_portion.size();
            ref->stride = cell_byte_size;

            comp_start += sizeof(T);

            qDebug() << "Add comp" << typeid(T).name() << ref->start
                     << ref->size << ref->stride;
        };

        add_component(refs.positions, ret.positions);
        add_component(refs.normals, ret.normals);
        add_component(refs.textures, ret.textures);
        add_component(refs.colors, ret.colors);

        bytes.insert(bytes.end(), vertex_portion.begin(), vertex_portion.end());
    }

    // reset to next append...
    start_byte = bytes.size();

    PackedMeshDataResult::Ref index_ref;
    index_ref.start = bytes.size();

    std::span<std::byte const> index_copy_from;

    if (!refs.lines.empty()) {
        qDebug() << "Line segs" << refs.lines.size();
        index_copy_from = std::as_bytes(refs.lines);

        index_ref.size   = index_copy_from.size();
        index_ref.stride = sizeof(decltype(refs.lines)::value_type);

        ret.lines = index_ref;

    } else if (!refs.triangles.empty()) {
        qDebug() << "Triangles" << refs.triangles.size();
        index_copy_from = std::as_bytes(refs.triangles);

        index_ref.size   = index_copy_from.size();
        index_ref.stride = sizeof(decltype(refs.triangles)::value_type);

        ret.triangles = index_ref;
    }

    bytes.insert(bytes.end(), index_copy_from.begin(), index_copy_from.end());

    return ret;
}

PackedMeshDataResult::Ref
pack_image_to_vector(std::filesystem::path const& path,
                     std::vector<std::byte>&      out_bytes) {

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);

    if (!ifs.good()) return {};

    auto size = ifs.tellg();


    PackedMeshDataResult::Ref ret;
    ret.start  = out_bytes.size();
    ret.stride = 1;
    ret.size   = size;

    std::vector<char> buffer(size);

    ifs.read(buffer.data(), size);

    auto sp = std::as_bytes(std::span(buffer));

    out_bytes.insert(out_bytes.end(), sp.begin(), sp.end());

    return ret;
}


BufferTPtr create_buffer(DocumentTPtrRef doc, BufferData data) {
    return doc->buffer_list().provision_next(data);
}

// Texture =====================================================================
TextureTPtr create_texture(DocumentTPtrRef doc, TextureData const& data) {
    return doc->tex_list().provision_next(data);
}

TextureTPtr create_texture_from_file(DocumentTPtrRef      doc,
                                     std::span<std::byte> data) {
    auto new_buffer = create_buffer(doc, BufferCopySource { .to_copy = data });

    TextureData td { .buffer = new_buffer, .start = 0, .size = data.size() };

    return create_texture(doc, td);
}

void update_texture(TextureTPtr item, TextureData const& data) {
    item->update(data);
}


// Material ====================================================================

MaterialTPtr create_material(DocumentTPtrRef doc, MaterialData const& data) {
    return doc->mat_list().provision_next(data);
}

void update_material(MaterialTPtr item, MaterialData const& data) {
    item->update(data);
}

// Mesh ========================================================================

MeshData::MeshData(PackedMeshDataResult const& res, BufferTPtr ptr) {
    auto set_from = [&ptr](auto const& src, auto& out) {
        qDebug() << "HERE";
        if (!src) return;
        auto& l  = *src;
        auto& d  = out.emplace();
        d.buffer = ptr;
        d.start  = l.start;
        d.size   = l.size;
        d.stride = l.stride;

        qDebug() << d.start << d.stride << d.size;
    };

    extent_min = res.extent_min;
    extent_max = res.extent_max;

    positions.buffer = ptr;
    positions.start  = res.positions.start;
    positions.size   = res.positions.size;
    positions.stride = res.positions.stride;

    set_from(res.normals, normals);
    set_from(res.textures, textures);
    set_from(res.colors, colors);
    set_from(res.lines, lines);
    set_from(res.triangles, triangles);
}

MeshTPtr create_mesh(DocumentTPtrRef doc, MeshData const& data) {
    return doc->mesh_list().provision_next(data);
}

MeshTPtr create_mesh(DocumentTPtrRef doc, BufferMeshDataRef const& ref) {
    std::vector<std::byte> bytes;

    auto result = pack_mesh_to_vector(ref, bytes);

    BufferData bd = BufferCopySource { .to_copy = bytes };

    auto buffer = create_buffer(doc, bd);

    auto md = MeshData(result, buffer);

    return create_mesh(doc, md);
}

void update_mesh(MeshT* item, MeshData const& data) {
    item->update(data);
}


// Table =======================================================================

TableQuery::~TableQuery() = default;

size_t TableQuery::next() const {
    return 0;
}

void TableQuery::write_next_to(std::span<double>) {
    // do nothing
}


TableSource::~TableSource() = default;

std::vector<std::string> TableSource::get_columns() {
    return {};
}

size_t TableSource::get_num_rows() {
    return 0;
}

QueryPtr TableSource::get_row(int64_t /*row*/, std::span<int64_t> /*columns*/) {
    return nullptr;
}

QueryPtr TableSource::get_block(std::pair<int64_t, int64_t> /*rows*/,
                                std::span<int64_t> /*columns*/) {
    return nullptr;
}

QueryPtr TableSource::get_selection_data(std::string_view) {
    return nullptr;
}

bool TableSource::request_row_insert(int64_t /*row*/,
                                     std::span<double> /*data*/) {
    return false;
}
bool TableSource::request_row_update(int64_t /*row*/,
                                     std::span<double> /*data*/) {
    return false;
}
bool TableSource::request_row_append(std::span<std::span<double>>) {
    return false;
}
bool TableSource::request_deletion(Selection const&) {
    return false;
}

bool TableSource::request_reset() {
    return false;
}

SelectionRef TableSource::get_selection(std::string_view) {
    return SelectionRef();
}
bool TableSource::request_set_selection(std::string_view, SelectionRef) {
    return false;
}

std::vector<std::string> TableSource::get_all_selections() {
    return {};
}

bool TableSource::trigger_set_selection(std::string_view selection_id,
                                        SelectionRef     r) {
    auto b = request_set_selection(selection_id, r);

    if (b) { emit table_selection_updated(std::string(selection_id)); }

    return b;
}

bool TableSource::trigger_row_insert(int64_t row, std::span<double> data) {
    auto b = request_row_insert(row, data);

    if (b) { emit table_row_added(row, 1); }

    return b;
}

bool TableSource::trigger_row_update(int64_t row, std::span<double> data) {
    auto b = request_row_update(row, data);

    if (b) {
        Selection s;

        s.rows = { row };

        emit table_row_updated(s);
    }

    return b;
}

bool TableSource::trigger_row_append(std::span<std::span<double>> rows) {
    auto b = request_row_append(rows);

    if (b) { emit table_row_added(-1, rows.size()); }

    return b;
}

bool TableSource::trigger_deletion(Selection const& s) {
    auto b = request_deletion(s);

    if (b) { emit table_row_deleted(s); }

    return b;
}

bool TableSource::trigger_reset() {
    auto b = request_reset();

    if (b) { emit sig_reset(); }

    return b;
}


TableTPtr create_table(DocumentTPtrRef doc, TableData const& data) {
    return doc->table_list().provision_next(data);
}

// Object ======================================================================

std::shared_ptr<ObjectT> create_object(DocumentTPtrRef   doc,
                                       ObjectData const& data) {
    return doc->obj_list().provision_next(data);
}

void update_object(ObjectTPtr item, ObjectUpdateData& data) {
    item->update(data);
}

void                     ObjectCallbacks::on_activate_str(std::string) { }
void                     ObjectCallbacks::on_activate_int(int) { }
std::vector<std::string> ObjectCallbacks::get_activation_choices() {
    return {};
}

std::vector<std::string> ObjectCallbacks::get_option_choices() {
    return {};
}
std::string ObjectCallbacks::get_current_option() {
    return {};
}
void ObjectCallbacks::set_current_option(std::string) { }

void ObjectCallbacks::set_position(glm::vec3) { }
void ObjectCallbacks::set_rotation(glm::quat) { }
void ObjectCallbacks::set_scale(glm::vec3) { }

void ObjectCallbacks::select_region(glm::vec3 /*min*/,
                                    glm::vec3 /*max*/,
                                    SelAction /*select*/) { }
void ObjectCallbacks::select_sphere(glm::vec3 /*point*/,
                                    float /*distance*/,
                                    SelAction /*select*/) { }
void ObjectCallbacks::select_plane(glm::vec3 /*point*/,
                                   glm::vec3 /*normal*/,
                                   SelAction /*select*/) { }

std::pair<glm::vec3, std::string> ObjectCallbacks::probe_at(glm::vec3) {
    return {};
}


// Other =======================================================================

void issue_signal_direct(DocumentT* doc, SignalT* signal, AnyVarList var) {
    if (!doc or !signal) return;

    if (!doc->att_signal_list().has(signal)) return;

    signal->fire({}, std::move(var));
}

void issue_signal_direct(DocumentT*         doc,
                         std::string const& signal,
                         AnyVarList         var) {
    auto* sig = doc->att_signal_list().find_by_name(signal);
    return issue_signal_direct(doc, sig, std::move(var));
}

void issue_signal_direct(TableT* tbl, SignalT* signal, AnyVarList var) {
    if (!tbl or !signal) return;

    if (!tbl->att_signal_list().has(signal)) return;

    signal->fire(tbl->id(), std::move(var));
}

void issue_signal_direct(TableT*            tbl,
                         std::string const& signal,
                         AnyVarList         var) {

    auto* sig = tbl->att_signal_list().find_by_name(signal);
    return issue_signal_direct(tbl, sig, std::move(var));
}

void issue_signal_direct(ObjectT* obj, SignalT* signal, AnyVarList var) {
    if (!obj or !signal) return;

    if (!obj->att_signal_list().has(signal)) return;

    signal->fire(obj->id(), std::move(var));
}

void issue_signal_direct(ObjectT*           obj,
                         std::string const& signal,
                         AnyVarList         var) {

    auto* sig = obj->att_signal_list().find_by_name(signal);
    return issue_signal_direct(obj, sig, std::move(var));
}

} // namespace noo
