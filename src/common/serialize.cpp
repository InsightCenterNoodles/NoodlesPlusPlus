#include "src/common/serialize.h"

#include "src/common/variant_tools.h"

#include <QDebug>
#include <QtGlobal>

namespace noo {

namespace messages {

#define NOONVP(NAME) a(#NAME, NAME)

// =============================================================================
// Serialization
// =============================================================================

template <class Tag>
QCborValue serialize_f(noo::ID<Tag> id);
QCborValue serialize_f(InvokeID id);
QCborValue serialize_f(QString& t);
QCborValue serialize_f(bool t);
QCborValue serialize_f(int t);
QCborValue serialize_f(int64_t t);
QCborValue serialize_f(uint64_t t);
QCborValue serialize_f(float t);
template <class T>
QCborValue serialize_f(QVector<T>& t);
QCborValue serialize_f(QStringList& t);
QCborValue serialize_f(glm::vec3& t);
QCborValue serialize_f(glm::vec4& t);
QCborValue serialize_f(glm::mat3& t);
QCborValue serialize_f(glm::mat4& t);
QCborValue serialize_f(QByteArray& t);
QCborValue serialize_f(QCborArray& t);
QCborValue serialize_f(QCborValue& t);
QCborValue serialize_f(QColor& t);
QCborValue serialize_f(QUrl& t);
template <class T>
QCborValue serialize_f(std::optional<T>& t);
QCborValue serialize_f(noo::BoundingBox&);

template <class T>
QCborValue serialize(T& t);

//

template <class Tag>
QCborValue serialize(noo::ID<Tag> id) {
    return id.to_cbor();
};

QCborValue serialize_f(InvokeID id) {
    static auto const entity_name = QStringLiteral("entity");
    static auto const table_name  = QStringLiteral("table");
    static auto const plot_name   = QStringLiteral("plot");

    QCborMap m;

    VMATCH(
        id,
        VCASE(EntityID id) { m[entity_name] = serialize(id); },
        VCASE(TableID id) { m[table_name] = serialize(id); },
        VCASE(PlotID id) { m[plot_name] = serialize(id); },
        VCASE(std::monostate) {
            // do nothing.
        });


    return m;
}

QCborValue serialize_f(QString& t) {
    return { t };
}

QCborValue serialize_f(bool t) {
    return { t };
}
QCborValue serialize_f(int t) {
    return { t };
}

QCborValue serialize_f(int64_t t) {
    return { (::qint64)t };
}

QCborValue serialize_f(uint64_t t) {
    return { (::qint64)t };
}

QCborValue serialize_f(float t) {
    return { (float)t };
}

template <class T>
QCborValue serialize_f(QVector<T>& t) {
    QCborArray ret;
    for (auto& lt : t) {
        ret << serialize(lt);
    }
    return ret;
}

QCborValue serialize_f(QStringList& t) {
    return QCborArray::fromStringList(t);
}

QCborValue serialize_f(glm::vec3& t) {
    QCborArray ret;
    ret << t.x << t.y << t.z;
    return ret;
}

QCborValue serialize_f(glm::vec4& t) {
    QCborArray ret;
    ret << t.x << t.y << t.z << t.w;
    return ret;
}

QCborValue serialize_f(glm::mat3& t) {
    QCborArray ret;

    float const* p = glm::value_ptr(t);
    for (int i = 0; i < 9; i++) {
        ret << p[i];
    }
    return ret;
}

QCborValue serialize_f(glm::mat4& t) {
    QCborArray ret;

    float const* p = glm::value_ptr(t);
    for (int i = 0; i < 16; i++) {
        ret << p[i];
    }
    return ret;
}

QCborValue serialize_f(QByteArray& t) {
    return t;
}
QCborValue serialize_f(QCborArray& t) {
    return t;
}
QCborValue serialize_f(QCborValue& t) {
    return t;
}

QCborValue serialize_f(QColor& t) {
    QCborArray ret;
    ret << t.redF() << t.greenF() << t.blueF() << t.alphaF();
    return ret;
}

QCborValue serialize_f(QUrl& t) {
    return QCborValue(t);
}

template <class T>
QCborValue serialize_f(std::optional<T>& t) {
    if (!t) return {};
    return serialize(*t);
}

QCborValue serialize_f(noo::BoundingBox& b) {
    QCborMap m;
    m[QStringLiteral("min")] = serialize(b.aabb_min);
    m[QStringLiteral("max")] = serialize(b.aabb_max);
    return m;
}

struct CBArchive {
    QCborMap map;

