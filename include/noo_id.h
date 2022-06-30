#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>

#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QVariant>

namespace noo {

///
/// \brief Class to help turn ID tags into strings at compile time
template <class Tag>
struct TagToString;

///
/// \brief The core ID type.
///
template <class Tag>
struct ID {
    static constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();

    uint32_t id_slot = INVALID;
    uint32_t id_gen  = INVALID;

    ID() = default;

    ID(QCborValue const& v) {
        auto a  = v.toArray();
        id_slot = a.at(0).toInteger(INVALID);
        id_gen  = a.at(1).toInteger(INVALID);
    }

    ID(uint32_t _slot, uint32_t _gen) : id_slot(_slot), id_gen(_gen) { }

    // QVariant to_variant() const { return slot; }

    bool operator==(ID const& other) const {
        return std::tie(id_slot, id_gen) ==
               std::tie(other.id_slot, other.id_gen);
    }

    bool operator!=(ID const& other) const { return !(*this == other); }

    bool valid() const { return id_slot != INVALID and id_gen != INVALID; }

    std::string to_string() const {
        if (!valid()) {
            return TagToString<Tag>::str + std::string(" INVALID");
        }
        return std::string(TagToString<Tag>::str) + " " +
               std::to_string(id_slot) + "/" + std::to_string(id_gen);
    }

    QString to_qstring() const {
        if (!valid()) {
            return QString("%1 INVALID").arg(TagToString<Tag>::str);
        }
        return QString("%1 %2/%3")
            .arg(TagToString<Tag>::str)
            .arg(id_slot)
            .arg(id_gen);
    }

    QCborValue to_cbor() const { return QCborArray({ id_slot, id_gen }); }
};

template <class Tag>
bool operator<(ID<Tag> a, ID<Tag> b) {
    union glue {
        ID<Tag>  id;
        uint64_t packed;
    };

    return glue { .id = a }.packed < glue { .id = b }.packed;
}

template <class T>
static T id_from_message(QCborMap const& m) {
    return T(m[QStringLiteral("id")]);
}

struct EntityIDTag;
struct MeshIDTag;
struct MaterialIDTag;
struct TableIDTag;
struct LightIDTag;
struct SamplerIDTag;
struct TextureIDTag;
struct ImageIDTag;
struct BufferIDTag;
struct BufferViewIDTag;
struct MethodIDTag;
struct SignalIDTag;
struct PlotIDTag;

// Typedefs for used ID types
using EntityID     = ID<EntityIDTag>;
using PlotID       = ID<PlotIDTag>;
using TableID      = ID<TableIDTag>;
using SignalID     = ID<SignalIDTag>;
using MethodID     = ID<MethodIDTag>;
using MaterialID   = ID<MaterialIDTag>;
using GeometryID   = ID<MeshIDTag>;
using LightID      = ID<LightIDTag>;
using ImageID      = ID<ImageIDTag>;
using SamplerID    = ID<SamplerIDTag>;
using TextureID    = ID<TextureIDTag>;
using BufferID     = ID<BufferIDTag>;
using BufferViewID = ID<BufferViewIDTag>;

template <>
struct TagToString<EntityIDTag> {
    static constexpr const char* str = "Object";
};
template <>
struct TagToString<MeshIDTag> {
    static constexpr const char* str = "Mesh";
};
template <>
struct TagToString<MaterialIDTag> {
    static constexpr const char* str = "Material";
};
template <>
struct TagToString<TableIDTag> {
    static constexpr const char* str = "Table";
};
template <>
struct TagToString<LightIDTag> {
    static constexpr const char* str = "Light";
};
template <>
struct TagToString<TextureIDTag> {
    static constexpr const char* str = "Texture";
};
template <>
struct TagToString<SamplerIDTag> {
    static constexpr const char* str = "Sampler";
};
template <>
struct TagToString<ImageIDTag> {
    static constexpr const char* str = "Image";
};
template <>
struct TagToString<BufferIDTag> {
    static constexpr const char* str = "Buffer";
};
template <>
struct TagToString<BufferViewIDTag> {
    static constexpr const char* str = "Bufferview";
};
template <>
struct TagToString<MethodIDTag> {
    static constexpr const char* str = "Method";
};
template <>
struct TagToString<SignalIDTag> {
    static constexpr const char* str = "Signal";
};
template <>
struct TagToString<PlotIDTag> {
    static constexpr const char* str = "Plot";
};


template <class Tag>
QDebug operator<<(QDebug debug, ID<Tag> id) {
    QDebugStateSaver saver(debug);
    debug.nospace() << id.to_qstring();

    return debug;
}

///
/// \brief The AnyID struct models the NOODLES Any type
///
struct InvokeID
    : public std::variant<std::monostate, EntityID, TableID, PlotID> {
    using variant::variant;
};

///
/// \brief The AnyID struct models the NOODLES Any type
///
struct AnyID : public std::variant<std::monostate,
                                   EntityID,
                                   TableID,
                                   SignalID,
                                   MethodID,
                                   MaterialID,
                                   GeometryID,
                                   LightID,
                                   ImageID,
                                   TextureID,
                                   SamplerID,
                                   BufferID,
                                   BufferViewID,
                                   PlotID> {
    using variant::variant;
};

} // namespace noo


// Enable hash support for IDs
namespace std {

template <class Tag>
struct hash<noo::ID<Tag>> {
    std::size_t operator()(noo::ID<Tag> const& s) const noexcept {
        std::size_t h1 = std::hash<uint32_t> {}(s.id_slot);
        std::size_t h2 = std::hash<uint32_t> {}(s.id_gen);
        return h1 ^ (h2 << 1);
    }
};

} // namespace std
