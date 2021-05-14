#include "noo_server_interface.h"

#include "include/noo_include_glm.h"
#include "src/common/variant_tools.h"
#include "src/server/noodlesserver.h"
#include "src/server/noodlesstate.h"

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

// TableQuery::~TableQuery() = default;

// size_t TableQuery::next() const {
//    return 0;
//}

// void TableQuery::write_next_to(std::span<double>) {
//    // do nothing
//}

bool TableQuery::is_column_string(size_t) const {
    return false;
}

bool TableQuery::get_reals_to(size_t, std::span<double>) const {
    return false;
}

bool TableQuery::get_cell_to(size_t, size_t, std::string_view&) const {
    return false;
}

bool TableQuery::get_keys_to(std::span<int64_t>) const {
    return false;
}

// =============

size_t TableColumn::size() const {
    return std::visit([&](auto const& a) { return a.size(); }, *this);
}

bool TableColumn::is_string() const {
    return std::holds_alternative<std::vector<std::string>>(*this);
}

std::span<double const> TableColumn::as_doubles() const {
    auto* p = std::get_if<std::vector<double>>(this);

    return p ? std::span<double const>(*p) : std::span<double const> {};
}
std::span<std::string const> TableColumn::as_string() const {
    auto* p = std::get_if<std::vector<std::string>>(this);

    return p ? std::span<std::string const>(*p)
             : std::span<std::string const> {};
}

void TableColumn::append(std::span<double const> d) {
    VMATCH(
        *this,
        VCASE(std::vector<double> & a) {
            a.insert(a.end(), d.begin(), d.end());
        },
        VCASE(std::vector<std::string> & a) {
            for (auto value : d) {
                a.push_back(std::to_string(value));
            }
        });
}

void TableColumn::append(AnyVarListRef const& d) {
    VMATCH(
        *this,
        VCASE(std::vector<double> & a) {
            d.for_each(
                [&](auto, auto const& ref) { a.push_back(ref.to_real()); });
        },
        VCASE(std::vector<std::string> & a) {
            d.for_each([&](auto, auto const& ref) {
                a.push_back(std::string(ref.to_string()));
            });
        });
}

void TableColumn::set(size_t row, double d) {
    VMATCH(
        *this,
        VCASE(std::vector<double> & a) { a[row] = d; },
        VCASE(std::vector<std::string> & a) { a[row] = std::to_string(d); });
}
void TableColumn::set(size_t row, AnyVarRef d) {
    VMATCH(
        *this,
        VCASE(std::vector<double> & a) { a[row] = d.to_real(); },
        VCASE(std::vector<std::string> & a) {
            a[row] = std::string(d.to_string());
        });
}

void TableColumn::erase(size_t row) {
    std::visit([row](auto& a) { a.erase(a.begin() + row); }, *this);
}

void TableColumn::clear() {
    std::visit([](auto& a) { a.clear(); }, *this);
}

// =============

namespace {


struct WholeTableQuery : TableQuery {
    TableSource* source;

    WholeTableQuery(TableSource* s) : source(s) {
        num_cols = s->get_columns().size();
        num_rows = num_cols ? s->get_columns().at(0).size() : 0;
    }

    bool is_column_string(size_t col) const override {
        return source->get_columns().at(col).is_string();
    }

    bool get_reals_to(size_t col, std::span<double> dest) const override {
        auto& column = source->get_columns().at(col);

        if (column.is_string()) return false;

        auto sp = column.as_doubles();

        copy(sp.begin(), sp.end(), dest.begin(), dest.end());

        return true;
    }

    bool
    get_cell_to(size_t col, size_t row, std::string_view& s) const override {
        try {
            auto& column = source->get_columns().at(col);

            auto sp = column.as_string();

            if (row >= sp.size()) return false;

            s = std::string_view(sp[row]);

            return true;

        } catch (...) { return false; }
    }

    bool get_keys_to(std::span<int64_t> dest) const override {
        auto const& r = source->get_row_to_key_map();
        copy(r.begin(), r.end(), dest.begin(), dest.end());
        return true;
    }
};

struct InsertQuery : TableQuery {
    TableSource* source;

    size_t start_at = 0;

    InsertQuery(TableSource* s, size_t row_start, size_t count) : source(s) {
        num_cols = s->get_columns().size();
        num_rows = count;
        start_at = row_start;
    }

    bool is_column_string(size_t col) const override {
        return source->get_columns().at(col).is_string();
    }

    bool get_reals_to(size_t col, std::span<double> dest) const override {
        auto& column = source->get_columns().at(col);

        if (column.is_string()) return false;

        auto sp = column.as_doubles();

        copy(sp.begin() + start_at,
             sp.begin() + start_at + num_rows,
             dest.begin(),
             dest.end());

        return true;
    }

    bool
    get_cell_to(size_t col, size_t row, std::string_view& s) const override {
        try {
            auto& column = source->get_columns().at(col);

            auto sp = column.as_string();

            if (row >= sp.size()) return false;

            s = std::string_view(sp[row + start_at]);

            return true;

        } catch (...) { return false; }
    }