    template <class T>
    void operator()(QString s, T& v) {
        map[s] = serialize(v);
    }

    template <class T>
    void operator()(QString s, std::optional<T>& v) {
        if (v) map[s] = serialize(*v);
    }
};

template <class T>
QCborValue serialize_f(T& t) {
    CBArchive archive;
    t.serialize(archive);
    return archive.map;
}

template <class T>
QCborValue serialize(T& t) {
    return serialize_f(t);
}

// =============================================================================
// Deserialization
// =============================================================================

template <class Tag>
bool deserialize_f(QCborValue v, noo::ID<Tag>& id);
bool deserialize_f(QCborValue v, noo::InvokeID& t);
bool deserialize_f(QCborValue v, QString& t);
bool deserialize_f(QCborValue v, bool& t);
bool deserialize_f(QCborValue v, int64_t& t);
bool deserialize_f(QCborValue v, uint64_t& t);
bool deserialize_f(QCborValue v, float& t);
template <class T>
bool deserialize_f(QCborValue v, QVector<T>& t);
bool deserialize_f(QCborValue v, QStringList& t);
bool deserialize_f(QCborValue v, glm::vec3& t);
bool deserialize_f(QCborValue v, glm::vec4& t);
bool deserialize_f(QCborValue v, glm::mat3& t);
bool deserialize_f(QCborValue v, glm::mat4& t);
bool deserialize_f(QCborValue v, QByteArray& t);
bool deserialize_f(QCborValue v, QCborArray& t);
bool deserialize_f(QCborValue v, QCborValue& t);
bool deserialize_f(QCborValue v, QColor& t);
bool deserialize_f(QCborValue v, QUrl& t);
template <class T>
bool deserialize_f(QCborValue v, std::optional<T>& t);
bool deserialize_f(QCborValue v, noo::BoundingBox& t);

template <class T>
bool deserialize(QCborValue v, T& t);

//

template <class Tag>
bool deserialize_f(QCborValue v, noo::ID<Tag>& id) {
    id = noo::ID<Tag>(v);
    return true;
};

bool deserialize_f(QCborValue v, noo::InvokeID& id) {
    // we dont use an empty here as blank items still convey context: document
    static auto const entity_name = QStringLiteral("entity");
    static auto const table_name  = QStringLiteral("table");
    static auto const plot_name   = QStringLiteral("plot");

    auto m = v.toMap();

    if (m.contains(entity_name)) {
        EntityID eid;
        if (!deserialize(m[entity_name], eid)) { return false; }

        id = eid;
    } else if (m.contains(table_name)) {
        TableID eid;
        if (!deserialize(m[table_name], eid)) { return false; }

        id = eid;
    } else if (m.contains(plot_name)) {
        PlotID eid;
        if (!deserialize(m[plot_name], eid)) { return false; }

        id = eid;
    }


    return true;
};

bool deserialize_f(QCborValue v, QString& t) {
    if (!v.isString()) return false;
    t = v.toString();
    return true;
}

bool deserialize_f(QCborValue v, bool& t) {
    if (!v.isBool()) return false;
    t = v.toBool();
    return true;
}

bool deserialize_f(QCborValue v, int64_t& t) {
    if (!v.isInteger()) return false;
    t = v.toInteger();
    return true;
}

bool deserialize_f(QCborValue v, uint64_t& t) {
    if (!v.isInteger()) return false;
    t = v.toInteger();
    return true;
}

bool deserialize_f(QCborValue v, float& t) {
    if (!v.isDouble()) return false;
    t = v.toDouble();
    return true;
}

template <class T>
bool deserialize_f(QCborValue v, QVector<T>& t) {
    if (!v.isArray()) return false;
    auto arr = v.toArray();

    for (auto lv : arr) {
        T    new_t;
        bool ok = deserialize(lv, new_t);
        if (!ok) return false;
        t << new_t;
    }
    return true;
}

bool deserialize_f(QCborValue v, QStringList& t) {
    if (!v.isArray()) return false;
    auto arr = v.toArray();

    for (auto lv : arr) {
        QString new_t;
        bool    ok = deserialize(lv, new_t);
        if (!ok) return false;
        t << new_t;
    }
    return true;
}

bool deserialize_f(QCborValue v, glm::vec3& t) {
    if (!v.isArray()) return false;
    auto arr = v.toArray();

    t[0] = arr[0].toDouble();
    t[1] = arr[1].toDouble();
    t[2] = arr[2].toDouble();
    return true;
}

bool deserialize_f(QCborValue v, glm::vec4& t) {
    if (!v.isArray()) return false;
    auto arr = v.toArray();

    t[0] = arr[0].toDouble();
    t[1] = arr[1].toDouble();
    t[2] = arr[2].toDouble();
    t[3] = arr[3].toDouble();
    return true;
}

bool deserialize_f(QCborValue v, glm::mat3& t) {
    if (!v.isArray()) return false;
    t = glm::mat3(1);

    auto arr = v.toArray();

    for (int i = 0; i < arr.size(); i++) {
        glm::value_ptr(t)[i] = arr[i].toDouble();
    }

    return true;
}

bool deserialize_f(QCborValue v, glm::mat4& t) {
    if (!v.isArray()) return false;
    t = glm::mat4(1);

    auto arr = v.toArray();

    for (int i = 0; i < arr.size(); i++) {
        glm::value_ptr(t)[i] = arr[i].toDouble();
    }

    return true;
}

bool deserialize_f(QCborValue v, QByteArray& t) {
    if (!v.isByteArray()) return false;
    t = v.toByteArray();
    return true;
}
bool deserialize_f(QCborValue v, QCborArray& t) {
    if (!v.isArray()) return false;
    t = v.toArray();
    return true;
}
bool deserialize_f(QCborValue v, QCborValue& t) {
    t = v;
    return true;
}


bool deserialize_f(QCborValue v, QColor& t) {
    if (!v.isArray()) return false;
    auto arr = v.toArray();

    t.setRedF(arr[0].toDouble());
    t.setGreenF(arr[1].toDouble());
    t.setBlueF(arr[2].toDouble());
    t.setAlphaF(arr[3].toDouble(1));
    return true;
}

bool deserialize_f(QCborValue v, QUrl& t) {
    if (!v.isUrl()) return false;
    t = v.toUrl();
    return true;
}

template <class T>
bool deserialize_f(QCborValue b, std::optional<T>& t) {
    if (b.isUndefined()) return true;
    t.emplace();
    return deserialize(b, *t);
}

bool deserialize_f(QCborValue v, noo::BoundingBox& t) {
    if (!v.isMap()) return false;
    auto m = v.toMap();
    bool ok;
    ok = deserialize(m[QStringLiteral("min")], t.aabb_min);
    if (!ok) return false;
    ok = deserialize(m[QStringLiteral("max")], t.aabb_max);
    return ok;
}

struct DCBArchive {
    QCborMap map;

