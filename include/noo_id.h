#ifndef NOO_ID_H
#define NOO_ID_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <variant>

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
};

struct ObjectIDTag;
struct MeshIDTag;
struct MaterialIDTag;
struct TableIDTag;
struct LightIDTag;
struct TextureIDTag;
struct BufferIDTag;
struct MethodIDTag;
struct SignalIDTag;
struct PlotIDTag;

// Typedefs for used ID types
using TextureID  = ID<TextureIDTag>;
using BufferID   = ID<BufferIDTag>;
using TableID    = ID<TableIDTag>;
using LightID    = ID<LightIDTag>;
using MaterialID = ID<MaterialIDTag>;
using MeshID     = ID<MeshIDTag>;
using ObjectID   = ID<ObjectIDTag>;
using SignalID   = ID<SignalIDTag>;
using MethodID   = ID<MethodIDTag>;
using PlotID     = ID<PlotIDTag>;

template <>
struct TagToString<ObjectIDTag> {
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
struct TagToString<BufferIDTag> {
    static constexpr const char* str = "Buffer";
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


///
/// \brief The AnyID struct models the NOODLES Any type
///
struct AnyID : public std::variant<std::monostate,
                                   TextureID,
                                   BufferID,
                                   LightID,
                                   TableID,
                                   MaterialID,
                                   MeshID,
                                   ObjectID,
                                   SignalID,
                                   MethodID,
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

#endif // ID_H
