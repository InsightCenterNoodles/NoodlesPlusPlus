#include "noo_server_interface.h"

#include "include/noo_include_glm.h"
#include "src/common/variant_tools.h"
#include "src/server/bufferlist.h"
#include "src/server/noodlesserver.h"
#include "src/server/noodlesstate.h"

#include <glm/gtx/component_wise.hpp>

#include <QBuffer>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QLoggingCategory>

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

// Server ======================================================================
std::shared_ptr<ServerT> create_server(ServerOptions const& options) {
    return std::make_shared<ServerT>(options);
}

std::shared_ptr<ServerT> create_server(QCommandLineParser& parser) {
    QCommandLineOption debug_option(
        "d", QCoreApplication::translate("main", "Enable debug output."));

    QCommandLineOption port_option(
        "p",
        QCoreApplication::translate("main", "Port number to use."),
        "port",
        "50000");

    QCommandLineOption asset_port_option(
        "ap",
        QCoreApplication::translate("main", "Asset server port number to use."),
        "port",
        "50001");

    QCommandLineOption asset_host_option(
        "ah",
        QCoreApplication::translate(
            "main", "Asset server host name to use (automatic by default)"),
        "hostname",
        "");

    parser.addOption(debug_option);
    parser.addOption(port_option);
    parser.addOption(asset_port_option);
    parser.addOption(asset_host_option);

    parser.process(*QCoreApplication::instance());

    {
        bool use_debug = parser.isSet(debug_option);

        if (!use_debug) { QLoggingCategory::setFilterRules("*.debug=false"); }
    }

    auto get_port_opt =
        [&parser](QCommandLineOption const& opt) -> std::optional<uint16_t> {
        bool ok;
        auto value = parser.value(opt).toInt(&ok);
        if (ok and value > 0 and value < std::numeric_limits<uint16_t>::max()) {
            return value;
        }
        return {};
    };

    ServerOptions options;
    options.port = get_port_opt(port_option).value_or(options.port);

    if (auto ap = get_port_opt(asset_port_option); ap and *ap != options.port) {
        options.asset_port = *ap;
    }

    options.asset_hostname = parser.value(asset_host_option);

    return create_server(options);
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

    size_t vcount;
    size_t icount;


    PMDRef                positions;
    std::optional<PMDRef> normals;
    std::optional<PMDRef> textures;
    std::optional<PMDRef> colors;
    std::optional<PMDRef> lines;
    std::optional<PMDRef> triangles;

    Format format;
};