    bool ok = true;

    template <class T>
    void operator()(QString s, T& v) {
        if (!map.contains(s)) {
            ok = false;
            return;
        }
        ok = ok and deserialize(map[s], v);
    }
};

template <class T>
bool deserialize_f(QCborValue v, T& t) {
    if (!v.isMap()) return false;
    DCBArchive archive { v.toMap() };
    t.serialize(archive);
    return archive.ok;
}

template <class T>
bool deserialize(QCborValue v, T& t) {
    return deserialize_f(v, t);
}

// =============================================================================
// Message Serialization
// =============================================================================

template <class Archive>
void MethodArg::serialize(Archive& a) {
    NOONVP(name);
    NOONVP(doc);
    NOONVP(editor_hint);
}

template <class Archive>
void MsgMethodCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);
    NOONVP(doc);
    NOONVP(return_doc);
    NOONVP(arg_doc);
}

template <class Archive>
void MsgMethodDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgSignalCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);
    NOONVP(doc);
    NOONVP(arg_doc);
}

template <class Archive>
void MsgSignalDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void TextRepresentation::serialize(Archive& a) {
    NOONVP(txt);
    NOONVP(font);
    NOONVP(height);
    NOONVP(width);
}

template <class Archive>
void WebRepresentation::serialize(Archive& a) {
    NOONVP(source);
    NOONVP(height);
    NOONVP(width);
}

template <class Archive>
void InstanceSource::serialize(Archive& a) {
    NOONVP(view);
    NOONVP(stride);
    NOONVP(bb);
}

template <class Archive>
void RenderRepresentation::serialize(Archive& a) {
    NOONVP(mesh);
    NOONVP(instances);
}

template <class Archive>
void MsgEntityCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);
    NOONVP(parent);
    NOONVP(transform);

    NOONVP(null_rep);
    NOONVP(text_rep);
    NOONVP(web_rep);
    NOONVP(render_rep);

    NOONVP(lights);
    NOONVP(tables);
    NOONVP(plots);
    NOONVP(tags);

    NOONVP(methods_list);
    NOONVP(signals_list);
    NOONVP(influence);
}