    bool get_keys_to(std::span<int64_t> dest) const override {
        auto const& r = source->get_row_to_key_map();
        copy(r.begin() + start_at,
             r.begin() + start_at + num_rows,
             dest.begin(),
             dest.end());
        return true;
    }
};

struct UpdateQuery : TableQuery {
    TableSource*         source;
    std::vector<int64_t> keys;


    UpdateQuery(TableSource* s, std::vector<int64_t> _keys)
        : source(s), keys(std::move(_keys)) {
        num_cols = s->get_columns().size();
        num_rows = keys.size();
    }

    bool is_column_string(size_t col) const override {
        return source->get_columns().at(col).is_string();
    }

    bool get_reals_to(size_t col, std::span<double> dest) const override {
        auto& column = source->get_columns().at(col);

        if (column.is_string()) return false;

        auto sp = column.as_doubles();

        auto& map = source->get_key_to_row_map();

        for (size_t i = 0; i < num_rows; i++) {
            auto key = keys[i];
            auto row = map.at(key);

            dest[i] = sp[row];
        }

        return true;
    }

    bool
    get_cell_to(size_t col, size_t row, std::string_view& s) const override {
        try {
            auto& column = source->get_columns().at(col);

            auto sp = column.as_string();

            if (row >= sp.size()) return false;

            auto& map = source->get_key_to_row_map();

            auto key        = keys[row];
            auto source_row = map.at(key);

            s = sp[source_row];

            return true;

        } catch (...) {
            qCritical() << Q_FUNC_INFO << "Internal error!";
            return false;
        }
    }

    bool get_keys_to(std::span<int64_t> dest) const override {
        copy(keys.begin(), keys.end(), dest.begin(), dest.end());
        return true;
    }
};

struct DeleteQuery : TableQuery {
    TableSource*         source;
    std::vector<int64_t> keys;


    DeleteQuery(TableSource* s, std::vector<int64_t> _keys)
        : source(s), keys(std::move(_keys)) {
        num_cols = s->get_columns().size();
        num_rows = keys.size();
    }

    bool is_column_string(size_t col) const override {
        return source->get_columns().at(col).is_string();
    }

    bool get_reals_to(size_t /*col*/, std::span<double>) const override {
        return false;
    }

    bool get_cell_to(size_t /*col*/,
                     size_t /*row*/,
                     std::string_view&) const override {
        return false;
    }

    bool get_keys_to(std::span<int64_t> dest) const override {
        qDebug() << Q_FUNC_INFO << dest.size() << keys.size();
        copy(keys.begin(), keys.end(), dest.begin(), dest.end());
        return true;
    }
};

} // namespace

TableQueryPtr TableSource::handle_insert(AnyVarListRef const& cols) {
    // get dimensions of insert

    size_t num_cols = cols.size();

    if (num_cols == 0) return nullptr;
    if (num_cols != m_columns.size()) return nullptr;

    size_t num_rows = 0;
    bool   ok       = true;

    for (size_t ci = 0; ci < num_cols; ci++) {
        auto r = cols[ci];
        switch (r.type()) {
        case AnyVarRef::AnyType::RealList:
            if (m_columns.at(ci).is_string()) ok = false;
            num_rows = std::max(num_rows, r.to_real_list().size());
            break;
        case AnyVarRef::AnyType::AnyList:
            num_rows = std::max(num_rows, r.to_vector().size());
            break;
        default: ok = false; break;
        }
    }

    if (!ok or num_rows == 0) return nullptr;

    qDebug() << Q_FUNC_INFO << "num rows" << num_rows;

    // lets get some keys

    std::vector<int64_t> new_row_keys;
    new_row_keys.resize(num_rows);

    auto const current_row_count = m_columns.at(0).size();

    qDebug() << "current row count" << current_row_count;

    for (size_t i = 0; i < num_rows; i++) {
        new_row_keys[i]             = m_counter;
        m_key_to_row_map[m_counter] = current_row_count + i;
        m_counter++;
    }

    m_row_to_key_map.insert(
        m_row_to_key_map.end(), new_row_keys.begin(), new_row_keys.end());

    qDebug() << "assigned keys"
             << QVector<int64_t>::fromStdVector(new_row_keys);

    // now lets insert

    for (size_t ci = 0; ci < num_cols; ci++) {
        auto source_col = cols[ci];
        auto dest_col   = m_columns.at(ci);
        VMATCH_W(
            visit,
            source_col,
            VCASE(std::span<double> data) { dest_col.append(data); },
            VCASE(AnyVarListRef const& ref) { dest_col.append(ref); },
            VCASE(auto const&) {
                // do nothing, should not get here
                qFatal("Unable to insert this data type");
            })

        if (dest_col.is_string()) {
            QVector<QString> sl;

            for (auto const& s : dest_col.as_string()) {
                sl.push_back(noo::to_qstring(s));
            }
            qDebug() << "Col " << ci << "is now" << sl;

        } else {
            QVector<double> sl;

            for (auto const& s : dest_col.as_doubles()) {
                sl.push_back(s);
            }
            qDebug() << "Col " << ci << "is now" << sl;
        }
    }

    // now return a query to the data

    return std::make_shared<InsertQuery>(this, current_row_count, num_rows);
}