static std::optional<PackedMeshDataResult>
pack_mesh_source(MeshSource const& refs) {

    qDebug() << Q_FUNC_INFO;

    if (refs.indices.empty()) return std::nullopt;
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

    ret.vcount = num_verts;

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

    qDebug() << "Packed vertex" << bytes.size();

    PMDRef index_ref;
    index_ref.start = bytes.size();

    std::span<std::byte const> index_copy_from;

    {
        index_copy_from = std::as_bytes(refs.indices);
        index_ref.size  = index_copy_from.size();

        ret.format = refs.index_format;

        auto base_size = 1;

        switch (refs.index_format) {
        case Format::U8: break;
        case Format::U16: base_size = sizeof(uint16_t); break;
        case Format::U32: base_size = sizeof(uint32_t); break;
        default: break;
        }


        switch (refs.type) {
        case MeshSource::LINE:
            ret.lines        = index_ref;
            index_ref.stride = base_size * 2;
            break;
        case MeshSource::TRIANGLE:
            ret.triangles    = index_ref;
            index_ref.stride = base_size * 3;
            break;
        }

        ret.icount = refs.indices.size() / base_size;
    }

    qDebug() << "Packed index" << index_copy_from.size();

    bytes.insert(bytes.size(),
                 (const char*)index_copy_from.data(),
                 index_copy_from.size());

    ret.material = refs.material;
    ret.data     = bytes;

    qDebug() << "Packed" << bytes.size();

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
                           b.indices.size_bytes();
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

            patch.material     = minfo.material;
            patch.vertex_count = minfo.vcount;

            auto& new_index = patch.indices.emplace();

            // what format to use?

            new_index.view   = view;
            new_index.stride = 0; // we pack our indicies
            new_index.format = minfo.format;
            new_index.count  = minfo.icount;

            if (minfo.lines) {
                new_index.offset = minfo.lines->start;
                patch.type       = PrimitiveType::LINES;
            } else {
                new_index.offset = minfo.triangles->start;
                patch.type       = PrimitiveType::TRIANGLES;
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
                         AttributeSemantic::COLOR,
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

ServerTableDelegate::ServerTableDelegate(QObject* p) : QObject(p) { }
ServerTableDelegate::~ServerTableDelegate() = default;

QStringList ServerTableDelegate::get_headers() {
    return QStringList();
}


std::pair<QCborArray, QCborArray> ServerTableDelegate::get_all_data() {
    return {};
}

QList<Selection> ServerTableDelegate::get_all_selections() {
    return {};
}


void ServerTableDelegate::handle_insert(QCborArray const& rows) {
    //    QCborArray new_keys, fixed_rows;
    //    auto       b = handle_insert(rows, new_keys, fixed_rows);

    //    if (b) { emit table_row_updated(new_keys, fixed_rows); }

    //    return b;
}


void ServerTableDelegate::handle_update(QCborArray const& keys,
                                        QCborArray const& rows) {
    //    QCborArray fixed_rows;

    //    auto b = handle_update(keys.toArray(), rows, fixed_rows);

    //    if (b) { emit table_row_updated(keys.toArray(), fixed_rows); }

    //    return b;
}

void ServerTableDelegate::handle_deletion(QCborArray const& keys) {
    //    auto b = handle_deletion(keys.toArray());

    //    if (b) { emit table_row_deleted(keys.toArray()); }

    //    return !!b;
}

void ServerTableDelegate::handle_reset() {
    //    auto b = handle_reset();

    //    if (b) emit table_reset();

    //    return b;
}

void ServerTableDelegate::handle_set_selection(Selection const& s) {
    //    auto b = handle_set_selection(s);

    //    if (b) emit table_selection_updated(s);

    //    return b;
}


TableTPtr create_table(DocumentTPtrRef doc, TableData const& data) {
    return doc->table_list().provision_next(data);
}

// Table Delegate ==============================================================

uint64_t AbstractTableDelegate::next_counter() {
    auto save = m_counter;
    m_counter++;
    return save;
}

void AbstractTableDelegate::reconnect_standard(QAbstractTableModel* m) {
    m_connections << connect(m,
                             &QAbstractTableModel::destroyed,
                             this,
                             &AbstractTableDelegate::act_model_destroy);

    m_connections << connect(m,
                             &QAbstractTableModel::modelReset,
                             this,
                             &AbstractTableDelegate::act_model_reset);

    m_connections << connect(m,
                             &QAbstractTableModel::rowsInserted,
                             this,
                             &AbstractTableDelegate::act_model_rows_inserted);

    m_connections << connect(m,
                             &QAbstractTableModel::rowsRemoved,
                             this,
                             &AbstractTableDelegate::act_model_rows_removed);

    m_connections << connect(m,
                             &QAbstractTableModel::columnsInserted,
                             this,
                             &AbstractTableDelegate::act_model_reset);

    m_connections << connect(m,
                             &QAbstractTableModel::columnsRemoved,
                             this,
                             &AbstractTableDelegate::act_model_reset);

    m_connections << connect(m,
                             &QAbstractTableModel::columnsMoved,
                             this,
                             &AbstractTableDelegate::act_model_reset);

    m_connections << connect(m,
                             &QAbstractTableModel::dataChanged,
                             this,
                             &AbstractTableDelegate::act_model_data_changed);
}

void AbstractTableDelegate::handle_insert(QCborArray const& new_rows) {
    // we check both: the insertable class is not a child class of qobject, and
    // we dont want to have to force that, as it could require virtual
    // inheriting. so, we can just check the pointer that we do know has to be a
    // qobject.
    if (m_table_model and m_insertable) {
        m_insertable->handle_insert(new_rows);
    }
}

void AbstractTableDelegate::handle_update(QCborArray const& keys,
                                          QCborArray const& rows) {
    if (m_table_model and m_insertable) {
        m_insertable->handle_update(keys, rows);
    }
}

void AbstractTableDelegate::handle_deletion(QCborArray const& keys) {
    if (m_table_model and m_insertable) { m_insertable->handle_deletion(keys); }
}

void AbstractTableDelegate::handle_reset() {
    if (m_table_model and m_insertable) { m_insertable->handle_reset(); }
}

void AbstractTableDelegate::handle_set_selection(Selection const& s) {
    m_selections[s.name] = s;
    emit table_selection_updated(s);

    if (s.row_ranges.empty() and s.rows.empty()) {
        m_selections.remove(s.name);
    }
}

AbstractTableDelegate::~AbstractTableDelegate() { }

QStringList AbstractTableDelegate::get_headers() {

    QStringList ret;

    if (m_table_model) {
        for (auto i = 0; i < m_table_model->columnCount(); i++) {
            ret << m_table_model->headerData(i, Qt::Orientation::Horizontal)
                       .toString();
        }
    }

    return ret;
}

std::pair<QCborArray, QCborArray> AbstractTableDelegate::get_all_data() {

    QCborArray rows;
    QCborArray keys;

    auto cc = m_table_model->columnCount();

    for (auto iter = m_model_to_key_map.begin();
         iter != m_model_to_key_map.end();
         ++iter) {
        auto key = iter.value();

        keys << (qint64)key;

        auto model_row = iter.key().row();

        QCborArray row;

        for (auto i = 0; i < cc; i++) {
            row << QCborValue::fromVariant(
                m_table_model->data(m_table_model->index(model_row, i)));
        }

        rows << row;
    }

    return { keys, rows };
}

QList<Selection> AbstractTableDelegate::get_all_selections() {
    return m_selections.values();
}

void AbstractTableDelegate::act_model_destroy() {
    set_model((QAbstractTableModel*)nullptr);
}

void AbstractTableDelegate::act_model_reset() {
    emit table_reset();
}

void AbstractTableDelegate::act_model_rows_inserted(QModelIndex mi,
                                                    int         first,
                                                    int         last) {
    Q_ASSERT(!mi.isValid());

    QCborArray new_keys;

    QCborArray rows;

    for (int i = first; i <= last; i++) {
        auto key = next_counter();
        new_keys << (qint64)key;
        auto pi = QPersistentModelIndex(m_table_model->index(i, 0));
        m_key_to_model_map[key] = pi;
        m_model_to_key_map[pi]  = key;

        QCborArray row;

        for (int j = 0; j < m_table_model->columnCount(); j++) {
            row << QCborValue::fromVariant(
                m_table_model->data(m_table_model->index(i, j)));
        }

        rows << row;
    }

    emit table_row_updated(new_keys, rows);
}
void AbstractTableDelegate::act_model_rows_removed(QModelIndex mi,
                                                   int         first,
                                                   int         last) {
    Q_ASSERT(!mi.isValid());

    QCborArray del_keys;

    for (int i = first; i <= last; i++) {
        auto index = m_table_model->index(i, 0);
        auto key   = m_model_to_key_map[QPersistentModelIndex(index)];

        del_keys << (qint64)key;
    }

    emit table_row_deleted(del_keys);
}
void AbstractTableDelegate::act_model_data_changed(QModelIndex top_left,
                                                   QModelIndex bottom_right,
                                                   QVector<int>) {
    int first = std::min(top_left.row(), bottom_right.row());
    int last  = std::max(top_left.row(), bottom_right.row());

    QCborArray new_keys;

    QCborArray rows;

    for (int i = first; i <= last; i++) {
        auto index = m_table_model->index(i, 0);
        auto key   = m_model_to_key_map[QPersistentModelIndex(index)];

        new_keys << (qint64)key;
        m_key_to_model_map[key] = m_table_model->index(i, 0);

        QCborArray row;

        for (int j = 0; j < m_table_model->columnCount(); j++) {
            row << QCborValue::fromVariant(
                m_table_model->data(m_table_model->index(i, j)));
        }

        rows << row;
    }

    emit table_row_updated(new_keys, rows);
}

// Variant Delegate ============================================================

uint64_t VariantTableDelegate::next_counter() {
    auto save = m_counter;
    m_counter++;
    return save;
}

void VariantTableDelegate::normalize_row(QCborArray& row_arr) {
    while (row_arr.size() < m_headers.size()) {
        row_arr << QCborValue(0);
    }

    while (row_arr.size() > m_headers.size()) {
        row_arr.pop_back();
    }
}

std::pair<QCborArray, QCborArray>
VariantTableDelegate::common_insert(QCborArray const& new_rows) {
    QCborArray new_keys;
    QCborArray final_rows;

    for (auto row : qAsConst(new_rows)) {
        auto new_key = next_counter();

        new_keys << (qint64)new_key;

        auto row_arr = row.toArray();

        normalize_row(row_arr);

        final_rows << row_arr;

        m_rows[new_key] = row_arr;
    }

    return { new_keys, final_rows };
}

void VariantTableDelegate::handle_insert(QCborArray const& new_rows) {

    auto [new_keys, final_rows] = common_insert(new_rows);

    emit table_row_updated(new_keys, final_rows);
}

void VariantTableDelegate::handle_update(QCborArray const& keys,
                                         QCborArray const& rows) {

    QCborArray final_keys;
    QCborArray final_rows;

    int bounds = std::min(keys.size(), rows.size());

    for (int i = 0; i < bounds; i++) {
        auto key = keys[i].toInteger(-1);

        auto iter = m_rows.find(key);

        if (iter == m_rows.end()) { continue; }

        auto row_arr = rows[i].toArray();

        normalize_row(row_arr);

        final_keys << key;
        final_rows << row_arr;

        iter.value() = row_arr;
    }

    emit table_row_updated(final_keys, final_rows);
}
void VariantTableDelegate::handle_deletion(QCborArray const& keys) {

    QCborArray final_keys;

    for (auto key_v : qAsConst(keys)) {
        auto key = key_v.toInteger(-1);

        auto iter = m_rows.find(key);

        if (iter == m_rows.end()) { continue; }

        m_rows.erase(iter);

        final_keys << key;
    }

    emit table_row_deleted(final_keys);
}

void VariantTableDelegate::handle_reset() {
    m_rows.clear();

    emit table_reset();
}
void VariantTableDelegate::handle_set_selection(Selection const& s) {
    m_selections[s.name] = s;
    emit table_selection_updated(s);

    if (s.row_ranges.empty() and s.rows.empty()) {
        m_selections.remove(s.name);
    }
}

VariantTableDelegate::VariantTableDelegate(QStringList column_names,
                                           QCborArray  initial_rows,
                                           QObject*    p)
    : ServerTableDelegate(p), m_headers(column_names) {

    common_insert(initial_rows);
}
VariantTableDelegate::~VariantTableDelegate() { }

QStringList VariantTableDelegate::get_headers() {
    return m_headers;
}
std::pair<QCborArray, QCborArray> VariantTableDelegate::get_all_data() {

    QCborArray keys;
    QCborArray rows;

    for (auto iter = m_rows.begin(); iter != m_rows.end(); ++iter) {
        keys << (qint64)iter.key();
        rows << iter.value();
    }

    return { keys, rows };
}

QList<Selection> VariantTableDelegate::get_all_selections() {
    return m_selections.values();
}


// Object ======================================================================

std::shared_ptr<ObjectT> create_object(DocumentTPtrRef   doc,
                                       ObjectData const& data) {
    return doc->obj_list().provision_next(data);
}

void update_object(ObjectT* item, ObjectUpdateData& data) {
    item->update(data);
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

QStringList EntityCallbacks::get_var_keys() {
    return {};
}
QCborArray EntityCallbacks::get_var_options(QString /*key*/) {
    return {};
}
QCborValue EntityCallbacks::get_var_value(QString /*key*/) {
    return {};
}
bool EntityCallbacks::set_var_value(QCborValue /*value*/, QString /*key*/) {
    return false;
}

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