template <class Archive>
void MsgEntityUpdate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(parent);
    NOONVP(transform);

    NOONVP(null_rep);
    NOONVP(text_rep);
    NOONVP(web_rep);
    NOONVP(render_rep);

    NOONVP(lights);
    NOONVP(tables);
    NOONVP(plots);
    NOONVP(tags);

    NOONVP(methods_list);
    NOONVP(signals_list);
    NOONVP(influence);
}

template <class Archive>
void MsgEntityDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgPlotCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);
    NOONVP(table);

    NOONVP(simple_plot);
    NOONVP(url_plot);

    NOONVP(methods_list);
    NOONVP(signals_list);
}

template <class Archive>
void MsgPlotUpdate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(table);

    NOONVP(simple_plot);
    NOONVP(url_plot);

    NOONVP(methods_list);
    NOONVP(signals_list);
}

template <class Archive>
void MsgPlotDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgBufferCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);
    NOONVP(size);

    NOONVP(inline_bytes);
    NOONVP(uri_bytes);
}

template <class Archive>
void MsgBufferDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgBufferViewCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);
    NOONVP(source_buffer);

    NOONVP(type);
    NOONVP(offset);
    NOONVP(length);
}

template <class Archive>
void MsgBufferViewDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void TextureRef::serialize(Archive& a) {
    NOONVP(texture);
    NOONVP(transform);
    NOONVP(texture_coord_slot);
}

template <class Archive>
void PBRInfo::serialize(Archive& a) {
    NOONVP(base_color);
    NOONVP(base_color_texture);

    NOONVP(metallic);
    NOONVP(roughness);
    NOONVP(metal_rough_texture);
}

template <class Archive>
void MsgMaterialCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);

    NOONVP(pbr_info);
    NOONVP(normal_texture);

    NOONVP(occlusion_texture);
    NOONVP(occlusion_texture_factor);

    NOONVP(emissive_texture);
    NOONVP(emissive_factor);

    NOONVP(use_alpha);
    NOONVP(alpha_cutoff);

    NOONVP(double_sided);
}

template <class Archive>
void MsgMaterialUpdate::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgMaterialDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgImageCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);

    NOONVP(buffer_source);
    NOONVP(uri_source);
}

template <class Archive>
void MsgImageDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgTextureCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);

    NOONVP(image);
    NOONVP(sampler);
}

template <class Archive>
void MsgTextureDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgSamplerCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);

    NOONVP(mag_filter);
    NOONVP(min_filter);

    NOONVP(wrap_s);
    NOONVP(wrap_t);
}

template <class Archive>
void MsgSamplerDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void PointLight::serialize(Archive& a) {
    NOONVP(range);
}

template <class Archive>
void SpotLight::serialize(Archive& a) {
    NOONVP(range);
    NOONVP(inner_cone_angle_rad);
    NOONVP(outer_cone_angle_rad);
}

template <class Archive>
void DirectionalLight::serialize(Archive& a) {
    NOONVP(range);
}

template <class Archive>
void MsgLightCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);

    NOONVP(color);
    NOONVP(intensity);

    NOONVP(point);
    NOONVP(spot);
    NOONVP(directional);
}

template <class Archive>
void MsgLightUpdate::serialize(Archive& a) {
    NOONVP(id);

    NOONVP(color);
    NOONVP(intensity);
}

template <class Archive>
void MsgLightDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void Attribute::serialize(Archive& a) {
    NOONVP(view);
    NOONVP(semantic);

    NOONVP(channel);
    NOONVP(offset);
    NOONVP(stride);

    NOONVP(format);

    NOONVP(minimum_value);
    NOONVP(maximum_value);

    NOONVP(normalized);
}

template <class Archive>
void Index::serialize(Archive& a) {
    NOONVP(view);
    NOONVP(offset);
    NOONVP(stride);

    NOONVP(format);
}

template <class Archive>
void GeometryPatch::serialize(Archive& a) {
    NOONVP(attributes);
    NOONVP(indicies);
    NOONVP(type);
    NOONVP(material);
}

template <class Archive>
void MsgGeometryCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);
    NOONVP(patches);
}

template <class Archive>
void MsgGeometryDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgTableCreate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(name);
    NOONVP(meta);
    NOONVP(methods_list);
    NOONVP(signals_list);
}

template <class Archive>
void MsgTableUpdate::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(meta);
    NOONVP(methods_list);
    NOONVP(signals_list);
}

