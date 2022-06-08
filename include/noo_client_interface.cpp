#include "noo_client_interface.h"

#include "noo_common.h"
#include "noo_interface_types.h"
#include "src/client/client_common.h"
#include "src/client/clientstate.h"
#include "src/common/serialize.h"
#include "src/common/variant_tools.h"

#include <QWebSocket>

#include <optional>
#include <qcbormap.h>
#include <qcborvalue.h>
#include <qglobal.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qpoint.h>
#include <qstringliteral.h>
#include <unordered_set>
#include <variant>

namespace nooc {

template <class T>
bool decode_entity(InternalClientState& state, QCborValue rid, T*& out_ptr) {
    using IDType = decltype(out_ptr->id());
    auto new_id  = IDType(rid);
    out_ptr      = state.lookup(new_id);
    return true;
}

template <class T, class U>
bool decode_entity(InternalClientState& state, U rid, T*& out_ptr) {
    using IDType = decltype(out_ptr->id());
    auto new_id  = IDType(rid);
    out_ptr      = state.lookup(new_id);
    return true;
}

template <class T>
bool decode_entity(InternalClientState& state,
                   QCborValue           rid,
                   QPointer<T>&         out_ptr) {
    using IDType = decltype(out_ptr->id());
    auto new_id  = IDType(rid);
    out_ptr      = state.lookup(new_id);
    return true;
}

template <class T>
bool decode_entity(InternalClientState& state,
                   QCborValue           value,
                   std::vector<T*>&     out_ptr) {
    // using IDType = decltype(out_ptr[0]->id());
    auto arr = value.toArray();
    out_ptr.clear();
    for (auto lv : arr) {
        T* ptr = nullptr;
        decode_entity(state, lv, ptr);
        out_ptr.push_back(ptr);
    }
    return true;
}

template <class T>
bool decode_entity(InternalClientState& state,
                   QCborValue           value,
                   std::optional<T>&    out) {
    if (value.isUndefined()) return true;
    if constexpr (std::is_constructible_v<T, QCborMap, InternalClientState&>) {
        out = T(value.toMap(), state);
    } else {
        auto& lt = out.emplace();
        return decode_entity(state, value, lt);
    }
    __builtin_unreachable();
}

template <class T, class U>
void convert(T const& in, QVector<U>& out, InternalClientState& state) {
    // input better be some array
    for (auto const& v : in) {
        if constexpr (std::is_constructible_v<U,
                                              std::remove_cvref_t<decltype(v)>,
                                              InternalClientState&>) {
            out << U(v, state);
        } else {
            U u;
            decode_entity(state, v, u);
            out << u;
        }
    }
}

template <class T, class U>
void convert(T id, QPointer<U>& out, InternalClientState& state) {
    out = state.lookup(id);
}

template <class T, class U>
void convert(std::optional<T> const& in,
             QVector<U>&             out,
             InternalClientState&    state) {
    if (!in) return;
    convert(*in, out, state);
}


template <class T, class U>
void convert(std::optional<T>     id,
             QPointer<U>&         out,
             InternalClientState& state) {
    if (id) out = state.lookup(*id);
}

template <class T>
void convert(std::optional<T> const& in, T& out, InternalClientState&) {
    out = in.value_or(T());
}


// =============================================================================

MethodException::MethodException(noo::messages::MethodException const& m,
                                 InternalClientState&) {
    code    = m.code;
    message = m.message.value_or(QString());
    if (m.data) additional = m.data.value();
}

QString to_string(MethodException const& me) {
    auto str = me.additional.toDiagnosticNotation();
    return QString("Code %1: %2 Additional: %3")
        .arg(me.code)
        .arg(me.message)
        .arg(str);
}

void PendingMethodReply::interpret() { }

PendingMethodReply::PendingMethodReply(MethodDelegate* d, MethodContextPtr ctx)
    : m_method(d) {

    std::visit([&](auto x) { m_context = x; }, ctx);
}

void PendingMethodReply::call_direct(QCborArray l) {
    if (m_called) {
        MethodException exception;
        exception.code = -10000;
        exception.message =
            "Create a new invocation object instead of re-calling "
            "this one";

        return complete({}, &exception);
    }

    m_called = true;


    if (!m_method) {
        MethodException exception;
        exception.code    = noo::ErrorCodes::METHOD_NOT_FOUND;
        exception.message = "Method does not exist on this object";

        return complete({}, &exception);
    }

    // qDebug() << "Invoking" << noo::to_qstring(m_method->name());

    VMATCH(
        m_context,
        VCASE(std::monostate) {
            emit m_method->invoke(m_method->id(), std::monostate(), l, this);
        },
        VCASE(QPointer<EntityDelegate> & p) {
            if (!p) {
                MethodException exception;
                exception.code = noo::ErrorCodes::METHOD_NOT_FOUND;
                exception.message =
                    "Method does not exist on this object anymore";
                return complete({}, &exception);
            }

            emit m_method->invoke(m_method->id(), p.data(), l, this);
        },
        VCASE(QPointer<TableDelegate> & p) {
            if (!p) {
                MethodException exception;

                exception.code = noo::ErrorCodes::METHOD_NOT_FOUND;
                exception.message =
                    "Method does not exist on this table anymore";

                return complete({}, &exception);
            }

            emit m_method->invoke(m_method->id(), p.data(), l, this);
        },
        VCASE(QPointer<PlotDelegate> & p) {
            if (!p) {
                MethodException exception;

                exception.code = noo::ErrorCodes::METHOD_NOT_FOUND;
                exception.message =
                    "Method does not exist on this table anymore";
                return complete({}, &exception);
            }

            emit m_method->invoke(m_method->id(), p.data(), l, this);
        }

    );
}

void PendingMethodReply::complete(QCborValue       v,
                                  MethodException* exception_info) {
    qDebug() << Q_FUNC_INFO;

    if (exception_info) {
        emit recv_exception(*exception_info);
        emit recv_fail(to_string(*exception_info));
        return;
    }

    m_var = std::move(v);
    emit recv_data(m_var);
    interpret();
    this->deleteLater();
}

namespace replies {

void GetIntegerReply::interpret() {
    if (m_var.isInteger()) {
        emit recv(m_var.toInteger());
    } else {
        auto s = m_var.toDiagnosticNotation();
        emit recv_fail("Wrong result type expected" + s);
    }
}

void GetStringReply::interpret() {
    if (m_var.isString()) {
        emit recv(m_var.toString());
    } else {
        auto s = m_var.toDiagnosticNotation();
        emit recv_fail("Wrong result type expected" + s);
    }
}

void GetStringListReply::interpret() {
    QStringList list;

    noo::from_cbor(m_var, list);

    if (list.empty()) {
        auto s = m_var.toDiagnosticNotation();
        emit recv_fail("Wrong result type expected" + s);
    } else {
        emit recv(list);
    }
}

void GetRealsReply::interpret() {
    auto list = noo::coerce_to_real_list(m_var);

    if (list.size()) {
        emit recv(list);
    } else {
        auto s = m_var.toDiagnosticNotation();
        emit recv_fail("Wrong result type expected" + s);
    }
}

} // namespace replies

// =============================================================================

AttachedMethodList::AttachedMethodList(MethodContextPtr c) : m_context(c) { }

MethodDelegate* AttachedMethodList::find_direct_by_name(QString name) const {
    // try to find it

    auto iter =
        std::find_if(m_list.begin(), m_list.end(), [name](auto const& v) {
            return v->name() == name;
        });

    if (iter == m_list.end()) { return nullptr; }

    return (*iter);
}

bool AttachedMethodList::check_direct_by_delegate(MethodDelegate* p) const {
    // try to find it

    auto iter = std::find(m_list.begin(), m_list.end(), p);

    if (iter == m_list.end()) { return false; }

    return true;
}

// =============================================================================

AttachedSignal::AttachedSignal(SignalDelegate* m) : m_signal(m) { }

SignalDelegate* AttachedSignal::delegate() const {
    return m_signal;
}

AttachedSignalList::AttachedSignalList(MethodContextPtr c) : m_context(c) { }

AttachedSignalList&
AttachedSignalList::operator=(QVector<SignalDelegate*> const& l) {
    // we want to preserve signals already attached. so we want to check if the
    // new list has signals we already have.

    std::unordered_set<SignalDelegate*> new_list;
    for (auto const& p : l) {
        if (!p) continue;
        new_list.insert(p);
    }


    QVector<std::shared_ptr<AttachedSignal>> to_keep;

    for (int i = 0; i < m_list.size(); i++) {
        auto const p   = m_list[i];
        auto*      ptr = p->delegate();
        assert(ptr);
        if (new_list.contains(ptr)) {
            to_keep << p;
            new_list.erase(ptr);
        }
    }

    m_list = std::move(to_keep);

    for (auto const& p : new_list) {
        m_list << std::make_shared<AttachedSignal>(p);
    }
    return *this;
}

AttachedSignal* AttachedSignalList::find_by_name(QString name) const {
    for (auto const& p : m_list) {
        if (p->delegate()->name() == name) return p.get();
    }
    return nullptr;
}

AttachedSignal*
AttachedSignalList::find_by_delegate(SignalDelegate* ptr) const {
    for (auto const& p : m_list) {
        if (p->delegate() == ptr) return p.get();
    }
    return nullptr;
}

// =============================================================================

#define BASIC_DELEGATE_IMPL(C, ID, DATA)                                       \
    C::C(noo::ID i, DATA const&) : m_id(i) { }                                 \
    C::~C() = default;                                                         \
    noo::ID C::id() const {                                                    \
        return m_id;                                                           \
    }

// =============================================================================

ArgDoc::ArgDoc(noo::messages::MethodArg const& m, InternalClientState&) {
    name        = m.name;
    doc         = m.doc.value_or(QString());
    editor_hint = m.editor_hint.value_or(QString());
}

MethodInit::MethodInit(noo::messages::MsgMethodCreate const& m,
                       InternalClientState&                  state) {
    method_name          = m.name;
    documentation        = m.doc.value_or(QString());
    return_documentation = m.return_doc.value_or(QString());

    convert(m.arg_doc, argument_documentation, state);
}

MethodDelegate::MethodDelegate(noo::MethodID i, MethodInit const& d)
    : m_id(i), m_data(d) { }
MethodDelegate::~MethodDelegate() = default;

void MethodDelegate::post_create(InternalClientState& state) {
    QObject::connect(this,
                     &MethodDelegate::invoke,
                     &state,
                     &InternalClientState::on_method_ask_invoke);

    on_complete();
}

noo::MethodID MethodDelegate::id() const {
    return m_id;
}

void MethodDelegate::on_complete() { }

// =============================================================================

SignalInit::SignalInit(noo::messages::MsgSignalCreate const& m,
                       InternalClientState&                  state) {
    name          = m.name;
    documentation = m.doc.value_or(QString());
    convert(m.arg_doc, argument_documentation, state);
}

SignalDelegate::SignalDelegate(noo::SignalID i, SignalInit const& d)
    : m_id(i), m_data(d) { }
SignalDelegate::~SignalDelegate() = default;
noo::SignalID SignalDelegate::id() const {
    return m_id;
}

void SignalDelegate::on_complete() { }

// =============================================================================

BufferInit::BufferInit(noo::messages::MsgBufferCreate const& m,
                       InternalClientState&) {

    name       = m.name.value_or(QString());
    byte_count = m.size;

    if (m.uri_bytes) url = m.uri_bytes.value();
    if (m.inline_bytes) inline_bytes = m.inline_bytes.value();
}

BufferDelegate::BufferDelegate(noo::BufferID i, BufferInit const& data)
    : m_id(i), m_data(data) {
    if (m_data.inline_bytes.size()) { m_buffer_bytes = m_data.inline_bytes; }
}
BufferDelegate::~BufferDelegate() = default;
noo::BufferID BufferDelegate::id() const {
    return m_id;
}

void BufferDelegate::on_complete() { }

void BufferDelegate::post_create(InternalClientState& state) {
    if (is_data_ready()) {
        emit data_progress(m_buffer_bytes.size(), m_buffer_bytes.size());
        emit data_ready(m_buffer_bytes);
        on_complete();
        return;
    }

    auto* p = new URLFetch(state.network_manager(), m_data.url);

    connect(p, &URLFetch::completed, this, &BufferDelegate::ready_read);
    connect(p, &URLFetch::error, this, &BufferDelegate::on_error);
    connect(p, &URLFetch::progress, this, &BufferDelegate::data_progress);
}

void BufferDelegate::ready_read(QByteArray data) {
    m_buffer_bytes = data;
    emit data_ready(m_buffer_bytes);
    on_complete();
}
void BufferDelegate::on_error(QString s) {
    qWarning() << "Download failed:" << s;
}

// =============================================================================

BufferViewInit::BufferViewInit(noo::messages::MsgBufferViewCreate const& m,
                               InternalClientState& state) {
    convert(m.name, name, state);
    convert(m.source_buffer, source_buffer, state);
    offset = m.offset;
    length = m.length;

    static const QHash<QString, ViewType> type_hash = {
        { "UNK", UNKNOWN },
        { "GEOMETRY", GEOMETRY_INFO },
        { "IMAGE", IMAGE_INFO },
    };
    type = type_hash.value(m.type);
}

BufferViewDelegate::BufferViewDelegate(noo::BufferViewID     i,
                                       BufferViewInit const& data)
    : m_id(i), m_init(data) {
    connect(m_init.source_buffer,
            &BufferDelegate::data_ready,
            this,
            &BufferViewDelegate::ready_read);
}
BufferViewDelegate::~BufferViewDelegate() = default;

void BufferViewDelegate::post_create() {
    if (is_data_ready()) on_complete();
}

noo::BufferViewID BufferViewDelegate::id() const {
    return m_id;
}

bool BufferViewDelegate::is_data_ready() const {
    return m_init.source_buffer->is_data_ready();
}

BufferDelegate* BufferViewDelegate::source_buffer() const {
    return m_init.source_buffer;
}

QByteArray BufferViewDelegate::get_sub_range() const {
    return m_init.source_buffer->data().mid(m_init.offset, m_init.length);
}

void BufferViewDelegate::on_complete() { }

void BufferViewDelegate::ready_read(QByteArray) {
    emit data_ready(get_sub_range());
    on_complete();
}

// =============================================================================

ImageInit::ImageInit(noo::messages::MsgImageCreate const& m,
                     InternalClientState&                 state) {
    convert(m.name, name, state);

    if (m.buffer_source) {
        convert(*m.buffer_source, local_image, state);
    } else if (m.uri_source) {
        remote_image = *m.uri_source;
    } else {
        //
    }
}

ImageDelegate::ImageDelegate(noo::ImageID i, ImageInit const& data)
    : m_id(i), m_init(data) { }
ImageDelegate::~ImageDelegate() = default;

noo::ImageID ImageDelegate::id() const {
    return m_id;
}

static QImage convert_image(QByteArray array) {
    return QImage::fromData(array);
}

void ImageDelegate::post_create(InternalClientState& state) {
    if (m_init.local_image) {

        connect(m_init.local_image,
                &BufferViewDelegate::data_ready,
                this,
                &ImageDelegate::ready_read);

        if (m_init.local_image->is_data_ready()) {
            ready_read(m_init.local_image->get_sub_range());
        }

        return;
    }

    if (!m_init.remote_image.isValid()) return;

    auto* p = new URLFetch(state.network_manager(), m_init.remote_image);

    connect(p, &URLFetch::completed, this, &ImageDelegate::ready_read);
    connect(p, &URLFetch::error, this, &ImageDelegate::on_error);
    connect(p, &URLFetch::progress, this, &ImageDelegate::data_progress);
}

void ImageDelegate::on_complete() { }

void ImageDelegate::ready_read(QByteArray array) {
    m_image = convert_image(array);

    emit data_ready(m_image);
    on_complete();
}
void ImageDelegate::on_error(QString error) {
    qWarning() << "Download failed:" << error;
}

// =============================================================================

SamplerInit::SamplerInit(noo::messages::MsgSamplerCreate const& m,
                         InternalClientState&                   state) {
    convert(m.name, name, state);

    static const QHash<QString, MagFilter> mag_hash = {
        { "NEAREST", MagFilter::NEAREST },
        { "LINEAR", MagFilter::LINEAR },
    };

    static const QHash<QString, MinFilter> min_hash = {
        { "NEAREST", MinFilter::NEAREST },
        { "LINEAR", MinFilter::LINEAR },
        { "LINEAR_MIPMAP_LINEAR", MinFilter::LINEAR_MIPMAP_LINEAR },
    };

    static const QHash<QString, SamplerMode> samp_hash = {
        { "CLAMP_TO_EDGE", SamplerMode::CLAMP_TO_EDGE },
        { "MIRRORED_REPEAT", SamplerMode::MIRRORED_REPEAT },
        { "REPEAT", SamplerMode::REPEAT },
    };
}

SamplerDelegate::SamplerDelegate(noo::SamplerID i, SamplerInit const& data)
    : m_id(i), m_init(data) { }
SamplerDelegate::~SamplerDelegate() = default;
noo::SamplerID SamplerDelegate::id() const {
    return m_id;
}

void SamplerDelegate::on_complete() { }

// =============================================================================

TextureInit::TextureInit(noo::messages::MsgTextureCreate const& m,
                         InternalClientState&                   state) {
    convert(m.name, name, state);
    convert(m.image, image, state);
    convert(m.sampler, sampler, state);
}

TextureDelegate::TextureDelegate(noo::TextureID i, TextureInit const& data)
    : m_id(i), m_init(data) { }
TextureDelegate::~TextureDelegate() = default;

void TextureDelegate::post_create() {
    if (m_init.image) {

        if (m_init.image->is_data_ready()) {
            ready_read(m_init.image->image());

        } else {
            connect(m_init.image,
                    &ImageDelegate::data_ready,
                    this,
                    &TextureDelegate::data_ready);
        }
    }
}

noo::TextureID TextureDelegate::id() const {
    return m_id;
}

void TextureDelegate::on_complete() { }

void TextureDelegate::ready_read(QImage img) {
    emit data_ready(img);
    on_complete();
}

// =============================================================================

template <class T>
void convert(T const&                   opt,
             std::optional<TextureRef>& out,
             InternalClientState&       state) {
    if (opt) { out.emplace(*opt, state); }
}

TextureRef::TextureRef(noo::messages::TextureRef const& m,
                       InternalClientState&             state) {
    convert(m.texture, texture, state);
    convert(m.transform, transform, state);
    texture_coord_slot = m.texture_coord_slot.value_or(texture_coord_slot);
}

PBRInfo::PBRInfo(noo::messages::PBRInfo const& m, InternalClientState& state) {
    base_color = m.base_color;

    convert(m.base_color_texture, base_color_texture, state);

    metallic  = m.metallic;
    roughness = m.roughness;

    convert(m.metal_rough_texture, metal_rough_texture, state);
}

MaterialInit::MaterialInit(noo::messages::MsgMaterialCreate const& m,
                           InternalClientState&                    state)
    : pbr_info(m.pbr_info, state) {

    convert(m.name, name, state);

    convert(m.normal_texture, normal_texture, state);

    convert(m.occlusion_texture, occlusion_texture, state);
    if (m.occlusion_texture_factor) {
        occlusion_texture_factor = m.occlusion_texture_factor.value();
    }

    convert(m.emissive_texture, emissive_texture, state);
    if (m.emissive_factor) { emissive_factor = m.emissive_factor.value(); }

    use_alpha    = m.use_alpha.value_or(false);
    alpha_cutoff = m.alpha_cutoff.value_or(.5);
    double_sided = m.double_sided.value_or(false);
}

MaterialDelegate::MaterialDelegate(noo::MaterialID i, MaterialInit const& data)
    : m_id(i), m_init(data) {
    // for any textures, we need to link up

    auto test_texture = [&](std::optional<TextureRef> otex) {
        if (!otex) return;

        auto const& tr = *otex;

        TextureDelegate* tex = tr.texture;

        if (!tex) return;

        if (tex->is_data_ready()) {
            // textures are immutable, sooooo
            return;
        }

        m_unready_textures++;

        connect(tex,
                &TextureDelegate::data_ready,
                this,
                &MaterialDelegate::texture_ready);
    };

    test_texture(m_init.pbr_info.base_color_texture);
    test_texture(m_init.pbr_info.metal_rough_texture);
    test_texture(m_init.normal_texture);
    test_texture(m_init.occlusion_texture);
    test_texture(m_init.emissive_texture);
}
MaterialDelegate::~MaterialDelegate() = default;

void MaterialDelegate::post_create() {
    if (m_unready_textures == 0) {
        emit all_textures_ready();
        on_complete();
    }
}

noo::MaterialID MaterialDelegate::id() const {
    return m_id;
}
void MaterialDelegate::update(MaterialUpdate const& d) {
    this->on_update(d);
    emit updated();
}
void MaterialDelegate::on_update(MaterialUpdate const&) { }
void MaterialDelegate::on_complete() { }

void MaterialDelegate::texture_ready(QImage) {
    Q_ASSERT(m_unready_textures > 0);
    m_unready_textures--;
    if (!m_unready_textures) {
        emit all_textures_ready();
        on_complete();
    }
}

// =============================================================================

PointLight::PointLight(noo::messages::PointLight const& pl,
                       InternalClientState&) {
    range = pl.range;
}

SpotLight::SpotLight(noo::messages::SpotLight const& sl, InternalClientState&) {
    range                = sl.range;
    inner_cone_angle_rad = sl.inner_cone_angle_rad;
    outer_cone_angle_rad = sl.outer_cone_angle_rad;
}

DirectionLight::DirectionLight(noo::messages::DirectionalLight const& dl,
                               InternalClientState&) {
    range = dl.range;
}

LightInit::LightInit(noo::messages::MsgLightCreate const& m,
                     InternalClientState&                 state) {
    convert(m.name, name, state);

    if (m.point) {
        this->type = PointLight(*m.point, state);
    } else if (m.spot) {
        this->type = SpotLight(*m.spot, state);
    } else if (m.directional) {
        this->type = DirectionLight(*m.directional, state);
    }

    color     = m.color;
    intensity = m.intensity;
}

LightUpdate::LightUpdate(noo::messages::MsgLightUpdate const& m,
                         InternalClientState&) {

    if (m.color) { color = m.color; }
    if (m.intensity) { intensity = m.intensity; }
}

LightDelegate::LightDelegate(noo::LightID i, LightInit const& data)
    : m_id(i), m_init(data) { }

LightDelegate::~LightDelegate() = default;

noo::LightID LightDelegate::id() const {
    return m_id;
}

void LightDelegate::update(LightUpdate const& d) {
    this->on_update(d);
    emit updated();
}

void LightDelegate::on_update(LightUpdate const&) { }

void LightDelegate::on_complete() { }

// =============================================================================

static QHash<QString, Format> fmt_hash = {
    { "U8", Format::U8 },           { "U16", Format::U16 },
    { "U32", Format::U32 },         { "U8VEC4", Format::U8VEC4 },
    { "U16VEC2", Format::U16VEC2 }, { "VEC2", Format::VEC2 },
    { "VEC3", Format::VEC3 },       { "VEC4", Format::VEC4 },
    { "MAT3", Format::MAT3 },       { "MAT4", Format::MAT4 },
};

Attribute::Attribute(noo::messages::Attribute const& m,
                     InternalClientState&            state) {
    convert(m.view, view, state);

    static QHash<QString, AttributeSemantic> attrib_hash = {
        { "POSITION", AttributeSemantic::POSITION },
        { "NORMAL", AttributeSemantic::NORMAL },
        { "TANGENT", AttributeSemantic::TANGENT },
        { "TEXTURE", AttributeSemantic::TEXTURE },
        { "COLOR", AttributeSemantic::COLOR },
    };

    semantic = attrib_hash.value(m.semantic);

    if (m.channel) channel = *m.channel;
    if (m.offset) offset = *m.offset;
    if (m.stride) stride = *m.stride;
    format = fmt_hash.value(m.format);

    if (m.maximum_value) maximum_value = *m.maximum_value;
    if (m.minimum_value) minimum_value = *m.minimum_value;

    normalized = m.normalized;
}

Index::Index(noo::messages::Index const& m, InternalClientState& state) {
    convert(m.view, view, state);
    count = m.count;
    if (m.offset) offset = *m.offset;
    if (m.stride) stride = m.stride.value();

    format = fmt_hash.value(m.format);

    if (view) {
        connect(view, &BufferViewDelegate::data_ready, this, &Index::ready);
    }
}

bool Index::is_ready() {
    return view->is_data_ready();
}

MeshPatch::MeshPatch(noo::messages::GeometryPatch const& m,
                     InternalClientState&                state) {

    for (auto const& lm : m.attributes) {
        attributes << Attribute(lm, state);

        auto& b = attributes.back();

        if (!b.view->is_data_ready()) {
            m_unready_buffers++;
            connect(b.view,
                    &BufferViewDelegate::data_ready,
                    this,
                    &MeshPatch::on_buffer_ready);
        }
    }

    count = m.vertex_count;

    if (m.indicies) {
        indicies = new Index(m.indicies.value(), state);
        indicies->setParent(this);
        m_unready_buffers += !indicies->view->is_data_ready();

        connect(indicies->view,
                &BufferViewDelegate::data_ready,
                this,
                &MeshPatch::on_buffer_ready);
    }

    static QHash<QString, PrimitiveType> prim_hash = {
        { "POINTS", PrimitiveType::POINTS },
        { "LINES", PrimitiveType::LINES },
        { "LINE_LOOP", PrimitiveType::LINE_LOOP },
        { "LINE_STRIP", PrimitiveType::LINE_STRIP },
        { "TRIANGLES", PrimitiveType::TRIANGLES },
        { "TRIANGLE_STRIP", PrimitiveType::TRIANGLE_STRIP },
        { "TRIANGLE_FAN", PrimitiveType::TRIANGLE_FAN },
    };

    type = prim_hash.value(m.type);

    convert(m.material, material, state);
}

void MeshPatch::on_buffer_ready() {
    Q_ASSERT(m_unready_buffers);
    m_unready_buffers--;
    if (!m_unready_buffers) { emit ready(); }
}

MeshInit::MeshInit(noo::messages::MsgGeometryCreate const& m,
                   InternalClientState&                    state) {
    convert(m.name, name, state);

    for (auto const& lm : m.patches) {
        auto* p = new MeshPatch(lm, state);
        p->setParent(this);
        patches << p;
    }
}

MeshDelegate::MeshDelegate(noo::GeometryID i, MeshInit const& data)
    : m_id(i),
      m_init(&data) // gross, but we are doing it this way for consistency
{
    for (auto const& p : m_init->patches) {
        if (!p->is_ready()) {
            m_patch_unready++;
            connect(p, &MeshPatch::ready, this, &MeshDelegate::on_patch_ready);
        }
    }
}

MeshDelegate::~MeshDelegate() = default;

void MeshDelegate::post_create() {
    if (is_complete()) {
        emit ready();
        on_complete();
    }
}

noo::GeometryID MeshDelegate::id() const {
    return m_id;
}

void MeshDelegate::on_patch_ready() {
    Q_ASSERT(m_patch_unready > 0);
    m_patch_unready--;
    if (m_patch_unready == 0) {
        emit ready();
        on_complete();
    }
}

void MeshDelegate::on_complete() { }


// =============================================================================

template <class T, class U>
void convert(std::optional<QVector<T>> const& in_id_list,
             std::optional<QVector<U*>>&      out_list,
             InternalClientState&             state) {
    if (!in_id_list) return;

    auto& out_unpacked = out_list.emplace();

    for (auto const& lv : in_id_list.value()) {
        out_unpacked << state.lookup(lv);
    }
}

TableInit::TableInit(noo::messages::MsgTableCreate const& m,
                     InternalClientState&                 state) {
    convert(m.name, name, state);
    convert(m.methods_list, methods_list, state);
    convert(m.signals_list, signals_list, state);
}

TableUpdate::TableUpdate(noo::messages::MsgTableUpdate const& m,
                         InternalClientState&                 state) {
    convert(m.methods_list, methods_list, state);
    convert(m.signals_list, signals_list, state);
}

TableDelegate::ColumnInfo::ColumnInfo(QCborMap map) {
    noo::CborDecoder decoder(map);

    decoder("name", name);
    decoder("type", type);
}

TableDelegate::TableDelegate(noo::TableID i, TableInit const& data)
    : m_id(i),
      m_init(data),
      m_attached_methods(this),
      m_attached_signals(this) {

    m_attached_methods = m_init.methods_list;
    m_attached_signals = m_init.signals_list;
}
TableDelegate::~TableDelegate() = default;

void TableDelegate::post_create() {
    on_complete();
}

void TableDelegate::update(TableUpdate const& data) {

    if (data.methods_list) {
        m_init.methods_list = *data.methods_list;
        m_attached_methods  = *data.methods_list;
    }

    if (data.signals_list) {
        m_init.signals_list = *data.signals_list;

        for (auto c : m_spec_signals) {
            disconnect(c);
        }

        m_attached_signals = *data.signals_list;

        // find specific signals

        auto find_sig = [&](QString n, auto targ) {
            auto* p = m_attached_signals.find_by_name(n);

            if (p) {
                auto c = connect(p, &AttachedSignal::fired, this, targ);
                m_spec_signals.push_back(c);
            }
        };


        find_sig(noo::names::sig_tbl_reset, &TableDelegate::interp_table_reset);
        find_sig(noo::names::sig_tbl_updated,
                 &TableDelegate::interp_table_update);
        find_sig(noo::names::sig_tbl_rows_removed,
                 &TableDelegate::interp_table_remove);
        find_sig(noo::names::sig_tbl_selection_updated,
                 &TableDelegate::interp_table_sel_update);
    }

    this->on_update(data);
    emit updated();
}

void TableDelegate::on_update(TableUpdate const&) { }
void TableDelegate::on_complete() { }

AttachedMethodList const& TableDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& TableDelegate::attached_signals() const {
    return m_attached_signals;
}
noo::TableID TableDelegate::id() const {
    return m_id;
}

void TableDelegate::on_table_initialize(QVector<ColumnInfo> const&,
                                        QVector<int64_t>,
                                        QVector<QCborArray> const&,
                                        QVector<noo::Selection>) { }

void TableDelegate::on_table_reset() { }
void TableDelegate::on_table_updated(QVector<int64_t>, QCborArray) { }
void TableDelegate::on_table_rows_removed(QVector<int64_t>) { }
void TableDelegate::on_table_selection_updated(noo::Selection const&) { }

PendingMethodReply* TableDelegate::subscribe() const {
    auto* p = attached_methods().new_call_by_name<SubscribeInitReply>(
        "tbl_subscribe");

    connect(p,
            &SubscribeInitReply::recv,
            this,
            &TableDelegate::on_table_initialize);

    p->call();

    return p;
}


PendingMethodReply* TableDelegate::request_row_insert(QCborArray row) const {
    QCborArray new_list;

    for (int i = 0; i < row.size(); i++) {
        new_list << QCborArray({ row[i] });
    }

    return request_rows_insert(new_list);
}

PendingMethodReply*
TableDelegate::request_rows_insert(QCborArray columns) const {
    auto* p = attached_methods().new_call_by_name("tbl_insert");

    p->call_direct(columns);

    return p;
}

PendingMethodReply* TableDelegate::request_row_update(int64_t    key,
                                                      QCborArray row) const {
    QCborArray new_list;

    for (int i = 0; i < row.size(); i++) {
        new_list << QCborArray({ row[i] });
    }

    return request_rows_update({ key }, std::move(new_list));
}

PendingMethodReply*
TableDelegate::request_rows_update(QVector<int64_t> keys,
                                   QCborArray       columns) const {
    auto* p = attached_methods().new_call_by_name("tbl_update");

    p->call(keys, columns);

    return p;
}

PendingMethodReply*
TableDelegate::request_deletion(QVector<int64_t> keys) const {
    auto* p = attached_methods().new_call_by_name("tbl_remove");

    p->call(keys);

    return p;
}

PendingMethodReply* TableDelegate::request_clear() const {
    auto* p = attached_methods().new_call_by_name("tbl_clear");

    p->call();

    return p;
}

PendingMethodReply*
TableDelegate::request_selection_update(noo::Selection selection) const {
    auto* p = attached_methods().new_call_by_name("tbl_update_selection");

    p->call(selection.to_cbor());

    return p;
}

void TableDelegate::interp_table_reset(QCborArray const&) {
    this->on_table_reset();
}

void TableDelegate::interp_table_update(QCborArray const& ref) {
    // row of keys, then row of columns
    if (ref.size() < 2) {
        qWarning() << Q_FUNC_INFO << "Malformed signal from server";
        return;
    }

    auto keylist = ref[0];
    auto cols    = ref[1];

    this->on_table_updated(noo::coerce_to_int_list(keylist), cols.toArray());
}

void TableDelegate::interp_table_remove(QCborArray const& ref) {
    if (ref.size() < 1) {
        qWarning() << Q_FUNC_INFO << "Malformed signal from server";
        return;
    }

    this->on_table_rows_removed(noo::coerce_to_int_list(ref[0]));
}

void TableDelegate::interp_table_sel_update(QCborValue const& ref) {
    auto sel_ref = noo::Selection(ref.toMap());

    this->on_table_selection_updated(sel_ref);
}


// =============================================================================

EntityTextDefinition::EntityTextDefinition(
    noo::messages::TextRepresentation const& m,
    InternalClientState&) {
    text   = m.txt;
    font   = m.font;
    height = m.height;
    width  = m.width;
}

EntityWebpageDefinition::EntityWebpageDefinition(
    noo::messages::WebRepresentation const& m,
    InternalClientState&) {
    url = m.source;

    height = m.height;
    width  = m.width;
}

InstanceSource::InstanceSource(noo::messages::InstanceSource const& m,
                               InternalClientState&                 state) {

    convert(m.view, view, state);
    stride = m.stride;
    if (m.bb) instance_bb = m.bb.value();
}

EntityRenderableDefinition::EntityRenderableDefinition(
    noo::messages::RenderRepresentation const& m,
    InternalClientState&                       state) {

    convert(m.mesh, mesh, state);

    if (m.instances) instances = InstanceSource(*m.instances, state);
}

EntityInit::EntityInit(noo::messages::MsgEntityCreate const& m,
                       InternalClientState&                  state) {
    convert(m.name, name, state);

    if (m.parent) convert(*m.parent, parent, state);
    if (m.transform) transform = *m.transform;

    if (m.null_rep) {
        definition = std::monostate();
    } else if (m.text_rep) {
        definition = EntityTextDefinition(*m.text_rep, state);
    } else if (m.web_rep) {
        definition = EntityWebpageDefinition(*m.web_rep, state);
    } else if (m.render_rep) {
        definition = EntityRenderableDefinition(*m.render_rep, state);
    }

    convert(m.lights, lights, state);
    convert(m.tables, tables, state);
    convert(m.plots, plots, state);

    if (m.tags) tags = *m.tags;

    convert(m.methods_list, methods_list, state);
    convert(m.signals_list, signals_list, state);
    if (m.influence) influence = m.influence.value();
}


EntityUpdateData::EntityUpdateData(noo::messages::MsgEntityUpdate const& m,
                                   InternalClientState& state) {

    if (m.parent) convert(*m.parent, parent.emplace(), state);
    if (m.transform) transform = *m.transform;

    if (m.null_rep) {
        definition = std::monostate();
    } else if (m.text_rep) {
        definition = EntityTextDefinition(*m.text_rep, state);
    } else if (m.web_rep) {
        definition = EntityWebpageDefinition(*m.web_rep, state);
    } else if (m.render_rep) {
        definition = EntityRenderableDefinition(*m.render_rep, state);
    }

    convert(m.lights, lights, state);
    convert(m.tables, tables, state);
    convert(m.plots, plots, state);

    if (m.tags) tags = *m.tags;

    convert(m.methods_list, methods_list, state);
    convert(m.signals_list, signals_list, state);
    if (m.influence) influence = m.influence.value();
}

EntityDelegate::EntityDelegate(noo::EntityID i, EntityInit const& data)
    : m_id(i),
      m_data(data),
      m_attached_methods(this),
      m_attached_signals(this) {
    m_attached_methods = m_data.methods_list;
    m_attached_signals = m_data.signals_list;
}
EntityDelegate::~EntityDelegate() = default;

void EntityDelegate::post_create() {
    on_complete();
}

void EntityDelegate::update(EntityUpdateData const& data) {

    if (data.parent) m_data.parent = *data.parent;
    if (data.transform) m_data.transform = *data.transform;
    if (data.definition) m_data.definition = *data.definition;
    if (data.lights) m_data.lights = *data.lights;
    if (data.tables) m_data.tables = *data.tables;
    if (data.plots) m_data.plots = *data.plots;
    if (data.tags) m_data.tags = *data.tags;
    if (data.methods_list) {
        m_data.methods_list = *data.methods_list;
        m_attached_methods  = *data.methods_list;
    }
    if (data.signals_list) {
        m_data.signals_list = *data.signals_list;
        m_attached_signals  = *data.signals_list;
    }
    if (data.influence) m_data.influence = *data.influence;

    this->on_update(data);
}

void EntityDelegate::on_update(EntityUpdateData const&) { }
void EntityDelegate::on_complete() { }


AttachedMethodList const& EntityDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& EntityDelegate::attached_signals() const {
    return m_attached_signals;
}
noo::EntityID EntityDelegate::id() const {
    return m_id;
}

// =============================================================================

PlotInit::PlotInit(noo::messages::MsgPlotCreate const& m,
                   InternalClientState&                state) {
    convert(m.name, name, state);

    if (m.table) convert(*m.table, table, state);

    if (m.simple_plot) {
        type = *m.simple_plot;
    } else if (m.url_plot) {
        type = *m.url_plot;
    }

    convert(m.methods_list, methods_list, state);
    convert(m.signals_list, signals_list, state);
}

PlotUpdate::PlotUpdate(noo::messages::MsgPlotUpdate const& m,
                       InternalClientState&                state) {

    if (m.table) convert(*m.table, table.emplace(), state);

    if (m.simple_plot) {
        type = *m.simple_plot;
    } else if (m.url_plot) {
        type = *m.url_plot;
    }

    convert(m.methods_list, methods_list, state);
    convert(m.signals_list, signals_list, state);
}

PlotDelegate::PlotDelegate(noo::PlotID i, PlotInit const& data)
    : m_id(i),
      m_init(data),
      m_attached_methods(this),
      m_attached_signals(this) {

    m_attached_methods = data.methods_list;
    m_attached_signals = data.signals_list;
}
PlotDelegate::~PlotDelegate() = default;

void PlotDelegate::post_create() {
    on_complete();
}

void PlotDelegate::update(PlotUpdate const& data) {
    if (data.type) { m_type = *data.type; }
    if (data.table) { m_table = *data.table; }

    if (data.methods_list) {
        m_init.methods_list = *data.methods_list;
        m_attached_methods  = *data.methods_list;
    }
    if (data.signals_list) {
        m_init.signals_list = *data.signals_list;
        m_attached_signals  = *data.signals_list;
    }

    this->on_update(data);
    emit updated();
}
void                      PlotDelegate::on_update(PlotUpdate const&) { }
void                      PlotDelegate::on_complete() { }
AttachedMethodList const& PlotDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& PlotDelegate::attached_signals() const {
    return m_attached_signals;
}
noo::PlotID PlotDelegate::id() const {
    return m_id;
}

// =============================================================================

DocumentData::DocumentData(noo::messages::MsgDocumentUpdate const& m,
                           InternalClientState&                    state) {
    convert(m.methods_list, methods_list, state);
    convert(m.signals_list, signals_list, state);
}

DocumentDelegate::DocumentDelegate()
    : m_attached_methods(std::monostate()),
      m_attached_signals(std::monostate()) {
    // m_attached_methods = data.method_list;
    // m_attached_signals = data.signal_list;
}
DocumentDelegate::~DocumentDelegate() = default;
void DocumentDelegate::update(DocumentData const& data) {
    if (data.methods_list) m_attached_methods = *data.methods_list;
    if (data.signals_list) m_attached_signals = *data.signals_list;
    this->on_update(data);
    emit updated();
}
void DocumentDelegate::clear() {
    m_attached_methods = {};
    m_attached_signals = {};
    this->on_clear();
    emit updated();
}

void DocumentDelegate::on_update(DocumentData const&) { }
void DocumentDelegate::on_clear() { }

AttachedMethodList const& DocumentDelegate::attached_methods() const {
    return m_attached_methods;
}
AttachedSignalList const& DocumentDelegate::attached_signals() const {
    return m_attached_signals;
}

// =============================================================================

class ClientCore : public QObject {
    ClientConnection* m_owning_connection;

