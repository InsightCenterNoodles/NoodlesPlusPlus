#include "noo_server_interface.h"

#include "include/noo_include_glm.h"
#include "src/common/variant_tools.h"
#include "src/server/bufferlist.h"
#include "src/server/noodlesserver.h"
#include "src/server/noodlesstate.h"

#include <glm/gtx/component_wise.hpp>

#include <QBuffer>
#include <QDebug>
#include <QFile>

#include <fstream>
#include <sstream>


namespace noo {

static QByteArray image_to_bytes(QImage img) {
    QByteArray array;

    QBuffer buf(&array);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");

    return array;
}

// =============================================================================

MethodException::MethodException(int code, QString message, QCborValue data)
    : m_code(code), m_reason(message), m_data(data) { }

static char const* code_to_name(int code) {
    switch (code) {
    case -32700: return "Parse error";
    case -32600: return "Invalid request";
    case -32601: return "Method not found";
    case -32602: return "Invalid parameters";
    case -32603: return "Internal error";
    default: return "Unknown code";
    }
}

char const* MethodException::what() const noexcept {
    // api prevents us from adding data
    return code_to_name(m_code);
}

QString MethodException::to_string() const {
    return QString("Code %2(%1): %3 Additional: %4")
        .arg(m_code)
        .arg(code_to_name(m_code))
        .arg(m_reason)
        .arg(m_data.toDiagnosticNotation());
}

// =============================================================================

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
        qWarning() << "No code attached to method" << data.method_name;
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
std::shared_ptr<ServerT> create_server(ServerOptions const& options) {
    return std::make_shared<ServerT>(options.port);
}

// Document ====================================================================
DocumentTPtr get_document(ServerT* server) {
    return server->state()->document();
}

void update_document(DocumentTPtrRef doc, DocumentData const& data) {
    return doc->update(data);
}

// Buffer Construction =========================================================


template <class T>
struct is_optional : std::bool_constant<false> { };

template <class T>
struct is_optional<std::optional<T>> : std::bool_constant<true> { };

struct Range {
    QString  key;
    uint64_t offset;
    uint64_t length;
    ViewType type;
};

struct PMDRef {
    size_t start  = 0;
    size_t size   = 0;
    size_t stride = 0;
};

struct PackedMeshDataResult {
    QByteArray data;

    MaterialTPtr material;

    BoundingBox bounding_box;