template <class Archive>
void MsgTableDelete::serialize(Archive& a) {
    NOONVP(id);
}

template <class Archive>
void MsgDocumentUpdate::serialize(Archive& a) {
    NOONVP(methods_list);
    NOONVP(signals_list);
}

template <class Archive>
void MsgDocumentReset::serialize(Archive&) { }

template <class Archive>
void MsgSignalInvoke::serialize(Archive& a) {
    NOONVP(id);
    NOONVP(context);
    NOONVP(signal_data);
}

template <class Archive>
void MethodException::serialize(Archive& a) {
    NOONVP(code);
    NOONVP(message);
    NOONVP(data);
}

template <class Archive>
void MsgMethodReply::serialize(Archive& a) {
    NOONVP(invoke_id);
    NOONVP(result);
    NOONVP(method_exception);
}

template <class Archive>
void MsgIntroduction::serialize(Archive& a) {
    NOONVP(client_name);
}

bool MsgIntroduction::operator<(MsgIntroduction const& o) const {
    return client_name < o.client_name;
}
bool MsgIntroduction::operator==(MsgIntroduction const& o) const {
    return client_name == o.client_name;
}

template <class Archive>
void MsgInvokeMethod::serialize(Archive& a) {
    NOONVP(method);
    NOONVP(context);
    NOONVP(invoke_id);
    NOONVP(args);
}

bool MsgInvokeMethod::operator<(MsgInvokeMethod const& o) const {
    return std::tie(method, context, invoke_id, args) <
           std::tie(o.method, o.context, o.invoke_id, o.args);
}
bool MsgInvokeMethod::operator==(MsgInvokeMethod const& o) const {
    return std::tie(method, context, invoke_id, args) ==
           std::tie(o.method, o.context, o.invoke_id, o.args);
}

// =============================================================================
// Serialization Entry Points
// =============================================================================


template <class T>
bool extract_smsg(QCborValue const& value, ServerMessage& ret) {
    auto& t = ret.emplace<T>();
    return deserialize(value, t);
}

static bool
deserialize_server_message(int id, QCborValue v, ServerMessage& ret) {
    // now we can play some tricks with the index of type in the type list of
    // the variant, but there could be some holes in the official message id
    // list.

    switch (id) {
    case MsgMethodCreate::MID: return extract_smsg<MsgMethodCreate>(v, ret);
    case MsgMethodDelete::MID: return extract_smsg<MsgMethodDelete>(v, ret);
    case MsgSignalCreate::MID: return extract_smsg<MsgSignalCreate>(v, ret);
    case MsgSignalDelete::MID: return extract_smsg<MsgSignalDelete>(v, ret);
    case MsgEntityCreate::MID: return extract_smsg<MsgEntityCreate>(v, ret);
    case MsgEntityUpdate::MID: return extract_smsg<MsgEntityUpdate>(v, ret);
    case MsgEntityDelete::MID: return extract_smsg<MsgEntityDelete>(v, ret);
    case MsgPlotCreate::MID: return extract_smsg<MsgPlotCreate>(v, ret);
    case MsgPlotUpdate::MID: return extract_smsg<MsgPlotUpdate>(v, ret);
    case MsgPlotDelete::MID: return extract_smsg<MsgPlotDelete>(v, ret);
    case MsgBufferCreate::MID: return extract_smsg<MsgBufferCreate>(v, ret);
    case MsgBufferDelete::MID: return extract_smsg<MsgBufferDelete>(v, ret);
    case MsgBufferViewCreate::MID:
        return extract_smsg<MsgBufferViewCreate>(v, ret);
    case MsgBufferViewDelete::MID:
        return extract_smsg<MsgBufferViewDelete>(v, ret);
    case MsgMaterialCreate::MID: return extract_smsg<MsgMaterialCreate>(v, ret);
    case MsgMaterialUpdate::MID: return extract_smsg<MsgMaterialUpdate>(v, ret);
    case MsgMaterialDelete::MID: return extract_smsg<MsgMaterialDelete>(v, ret);
    case MsgImageCreate::MID: return extract_smsg<MsgImageCreate>(v, ret);
    case MsgImageDelete::MID: return extract_smsg<MsgImageDelete>(v, ret);
    case MsgTextureCreate::MID: return extract_smsg<MsgTextureCreate>(v, ret);
    case MsgTextureDelete::MID: return extract_smsg<MsgTextureDelete>(v, ret);
    case MsgSamplerCreate::MID: return extract_smsg<MsgSamplerCreate>(v, ret);
    case MsgSamplerDelete::MID: return extract_smsg<MsgSamplerDelete>(v, ret);
    case MsgLightCreate::MID: return extract_smsg<MsgLightCreate>(v, ret);
    case MsgLightUpdate::MID: return extract_smsg<MsgLightUpdate>(v, ret);
    case MsgLightDelete::MID: return extract_smsg<MsgLightDelete>(v, ret);
    case MsgGeometryCreate::MID: return extract_smsg<MsgGeometryCreate>(v, ret);
    case MsgGeometryDelete::MID: return extract_smsg<MsgGeometryDelete>(v, ret);
    case MsgTableCreate::MID: return extract_smsg<MsgTableCreate>(v, ret);
    case MsgTableUpdate::MID: return extract_smsg<MsgTableUpdate>(v, ret);
    case MsgTableDelete::MID: return extract_smsg<MsgTableDelete>(v, ret);
    case MsgDocumentUpdate::MID: return extract_smsg<MsgDocumentUpdate>(v, ret);
    case MsgDocumentReset::MID: return extract_smsg<MsgDocumentReset>(v, ret);
    case MsgSignalInvoke::MID: return extract_smsg<MsgSignalInvoke>(v, ret);
    case MsgMethodReply::MID: return extract_smsg<MsgMethodReply>(v, ret);
    default: return false;
    }
}