TableQueryPtr TableSource::handle_update(AnyVarRef const&     keys,
                                         AnyVarListRef const& cols) {
    // get dimensions of update

    size_t num_cols = cols.size();

    if (num_cols == 0) return nullptr;
    if (num_cols != m_columns.size()) return nullptr;

    size_t num_rows = 0;
    bool   ok       = true;

    for (size_t ci = 0; ci < num_cols; ci++) {
        auto r = cols[ci];
        switch (r.type()) {
        case AnyVarRef::AnyType::RealList:
            if (m_columns.at(ci).is_string()) ok = false;
            num_rows = std::max(num_rows, r.to_real_list().size());
            break;
        case AnyVarRef::AnyType::AnyList:
            num_rows = std::max(num_rows, r.to_vector().size());
            break;
        default: ok = false; break;
        }
    }

    if (!ok or num_rows == 0) return nullptr;

    // lets get some keys

    auto key_list = keys.coerce_int_list();


    // now lets update

    for (auto key : key_list) {

        auto update_at = m_key_to_row_map[key];

        for (size_t ci = 0; ci < num_cols; ci++) {
            auto source_col = cols[ci];
            auto dest_col   = m_columns.at(ci);
            VMATCH_W(
                visit,
                source_col,
                VCASE(std::span<double> data) {
                    dest_col.set(update_at, data[ci]);
                },
                VCASE(AnyVarListRef const& ref) {
                    dest_col.set(update_at, ref[ci]);
                },
                VCASE(auto const&) {
                    // do nothing, should not get here
                    qFatal("Unable to insert this data type");
                })
        }
    }

    // now return a query to the data

    // UpdateQuery(this, span_to_vector(key_list.span()));

    return std::make_shared<UpdateQuery>(this, span_to_vector(key_list.span()));
}

TableQueryPtr TableSource::handle_deletion(AnyVarRef const& keys) {
    qDebug() << Q_FUNC_INFO;
    auto key_list = keys.coerce_int_list();

    qDebug() << QVector<int64_t>(key_list.begin(), key_list.end());

    // to delete we are finding the rows to remove, and then deleting the rows
    // from highest to lowest. this means we dont break index validity

    std::vector<size_t> rows_to_delete;

    for (auto k : key_list) {
        if (m_key_to_row_map.count(k) != 1) continue;

        rows_to_delete.push_back(m_key_to_row_map.at(k));

        m_key_to_row_map.erase(k);
    }

    qDebug() << "Rows to delete"
             << QVector<size_t>(rows_to_delete.begin(), rows_to_delete.end());

    std::sort(
        rows_to_delete.begin(), rows_to_delete.end(), std::greater<size_t>());

    qDebug() << "Rows to delete sorted"
             << QVector<size_t>(rows_to_delete.begin(), rows_to_delete.end());

    for (auto row : rows_to_delete) {
        m_row_to_key_map.erase(m_row_to_key_map.begin() + row);
        for (auto& c : m_columns) {
            c.erase(row);
        }
    }

    qDebug() << "Rebuilding maps";

    for (size_t row = 0; row < m_row_to_key_map.size(); row++) {
        auto k              = m_row_to_key_map[row];
        m_key_to_row_map[k] = row;
    }


    return std::make_shared<DeleteQuery>(this, span_to_vector(key_list.span()));
}

bool TableSource::handle_reset() {
    for (auto& col : m_columns) {
        col.clear();
    }
    return true;
}

bool TableSource::handle_set_selection(std::string_view    s,
                                       SelectionRef const& ref) {

    m_selections[std::string(s)] = ref.to_selection();

    return true;
}


TableSource::~TableSource() = default;

std::vector<std::string> TableSource::get_headers() {
    std::vector<std::string> ret;
    for (auto const& c : m_columns) {
        ret.push_back(c.name);
    }
    return ret;
}


TableQueryPtr TableSource::get_all_data() {
    return std::make_shared<WholeTableQuery>(this);
}


bool TableSource::ask_insert(AnyVarListRef const& cols) {
    auto b = handle_insert(cols);

    if (b) { emit table_row_updated(b); }

    return !!b;
}


bool TableSource::ask_update(AnyVarRef const&     keys,
                             AnyVarListRef const& columns) {
    auto b = handle_update(keys, columns);

    if (b) { emit table_row_updated(b); }

    return !!b;
}

bool TableSource::ask_delete(AnyVarRef const& keys) {
    auto b = handle_deletion(keys);

    if (b) { emit table_row_deleted(b); }

    return !!b;
}

bool TableSource::ask_clear() {
    return handle_reset();
}

bool TableSource::ask_update_selection(std::string_view    k,
                                       SelectionRef const& s) {
    return handle_set_selection(k, s);
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