    PMDRef                positions;
    std::optional<PMDRef> normals;
    std::optional<PMDRef> textures;
    std::optional<PMDRef> colors;
    std::optional<PMDRef> lines;
    std::optional<PMDRef> triangles;
};

static std::optional<PackedMeshDataResult>
pack_mesh_source(MeshSource const& refs) {

    qDebug() << Q_FUNC_INFO;

    if (refs.triangles.empty() and refs.lines.empty()) return std::nullopt;
    if (!refs.triangles.empty() and !refs.lines.empty()) return std::nullopt;
    if (refs.positions.empty()) return std::nullopt;

    PackedMeshDataResult ret;

    // compute cell size
    const size_t cell_byte_size =
        (refs.positions.empty() ? 0 : sizeof(glm::vec3)) +
        (refs.normals.empty() ? 0 : sizeof(glm::vec3)) +
        (refs.textures.empty() ? 0 : sizeof(glm::u16vec2)) +
        (refs.colors.empty() ? 0 : sizeof(glm::u8vec4));

    // if the lists are not equal in size...well we can just duplicate

    glm::vec3 extent_min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 extent_max = glm::vec3(std::numeric_limits<float>::lowest());

    for (auto const& p : refs.positions) {
        extent_min = glm::min(extent_min, p);
        extent_max = glm::max(extent_max, p);
    }

    ret.bounding_box.aabb_max = extent_max;
    ret.bounding_box.aabb_min = extent_min;

    //    qDebug() << "Mesh extents" << ret.extent_min.x << ret.extent_min.y
    //             << ret.extent_min.z << "|" << ret.extent_max.x <<
    //             ret.extent_max.y
    //             << ret.extent_max.z;


    qDebug() << "Cell size" << cell_byte_size;

    auto const num_verts = refs.positions.size();

    qDebug() << "Num verts" << num_verts;

    QByteArray bytes;

    {
        size_t const total_vertex_bytes = num_verts * cell_byte_size;

        qDebug() << "New vert byte range" << total_vertex_bytes;
        QByteArray vertex_portion;
        vertex_portion.resize(total_vertex_bytes);

        char* comp_start = vertex_portion.data();

        auto add_component = [&]<class T, class U>(std::span<T const> vector,
                                                   U& dest_res) {
            if (vector.empty()) return;

            {
                char* write_at = comp_start;

                for (auto const& component : vector) {
                    assert((write_at - vertex_portion.data()) <
                           vertex_portion.size());
                    memcpy(write_at, &component, sizeof(component));
                    write_at += cell_byte_size;
                }
            }

            static_assert(std::is_same_v<glm::vec3, T> or
                          std::is_same_v<glm::vec2, T> or
                          std::is_same_v<glm::u16vec2, T> or
                          std::is_same_v<glm::u8vec4, T>);

            PMDRef* ref;

            if constexpr (is_optional<U>::value) {
                ref = &(dest_res.emplace());
            } else {
                ref = &dest_res;
            }


            ref->start  = (comp_start - vertex_portion.data());
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

        bytes += vertex_portion;
    }

    PMDRef index_ref;
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

    bytes.insert(index_copy_from.size(), (const char*)index_copy_from.data());

    ret.material = refs.material;
    ret.data     = bytes;

    return ret;
}

BufferDirectory create_directory(DocumentTPtrRef doc, BufferSources sources) {
    qDebug() << "Creating buffer directory";
    QByteArray whole_array;

    {
        // estimate size and reserve
        uint64_t sum = 0;

        for (auto const& v : sources) {

            sum += VMATCH(
                v,
                VCASE(BuilderBytes const& b)->uint64_t {
                    return b.bytes.size();
                },
                VCASE(QImage const& b)->uint64_t { return b.sizeInBytes(); },
                VCASE(MeshSource const& b)->uint64_t {
                    return b.positions.size_bytes() + b.normals.size_bytes() +
                           b.textures.size_bytes() + b.colors.size_bytes() +
                           b.lines.size_bytes() + b.triangles.size_bytes();
                });
        }

        whole_array.reserve(sum);

        qDebug() << "Approx. buffer size" << sum;
    }


    // copy

    std::vector<Range> ranges;
    ranges.reserve(sources.size());

    QHash<QString, PackedMeshDataResult> mesh_info;

    for (auto iter = sources.begin(); iter != sources.end(); ++iter) {

        Range r;

        r.key    = iter.key();
        r.offset = whole_array.size();

        QByteArray bytes_to_insert = VMATCH(
            iter.value(),
            VCASE(BuilderBytes const& b) {
                qDebug() << "Adding bytes...";
                r.type = b.type;
                return b.bytes;
            },
            VCASE(QImage const& b) {
                qDebug() << "Adding image...";
                r.type = ViewType::IMAGE_INFO;
                return image_to_bytes(b);
            },
            VCASE(MeshSource const& b) {
                qDebug() << "Adding mesh...";
                r.type      = ViewType::GEOMETRY_INFO;
                auto result = pack_mesh_source(b);
                if (!result) { return QByteArray {}; }
                mesh_info[r.key] = *result;
                return result->data;
            });

        if (bytes_to_insert.isEmpty()) {
            qDebug() << "No bytes for key" << r.key;
            return {};
        }

        r.length = bytes_to_insert.size();
        whole_array += bytes_to_insert;
        ranges.push_back(r);
    }

    auto buff = create_buffer(
        doc,
        BufferData { .source = BufferInlineSource { .data = whole_array } });

    BufferDirectory ret;

    for (auto const& r : ranges) {

        qDebug() << "Creating view for buffer:" << r.key << r.offset
                 << r.length;

        auto view = create_buffer_view(doc,
                                       BufferViewData {
                                           .source_buffer = buff,
                                           .type          = r.type,
                                           .offset        = r.offset,
                                           .length        = r.length,
                                       });

        if (mesh_info.contains(r.key)) {
            qDebug() << "Creating mesh objects...";
            auto const& minfo = mesh_info[r.key];

            MeshData mdata;

            auto& patch = mdata.patches.emplace_back();

            patch.material = minfo.material;

            patch.indicies.view   = view;
            patch.indicies.stride = 0; // we pack our indicies

            if (minfo.lines) {
                patch.indicies.offset = minfo.lines->start;
                patch.indicies.format = Format::U16;
                patch.type            = PrimitiveType::LINES;
            } else {
                patch.indicies.offset = minfo.triangles->start;
                patch.indicies.format = Format::U16;
                patch.type            = PrimitiveType::TRIANGLES;
            }


            auto add_comp =
                [&](auto ref, AttributeSemantic as, Format f, bool normalized) {
                    Attribute new_a {
                        .view       = view,
                        .semantic   = as,
                        .offset     = ref.start,
                        .stride     = ref.stride,
                        .format     = f,
                        .normalized = normalized,
                    };

                    if (as == AttributeSemantic::POSITION) {
                        auto mins = minfo.bounding_box.aabb_min;
                        auto maxs = minfo.bounding_box.aabb_max;
                        new_a.minimum_value << mins.x << mins.y << mins.z;
                        new_a.maximum_value << maxs.x << maxs.y << maxs.z;
                    }

                    patch.attributes.push_back(new_a);
                };

            add_comp(minfo.positions,
                     AttributeSemantic::POSITION,
                     Format::VEC3,
                     false);

            if (minfo.normals) {
                add_comp(*minfo.normals,
                         AttributeSemantic::NORMAL,
                         Format::VEC3,
                         false);
            }

            if (minfo.textures) {
                add_comp(*minfo.textures,
                         AttributeSemantic::TEXTURE,
                         Format::U16VEC2,
                         true);
            }

            if (minfo.colors) {
                add_comp(*minfo.colors,
                         AttributeSemantic::TEXTURE,
                         Format::U8VEC4,
                         true);
            }

            ret[r.key] = create_mesh(doc, mdata);

        } else {
            ret[r.key] = view;
        }
    }

    qDebug() << "Done";

    return ret;
}

noo::MeshTPtr create_mesh(DocumentTPtrRef doc, noo::MeshSource const& src) {
    auto          key = QStringLiteral("mesh");
    BufferSources sources { { key, src } };

    auto dir = create_directory(doc, sources);

    return std::get<MeshTPtr>(dir[key]);
}

BufferTPtr create_buffer(DocumentTPtrRef doc, BufferData const& data) {
    return doc->buffer_list().provision_next(data);
}

BufferTPtr create_buffer_from_file(DocumentTPtrRef doc, QString path) {
    QFile file(path);

    if (!file.open(QFile::ReadOnly)) { return {}; }


    auto all_bytes = file.readAll();

    return create_buffer(doc,
                         {
                             .name   = file.fileName(),
                             .source = BufferInlineSource { .data = all_bytes },
                         });
}

// BufferView ==================================================================

BufferViewTPtr create_buffer_view(DocumentTPtrRef       doc,
                                  BufferViewData const& data) {
    return doc->buffer_view_list().provision_next(data);
}

// Image =======================================================================

ImageTPtr create_image(DocumentTPtrRef doc, ImageData const& data) {
    return doc->image_list().provision_next(data);
}

// Sampler =====================================================================

SamplerTPtr create_sampler(DocumentTPtrRef doc, SamplerData const& data) {
    return doc->sampler_list().provision_next(data);
}

// Texture =====================================================================
TextureTPtr create_texture(DocumentTPtrRef doc, TextureData const& data) {
    return doc->tex_list().provision_next(data);
}

TextureTPtr create_texture(DocumentTPtrRef doc, QImage img) {

    auto bytes = image_to_bytes(img);

    auto new_buffer = create_buffer(
        doc, BufferData { .source = BufferInlineSource { .data = bytes } });


    auto new_view = create_buffer_view(doc,
                                       BufferViewData {
                                           .source_buffer = new_buffer,
                                           .type   = ViewType::IMAGE_INFO,
                                           .offset = 0,
                                           .length = (uint64_t)bytes.size(),
                                       });

    auto new_image = create_image(doc,
                                  ImageData {
                                      .source = new_view,
                                  });


    return create_texture(doc, TextureData { .image = new_image });
}


// Material ====================================================================

MaterialTPtr create_material(DocumentTPtrRef doc, MaterialData const& data) {
    return doc->mat_list().provision_next(data);
}

void update_material(MaterialTPtr item, MaterialData const& data) {
    item->update(data);
}

// Light =======================================================================

LightTPtr create_light(DocumentTPtrRef doc, LightData const& data) {
    return doc->light_list().provision_next(data);
}

void update_light(LightTPtr const& item, LightUpdateData const& data) {
    item->update(data);
}

// Mesh ========================================================================

// MeshData::MeshData(PackedMeshDataResult const& res, BufferTPtr ptr) {
//     auto set_from = [&ptr](auto const& src, auto& out) {
//         qDebug() << "HERE";
//         if (!src) return;
//         auto& l  = *src;
//         auto& d  = out.emplace();
//         d.buffer = ptr;
//         d.start  = l.start;
//         d.size   = l.size;
//         d.stride = l.stride;

//        qDebug() << d.start << d.stride << d.size;
//    };

//    bounding_box = res.bounding_box;

//    positions.buffer = ptr;
//    positions.start  = res.positions.start;
//    positions.size   = res.positions.size;
//    positions.stride = res.positions.stride;

//    set_from(res.normals, normals);
//    set_from(res.textures, textures);
//    set_from(res.colors, colors);
//    set_from(res.lines, lines);
//    set_from(res.triangles, triangles);
//}

MeshTPtr create_mesh(DocumentTPtrRef doc, MeshData const& data) {
    return doc->mesh_list().provision_next(data);
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

bool TableQuery::get_cell_to(size_t, size_t, QString&) const {
    return false;
}

bool TableQuery::get_keys_to(std::span<int64_t>) const {
    return false;
}

// =============

size_t TableColumn::size() const {
    return noo::visit([&](auto const& a) { return uint64_t(a.size()); }, *this);
}

bool TableColumn::is_string() const {
    return std::holds_alternative<QStringList>(*this);
}

std::span<double const> TableColumn::as_doubles() const {
    auto* p = std::get_if<QVector<double>>(this);

    return p ? std::span<double const>(*p) : std::span<double const> {};
}
QStringList const& TableColumn::as_string() const {
    auto* p = std::get_if<QStringList>(this);

    if (p) return *p;

    static const QStringList blank;

    return blank;
}

void TableColumn::append(std::span<double const> d) {
    VMATCH(
        *this,
        VCASE(QVector<double> & a) {
            a << QVector<double>(d.begin(), d.end());
        },
        VCASE(QStringList & a) {
            for (auto value : d) {
                a.push_back(QString::number(value));
            }
        });
}

void TableColumn::append(QCborArray const& d) {
    VMATCH(
        *this,
        VCASE(QVector<double> & a) {
            for (auto const& s : d) {
                a.push_back(s.toDouble());
            }
        },
        VCASE(QStringList & a) {
            for (auto const& s : d) {
                a.push_back(s.toString());
            }
        });
}

void TableColumn::append(double d) {
    VMATCH(
        *this,
        VCASE(QVector<double> & a) { a.push_back(d); },
        VCASE(QStringList & a) { a.push_back(QString::number(d)); });
}
void TableColumn::append(QString d) {
    VMATCH(
        *this,
        VCASE(QVector<double> & a) { a.push_back(d.toDouble()); },
        VCASE(QStringList & a) { a.push_back(d); });
}

void TableColumn::set(size_t row, double d) {
    VMATCH(
        *this,
        VCASE(QVector<double> & a) { a[row] = d; },
        VCASE(QStringList & a) { a[row] = QString::number(d); });
}
void TableColumn::set(size_t row, QCborValue d) {
    VMATCH(
        *this,
        VCASE(QVector<double> & a) { a[row] = d.toDouble(); },
        VCASE(QStringList & a) { a[row] = d.toString(); });
}
void TableColumn::set(size_t row, QString d) {
    VMATCH(
        *this,
        VCASE(QVector<double> & a) { a[row] = d.toDouble(); },
        VCASE(QStringList & a) { a[row] = d; });
}

void TableColumn::erase(size_t row) {
    noo::visit([row](auto& a) { a.erase(a.begin() + row); }, *this);
}

void TableColumn::clear() {
    noo::visit([](auto& a) { a.clear(); }, *this);
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

        copy_range(sp, dest);

        return true;
    }

    bool get_cell_to(size_t col, size_t row, QString& s) const override {
        try {
            auto& column = source->get_columns().at(col);

            auto sp = column.as_string();

            if (row >= sp.size()) return false;

            s = sp[row];

            return true;

        } catch (...) { return false; }
    }

    bool get_keys_to(std::span<int64_t> dest) const override {
        auto const& r = source->get_row_to_key_map();
        copy_range(r, dest);
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

        Q_ASSERT(!column.is_string()); // should already have been checked

        auto sp = column.as_doubles();

        qDebug() << Q_FUNC_INFO << col << dest.size() << start_at << num_rows
                 << sp.size();

        sp = noo::safe_subspan(sp, start_at, num_rows);

        Q_ASSERT(sp.size() == num_rows);

        copy_range(sp, dest);

        return true;
    }

    bool get_cell_to(size_t col, size_t row, QString& s) const override {
        try {
            auto& column = source->get_columns().at(col);

            auto sp = column.as_string();

            if (row >= sp.size()) return false;

            s = sp[row + start_at];

            return true;

        } catch (...) { return false; }
    }

    bool get_keys_to(std::span<int64_t> dest) const override {
        auto const& r = source->get_row_to_key_map();

        auto key_sp = noo::safe_subspan(std::span(r), start_at, num_rows);

        Q_ASSERT(key_sp.size() == num_rows);

        copy_range(key_sp, dest);
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

    bool get_cell_to(size_t col, size_t row, QString& s) const override {
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

    bool get_cell_to(size_t /*col*/, size_t /*row*/, QString&) const override {
        return false;
    }

    bool get_keys_to(std::span<int64_t> dest) const override {
        qDebug() << Q_FUNC_INFO << dest.size() << keys.size();
        copy(keys.begin(), keys.end(), dest.begin(), dest.end());
        return true;
    }
};

} // namespace

TableQueryPtr TableSource::handle_insert(QCborArray const& cols) {
    // get dimensions of insert

    size_t num_cols = cols.size();

    if (num_cols == 0) return nullptr;
    if (num_cols != m_columns.size()) return nullptr;

    size_t num_rows = 0;
    bool   ok       = true;

    for (size_t ci = 0; ci < num_cols; ci++) {
        auto r = cols[ci];
        if (r.isArray()) {
            auto la  = r.toArray();
            num_rows = std::max(num_rows, static_cast<size_t>(la.size()));
        } else {
            ok = false;
            break;
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

    // qDebug() << "assigned keys"
    //         << QVector<int64_t>::fromStdVector(new_row_keys);

    // now lets insert

    for (size_t ci = 0; ci < num_cols; ci++) {
        auto  source_col = cols[ci];
        auto& dest_col   = m_columns.at(ci);

        if (source_col.isArray()) {
            auto la = source_col.toArray();

            dest_col.append(la);
        }
    }

    // now return a query to the data

    return std::make_shared<InsertQuery>(this, current_row_count, num_rows);
}

TableQueryPtr TableSource::handle_update(QCborValue const& keys,
                                         QCborArray const& cols) {
    // get dimensions of update

    size_t const num_cols = cols.size();

    if (num_cols == 0) return nullptr;
    if (num_cols != m_columns.size()) return nullptr;

    size_t num_rows = 0;
    bool   ok       = true;

    for (size_t ci = 0; ci < num_cols; ci++) {
        auto r = cols[ci];

        if (r.isArray()) {
            num_rows = std::max<size_t>(num_rows, r.toArray().size());
        } else {
            ok = false;
            break;
        }
    }

    if (!ok or num_rows == 0) return nullptr;

    qDebug() << Q_FUNC_INFO << num_rows;

    // lets get some keys

    auto key_list      = coerce_to_int_list(keys);
    auto key_list_span = std::span(key_list);

    // now lets update

    for (size_t key_i = 0; key_i < key_list_span.size(); key_i++) {
        auto key = key_list_span[key_i];

        auto iter = m_key_to_row_map.find(key);

        if (iter == m_key_to_row_map.end()) continue;

        auto const update_at = iter->second;

        for (size_t ci = 0; ci < num_cols; ci++) {
            auto  source_col = cols[ci];
            auto& dest_col   = m_columns.at(ci);

            dest_col.set(update_at, source_col[key_i]);
        }
    }

    // now return a query to the data

    return std::make_shared<UpdateQuery>(this, span_to_vector(key_list_span));
}

TableQueryPtr TableSource::handle_deletion(QCborValue const& keys) {
    qDebug() << Q_FUNC_INFO;
    auto key_list = coerce_to_int_list(keys);

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


    return std::make_shared<DeleteQuery>(this,
                                         span_to_vector(std::span(key_list)));
}

bool TableSource::handle_reset() {
    for (auto& col : m_columns) {
        col.clear();
    }
    return true;
}

bool TableSource::handle_set_selection(Selection const& ref) {
    m_selections[ref.name] = ref;

    return true;
}


TableSource::~TableSource() = default;

QStringList TableSource::get_headers() {
    QStringList ret;
    for (auto const& c : m_columns) {
        ret << c.name;
    }
    return ret;
}


TableQueryPtr TableSource::get_all_data() {
    return std::make_shared<WholeTableQuery>(this);
}


bool TableSource::ask_insert(QCborArray const& cols) {
    auto b = handle_insert(cols);

    if (b) { emit table_row_updated(b); }

    return !!b;
}


bool TableSource::ask_update(QCborValue const& keys,
                             QCborArray const& columns) {
    auto b = handle_update(keys, columns);

    if (b) { emit table_row_updated(b); }

    return !!b;
}

bool TableSource::ask_delete(QCborValue const& keys) {
    auto b = handle_deletion(keys);

    if (b) { emit table_row_deleted(b); }

    return !!b;
}

bool TableSource::ask_clear() {
    auto b = handle_reset();

    if (b) emit table_reset();

    return b;
}

bool TableSource::ask_update_selection(Selection const& s) {
    auto b = handle_set_selection(s);

    if (b) emit table_selection_updated(s);

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


// Callbacks

EntityCallbacks::EntityCallbacks(ObjectT* h, EnableCallback c)
    : m_host(h), m_enabled(c) { }

ObjectT* EntityCallbacks::get_host() {
    return m_host;
}

EntityCallbacks::EnableCallback const&
EntityCallbacks::callbacks_enabled() const {
    return m_enabled;
}

void        EntityCallbacks::on_activate_str(QString) { }
void        EntityCallbacks::on_activate_int(int) { }
QStringList EntityCallbacks::get_activation_choices() {
    return {};
}

QStringList EntityCallbacks::get_option_choices() {
    return {};
}
std::string EntityCallbacks::get_current_option() {
    return {};
}
void EntityCallbacks::set_current_option(QString) { }

void EntityCallbacks::set_position(glm::vec3) { }
void EntityCallbacks::set_rotation(glm::quat) { }
void EntityCallbacks::set_scale(glm::vec3) { }

void EntityCallbacks::select_region(glm::vec3 /*min*/,
                                    glm::vec3 /*max*/,
                                    SelAction /*select*/) { }
void EntityCallbacks::select_sphere(glm::vec3 /*point*/,
                                    float /*distance*/,
                                    SelAction /*select*/) { }
void EntityCallbacks::select_plane(glm::vec3 /*point*/,
                                   glm::vec3 /*normal*/,
                                   SelAction /*select*/) { }

void EntityCallbacks::select_hull(std::span<glm::vec3 const> /*point_list*/,
                                  std::span<int64_t const> /*index_list*/,
                                  SelAction /*select*/) { }

std::pair<QString, glm::vec3> EntityCallbacks::probe_at(glm::vec3) {
    return {};
}


// Other =======================================================================

void issue_signal_direct(DocumentT* doc, SignalT* signal, QCborArray var) {
    if (!doc or !signal) return;

    if (!doc->att_signal_list().has(signal)) return;

    signal->fire({}, std::move(var));
}

void issue_signal_direct(DocumentT*     doc,
                         QString const& signal,
                         QCborArray     var) {
    auto* sig = doc->att_signal_list().find_by_name(signal);
    return issue_signal_direct(doc, sig, std::move(var));
}

void issue_signal_direct(TableT* tbl, SignalT* signal, QCborArray var) {
    if (!tbl or !signal) return;

    if (!tbl->att_signal_list().has(signal)) return;

    signal->fire(tbl->id(), std::move(var));
}

void issue_signal_direct(TableT* tbl, QString const& signal, QCborArray var) {

    auto* sig = tbl->att_signal_list().find_by_name(signal);
    return issue_signal_direct(tbl, sig, std::move(var));
}

void issue_signal_direct(ObjectT* obj, SignalT* signal, QCborArray var) {
    if (!obj or !signal) return;

    if (!obj->att_signal_list().has(signal)) return;

    signal->fire(obj->id(), std::move(var));
}

void issue_signal_direct(ObjectT* obj, QString const& signal, QCborArray var) {

    auto* sig = obj->att_signal_list().find_by_name(signal);
    return issue_signal_direct(obj, sig, std::move(var));
}

} // namespace noo