QVector<ServerMessage> deserialize_server(QByteArray bytes) {
    QCborParserError error;
    auto message_list = QCborValue::fromCbor(bytes, &error).toArray();

    if (error.error != QCborError::NoError) {
        qWarning() << "Bad server message:" << error.errorString();
        return {};
    }

    qDebug() << message_list;

    QVector<ServerMessage> ret;

    for (int i = 0; i < message_list.size(); i += 2) {
        auto id = message_list.at(i).toInteger(-1);

        if (id < 0) continue;

        auto content = message_list.at(i + 1);

        ServerMessage message;

        bool ok = deserialize_server_message(id, content, message);

        if (!ok) continue;

        ret.push_back(message);
    }

    return ret;
}

QByteArray serialize_server(std::span<ServerMessage> list) {
    QCborArray pack;

    for (auto& l : list) {
        noo::visit(
            [&pack]<class T>(T& t) {
                pack << (unsigned)T::MID;
                pack << serialize(t);
            },
            l);
    }

    return pack.toCborValue().toCbor();
}

template <class T>
bool extract_smsg(QCborValue const& value, ClientMessage& ret) {
    auto& t = ret.emplace<T>();
    return deserialize(value, t);
}

static bool
deserialize_client_message(int id, QCborValue v, ClientMessage& ret) {
    // now we can play some tricks with the index of type in the type list of
    // the variant, but there could be some holes in the official message id
    // list.

    switch (id) {
    case MsgIntroduction::MID: return extract_smsg<MsgIntroduction>(v, ret);
    case MsgInvokeMethod::MID: return extract_smsg<MsgInvokeMethod>(v, ret);
    default: return false;
    }
}

QVector<ClientMessage> deserialize_client(QByteArray bytes) {
    QCborParserError error;
    auto message_list = QCborValue::fromCbor(bytes, &error).toArray();

    if (error.error != QCborError::NoError) {
        qWarning() << "Bad client message:" << error.errorString();
        return {};
    }

    qDebug() << message_list;

    QVector<ClientMessage> ret;

    for (int i = 0; i < message_list.size(); i += 2) {
        auto id = message_list.at(i).toInteger(-1);

        if (id < 0) continue;

        auto content = message_list.at(i + 1);

        ClientMessage message;

        bool ok = deserialize_client_message(id, content, message);

        if (!ok) continue;

        ret.push_back(std::move(message));
    }

    return ret;
}

QByteArray serialize_client(std::span<ClientMessage> list) {
    QCborArray pack;

    for (auto& l : list) {
        noo::visit(
            [&pack]<class T>(T& t) {
                pack << (unsigned)T::MID;
                pack << serialize(t);
            },
            l);
    }

    return pack.toCborValue().toCbor();
}

} // namespace messages


// =============================

SMsgWriter::SMsgWriter() { }
SMsgWriter::~SMsgWriter() noexcept {
    flush();
}

void SMsgWriter::flush() {
    QByteArray bytes = messages::serialize_server(m_messages);

    m_messages.clear();

    emit data_ready(bytes);
}


} // namespace noo
