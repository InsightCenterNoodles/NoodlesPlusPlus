#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "include/noo_id.h"
#include "include/noo_server_interface.h"


#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QObject>

namespace noo {

inline std::optional<QString> opt_string(QString s) {
    if (!s.size()) return {};
    return s;
}

template <class T>
auto delegates_to_ids(QVector<T> const& delegates) {
    using IDType = decltype(delegates[0]->id());
    QVector<IDType> ret;
    for (auto const& d : delegates) {
        ret << d->id();
    }
    return ret;
}

template <class Iter>
auto delegates_to_ids(Iter first, Iter last) {
    using IDType = decltype((*first)->id());
    QVector<IDType> ret;
    for (auto const& d = first; first != last; ++first) {
        ret << (*d)->id();
    }
    return ret;
}

namespace messages {
#define MSGID(ID) static inline constexpr size_t MID = ID;

struct MethodArg {
    QString                name;
    std::optional<QString> doc;
    std::optional<QString> editor_hint;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgMethodCreate {
    MSGID(0);

    MethodID               id;
    QString                name;
    std::optional<QString> doc;
    std::optional<QString> return_doc;
    QVector<MethodArg>     arg_doc;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgMethodDelete {
    MSGID(1);

    MethodID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Signal Messages =============================================================

struct MsgSignalCreate {
    MSGID(2);
    SignalID               id;
    QString                name;
    std::optional<QString> doc;
    QVector<MethodArg>     arg_doc;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgSignalDelete {
    MSGID(3);
    SignalID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Entity Messages =============================================================

struct TextRepresentation {
    QString txt;
    QString font   = "Arial";
    float   height = .25;
    float   width  = -1;

    template <class Archive>
    void serialize(Archive& a);
};

struct WebRepresentation {
    QUrl  source;
    float height = .5;
    float width  = .5;

    template <class Archive>
    void serialize(Archive& a);
};

struct InstanceSource {
    BufferViewID view;
    uint64_t     stride = 0;

    std::optional<BoundingBox> bb;

    template <class Archive>
    void serialize(Archive& a);
};

struct RenderRepresentation {
    GeometryID                    mesh;
    std::optional<InstanceSource> instances;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgEntityCreate {
    MSGID(4);
    EntityID               id;
    std::optional<QString> name;

    std::optional<EntityID>  parent;
    std::optional<glm::mat4> transform;

    // ONE OF
    std::optional<float>                null_rep;
    std::optional<TextRepresentation>   text_rep;
    std::optional<WebRepresentation>    web_rep;
    std::optional<RenderRepresentation> render_rep;
    // END ONE OF

    std::optional<QVector<LightID>> lights;
    std::optional<QVector<TableID>> tables;
    std::optional<QVector<PlotID>>  plots;
    std::optional<QStringList>      tags;

    std::optional<QVector<MethodID>> methods_list;
    std::optional<QVector<SignalID>> signals_list;

    std::optional<BoundingBox> influence;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgEntityUpdate {
    MSGID(5);
    EntityID                 id;
    std::optional<EntityID>  parent;
    std::optional<glm::mat4> transform;

    // ONE OF
    std::optional<float>                null_rep;
    std::optional<TextRepresentation>   text_rep;
    std::optional<WebRepresentation>    web_rep;
    std::optional<RenderRepresentation> render_rep;
    // END ONE OF

    std::optional<QVector<LightID>> lights;
    std::optional<QVector<TableID>> tables;
    std::optional<QVector<PlotID>>  plots;
    std::optional<QStringList>      tags;

    std::optional<QVector<MethodID>> methods_list;
    std::optional<QVector<SignalID>> signals_list;

    std::optional<BoundingBox> influence;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgEntityDelete {
    MSGID(6);
    EntityID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Plot Messages ===============================================================

struct MsgPlotCreate {
    MSGID(7);
    PlotID                 id;
    std::optional<QString> name;
    std::optional<TableID> table;

    // ONE OF
    std::optional<QString> simple_plot;
    std::optional<QUrl>    url_plot;
    // END ONE OF

    std::optional<QVector<MethodID>> methods_list;
    std::optional<QVector<SignalID>> signals_list;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgPlotUpdate {
    MSGID(8);
    PlotID                 id;
    std::optional<TableID> table;

    // ONE OF
    std::optional<QString> simple_plot;
    std::optional<QUrl>    url_plot;
    // END ONE OF

    std::optional<QVector<MethodID>> methods_list;
    std::optional<QVector<SignalID>> signals_list;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgPlotDelete {
    MSGID(9);
    PlotID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Buffer Messages =============================================================

struct MsgBufferCreate {
    MSGID(10);
    BufferID               id;
    std::optional<QString> name;
    uint64_t               size;

    // ONE OF
    std::optional<QByteArray> inline_bytes;
    std::optional<QUrl>       uri_bytes;
    // END ONE OF

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgBufferDelete {
    MSGID(11);
    BufferID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Buffer Messages =============================================================

struct MsgBufferViewCreate {
    MSGID(12);
    BufferViewID           id;
    std::optional<QString> name;
    BufferID               source_buffer;

    QString  type;
    uint64_t offset = 0;
    uint64_t length = 0;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgBufferViewDelete {
    MSGID(13);
    BufferViewID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Material Messages ===========================================================

struct TextureRef {
    TextureID                texture;
    std::optional<glm::mat3> transform;
    std::optional<uint64_t>  texture_coord_slot;

    template <class Archive>
    void serialize(Archive& a);
};

struct PBRInfo {
    QColor                    base_color = Qt::white;
    std::optional<TextureRef> base_color_texture;

    float                     metallic  = 1;
    float                     roughness = 1;
    std::optional<TextureRef> metal_rough_texture;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgMaterialCreate {
    MSGID(14);
    MaterialID             id;
    std::optional<QString> name;

    PBRInfo                   pbr_info;
    std::optional<TextureRef> normal_texture;

    std::optional<TextureRef> occlusion_texture;
    std::optional<float>      occlusion_texture_factor = 1;

    std::optional<TextureRef> emissive_texture;
    std::optional<glm::vec3>  emissive_factor = glm::vec3(1);

    std::optional<bool>  use_alpha    = false;
    std::optional<float> alpha_cutoff = .5;

    std::optional<bool> double_sided = false;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgMaterialUpdate {
    MSGID(15);
    MaterialID id;
    // TBD

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgMaterialDelete {
    MSGID(16);
    MaterialID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Image Messages ==============================================================

struct MsgImageCreate {
    MSGID(17);
    ImageID                id;
    std::optional<QString> name;

    // ONE OF
    std::optional<BufferViewID> buffer_source;
    std::optional<QUrl>         uri_source;
    // END ONE OF

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgImageDelete {
    MSGID(18);
    ImageID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Texture Messages ============================================================

struct MsgTextureCreate {
    MSGID(19);
    TextureID                id;
    std::optional<QString>   name;
    ImageID                  image;
    std::optional<SamplerID> sampler;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgTextureDelete {
    MSGID(20);
    TextureID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Sampler Messages ============================================================

// MinFilters = "NEAREST" / "LINEAR" / "LINEAR_MIPMAP_LINEAR"

// SamplerMode = "CLAMP_TO_EDGE" / "MIRRORED_REPEAT" / "REPEAT"

struct MsgSamplerCreate {
    MSGID(21);
    SamplerID              id;
    std::optional<QString> name;

    std::optional<QString> mag_filter;
    std::optional<QString> min_filter;

    std::optional<QString> wrap_s;
    std::optional<QString> wrap_t;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgSamplerDelete {
    MSGID(22);
    SamplerID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Light Messages ==============================================================

struct PointLight {
    float range = -1;

    template <class Archive>
    void serialize(Archive& a);
};

struct SpotLight {
    float range                = -1;
    float inner_cone_angle_rad = 0;
    float outer_cone_angle_rad = M_PI / 4.0;

    template <class Archive>
    void serialize(Archive& a);
};

struct DirectionalLight {
    float range = -1;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgLightCreate {
    MSGID(23);
    LightID                id;
    std::optional<QString> name;

    QColor color     = Qt::white;
    float  intensity = 1;

    // ONE OF
    std::optional<PointLight>       point;
    std::optional<SpotLight>        spot;
    std::optional<DirectionalLight> directional;
    // END ONE OF

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgLightUpdate {
    MSGID(24);
    LightID id;

    std::optional<QColor> color;
    std::optional<float>  intensity;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgLightDelete {
    MSGID(25);
    LightID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Geometry Messages ===========================================================


struct Attribute {
    BufferViewID      view;
    AttributeSemantic semantic;

    std::optional<uint64_t> channel;
    std::optional<uint64_t> offset;
    std::optional<uint64_t> stride;

    Format format;

    std::optional<QVector<float>> minimum_value;
    std::optional<QVector<float>> maximum_value;

    bool normalized = false;

    template <class Archive>
    void serialize(Archive& a);
};

struct Index {
    BufferViewID            view;
    uint64_t                count;
    std::optional<uint64_t> offset;
    std::optional<uint64_t> stride;
    Format                  format;

    template <class Archive>
    void serialize(Archive& a);
};

struct GeometryPatch {
    QVector<Attribute>   attributes;
    uint64_t             vertex_count;
    std::optional<Index> indicies;
    PrimitiveType        type;
    MaterialID           material;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgGeometryCreate {
    MSGID(26);
    GeometryID             id;
    std::optional<QString> name;
    QVector<GeometryPatch> patches;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgGeometryDelete {
    MSGID(27);
    GeometryID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Table Messages ==============================================================

struct MsgTableCreate {
    MSGID(28);
    TableID                id;
    std::optional<QString> name;

    std::optional<QString> meta;

    std::optional<QVector<MethodID>> methods_list;
    std::optional<QVector<SignalID>> signals_list;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgTableUpdate {
    MSGID(29);
    TableID id;

    std::optional<QString> meta;

    std::optional<QVector<MethodID>> methods_list;
    std::optional<QVector<SignalID>> signals_list;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgTableDelete {
    MSGID(30);
    TableID id;

    template <class Archive>
    void serialize(Archive& a);
};

// Document Messages ===========================================================

struct MsgDocumentUpdate {
    MSGID(31);
    std::optional<QVector<MethodID>> methods_list;
    std::optional<QVector<SignalID>> signals_list;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgDocumentReset {
    MSGID(32);

    template <class Archive>
    void serialize(Archive&);
};

// Communication Messages ======================================================

struct MsgSignalInvoke {
    MSGID(33);
    SignalID id;

    std::optional<InvokeID> context;

    QCborArray signal_data;

    template <class Archive>
    void serialize(Archive& a);
};

struct MethodException {
    int64_t                   code;
    std::optional<QString>    message;
    std::optional<QCborValue> data;

    template <class Archive>
    void serialize(Archive& a);
};

struct MsgMethodReply {
    MSGID(34);
    QString                        invoke_id;
    std::optional<QCborValue>      result;
    std::optional<MethodException> method_exception;

    template <class Archive>
    void serialize(Archive& a);
};

struct ServerMessage : std::variant<MsgMethodCreate,
                                    MsgMethodDelete,
                                    MsgSignalCreate,
                                    MsgSignalDelete,
                                    MsgEntityCreate,
                                    MsgEntityUpdate,
                                    MsgEntityDelete,
                                    MsgPlotCreate,
                                    MsgPlotUpdate,
                                    MsgPlotDelete,
                                    MsgBufferCreate,
                                    MsgBufferDelete,
                                    MsgBufferViewCreate,
                                    MsgBufferViewDelete,
                                    MsgMaterialCreate,
                                    MsgMaterialUpdate,
                                    MsgMaterialDelete,
                                    MsgImageCreate,
                                    MsgImageDelete,
                                    MsgTextureCreate,
                                    MsgTextureDelete,
                                    MsgSamplerCreate,
                                    MsgSamplerDelete,
                                    MsgLightCreate,
                                    MsgLightUpdate,
                                    MsgLightDelete,
                                    MsgGeometryCreate,
                                    MsgGeometryDelete,
                                    MsgTableCreate,
                                    MsgTableUpdate,
                                    MsgTableDelete,
                                    MsgDocumentUpdate,
                                    MsgDocumentReset,
                                    MsgSignalInvoke,
                                    MsgMethodReply> {
    using variant::variant;
};

QVector<ServerMessage> deserialize_server(QByteArray);
QByteArray             serialize_server(std::span<ServerMessage>);


// =============================================================================
// Client messages
// =============================================================================

struct MsgIntroduction {
    MSGID(0);
    QString client_name;

    template <class Archive>
    void serialize(Archive& a);

    bool operator<(MsgIntroduction const&) const;
    bool operator==(MsgIntroduction const&) const;
};

struct MsgInvokeMethod {
    MSGID(1);
    MethodID                method;
    std::optional<InvokeID> context;
    std::optional<QString>  invoke_id;
    QCborArray              args;

    template <class Archive>
    void serialize(Archive& a);

    bool operator<(MsgInvokeMethod const&) const;
    bool operator==(MsgInvokeMethod const&) const;
};

struct ClientMessage : std::variant<MsgIntroduction, MsgInvokeMethod> {
    using variant::variant;
};

QVector<ClientMessage> deserialize_client(QByteArray);
QByteArray             serialize_client(std::span<ClientMessage>);


} // namespace messages

// =============================================================================
// Writer
// =============================================================================

class SMsgWriter : public QObject {
    Q_OBJECT

    QVector<messages::ServerMessage> m_messages;

public:
    SMsgWriter();
    ~SMsgWriter() noexcept override;

    template <class T>
    void add(T const& message) {
        m_messages << message;
    }

    void flush();

signals:
    void data_ready(QByteArray);
};

} // namespace noo

#endif // SERIALIZE_H