    ClientDelegates m_makers;

    std::optional<InternalClientState> m_state;

    bool       m_connecting = true;
    QWebSocket m_socket;

public:
    ClientCore(ClientConnection* conn,
               QUrl const&       url,
               ClientDelegates&& makers)
        : m_owning_connection(conn), m_makers(std::move(makers)) {
        connect(&m_socket,
                &QWebSocket::connected,
                this,
                &ClientCore::socket_connected);
        connect(&m_socket,
                &QWebSocket::disconnected,
                this,
                &ClientCore::socket_closed);
        connect(&m_socket,
                QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
                [=](QAbstractSocket::SocketError) {
                    emit m_owning_connection->socket_error(
                        m_socket.errorString());
                });


        m_socket.open(url);
    }

    ~ClientCore() { qDebug() << Q_FUNC_INFO; }

    bool is_connecting() const { return m_connecting; }

    TextureDelegate* get(noo::TextureID id) {
        if (!m_state) return {};

        return m_state->texture_list().comp_at(id);
    }
    BufferDelegate* get(noo::BufferID id) {
        if (!m_state) return {};

        return m_state->buffer_list().comp_at(id);
    }
    TableDelegate* get(noo::TableID id) {
        if (!m_state) return {};

        return m_state->table_list().comp_at(id);
    }
    LightDelegate* get(noo::LightID id) {
        if (!m_state) return {};

        return m_state->light_list().comp_at(id);
    }
    MaterialDelegate* get(noo::MaterialID id) {
        if (!m_state) return {};

        return m_state->material_list().comp_at(id);
    }
    MeshDelegate* get(noo::GeometryID id) {
        if (!m_state) return {};

        return m_state->mesh_list().comp_at(id);
    }
    EntityDelegate* get(noo::EntityID id) {
        if (!m_state) return {};

        return m_state->object_list().comp_at(id);
    }
    SignalDelegate* get(noo::SignalID id) {
        if (!m_state) return {};

        return m_state->signal_list().comp_at(id);
    }
    MethodDelegate* get(noo::MethodID id) {
        if (!m_state) return {};

        return m_state->method_list().comp_at(id);
    }

private slots:
    void socket_connected() {
        m_connecting = false;
        m_state.emplace(m_socket, m_makers);
        emit m_owning_connection->connected();
    }
    void socket_closed() {
        emit m_owning_connection->disconnected();
        m_state.reset();
    }
};


std::unique_ptr<ClientCore>
create_client(ClientConnection* conn, QUrl server, ClientDelegates&& d) {

#define CREATE_DEFAULT(E, ID, DATA, DEL)                                       \
    if (!d.E) {                                                                \
        d.E = [](noo::ID id, DATA const& d) {                                  \
            return std::make_unique<DEL>(id, d);                               \
        };                                                                     \
    }
    CREATE_DEFAULT(tex_maker, TextureID, TextureInit, TextureDelegate);
    CREATE_DEFAULT(buffer_maker, BufferID, BufferInit, BufferDelegate);
    CREATE_DEFAULT(
        buffer_view_maker, BufferViewID, BufferViewInit, BufferViewDelegate);
    CREATE_DEFAULT(sampler_maker, SamplerID, SamplerInit, SamplerDelegate);
    CREATE_DEFAULT(image_maker, ImageID, ImageInit, ImageDelegate);
    CREATE_DEFAULT(table_maker, TableID, TableInit, TableDelegate);
    CREATE_DEFAULT(light_maker, LightID, LightInit, LightDelegate);
    CREATE_DEFAULT(mat_maker, MaterialID, MaterialInit, MaterialDelegate);
    CREATE_DEFAULT(mesh_maker, GeometryID, MeshInit, MeshDelegate);
    CREATE_DEFAULT(object_maker, EntityID, EntityInit, EntityDelegate);
    CREATE_DEFAULT(sig_maker, SignalID, SignalInit, SignalDelegate);
    CREATE_DEFAULT(method_maker, MethodID, MethodInit, MethodDelegate);


    if (!d.doc_maker) {
        d.doc_maker = []() { return std::make_unique<DocumentDelegate>(); };
    }


    return std::make_unique<ClientCore>(conn, server, std::move(d));
}

ClientConnection::ClientConnection(QObject* parent) : QObject(parent) { }
ClientConnection::~ClientConnection() = default;

void ClientConnection::open(QUrl server, ClientDelegates&& delegates) {
    if (m_data) {
        if (m_data->is_connecting()) { return; }

        // disconnect current connection
        m_data.reset();
    }

    m_data = create_client(this, server, std::move(delegates));
}

TextureDelegate* ClientConnection::get(noo::TextureID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
BufferDelegate* ClientConnection::get(noo::BufferID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
TableDelegate* ClientConnection::get(noo::TableID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
LightDelegate* ClientConnection::get(noo::LightID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MaterialDelegate* ClientConnection::get(noo::MaterialID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MeshDelegate* ClientConnection::get(noo::GeometryID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
EntityDelegate* ClientConnection::get(noo::EntityID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
SignalDelegate* ClientConnection::get(noo::SignalID id) {
    if (!m_data) return {};

    return m_data->get(id);
}
MethodDelegate* ClientConnection::get(noo::MethodID id) {
    if (!m_data) return {};

    return m_data->get(id);
}

} // namespace nooc
