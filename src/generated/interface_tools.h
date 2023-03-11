#ifndef NOO_INTERFACE_TOOLS_H
#define NOO_INTERFACE_TOOLS_H

#include "include/noo_id.h"
#include "include/noo_include_glm.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#if 0

// Forward declare generated noodles types
namespace noodles {
struct Any;
struct AnyList;
struct AnyID;

struct Vec2;
struct Vec3;
struct Vec4;
struct Mat4;
struct RGB;
struct BoundingBox;

struct TextureID;
struct BufferID;
struct BufferViewID;
struct TableID;
struct LightID;
struct MaterialID;
struct GeometryID;
struct EntityID;
struct SignalID;
struct ImageID;
struct SamplerID;
struct MethodID;
struct PlotID;

} // namespace noodles

namespace noo {


/// Template to help convert noodles to noo ID types
template <class T>
struct IDConvType;

template <>
struct IDConvType<noodles::EntityID> {
    using type = EntityID;
};

template <>
struct IDConvType<noodles::TableID> {
    using type = TableID;
};

template <>
struct IDConvType<noodles::SignalID> {
    using type = SignalID;
};

template <>
struct IDConvType<noodles::MethodID> {
    using type = MethodID;
};

template <>
struct IDConvType<noodles::MaterialID> {
    using type = MaterialID;
};

template <>
struct IDConvType<noodles::GeometryID> {
    using type = GeometryID;
};

template <>
struct IDConvType<noodles::LightID> {
    using type = LightID;
};

template <>
struct IDConvType<noodles::ImageID> {
    using type = TextureID;
};

template <>
struct IDConvType<noodles::TextureID> {
    using type = TextureID;
};

template <>
struct IDConvType<noodles::SamplerID> {
    using type = SamplerID;
};

template <>
struct IDConvType<noodles::BufferID> {
    using type = BufferID;
};

template <>
struct IDConvType<noodles::BufferViewID> {
    using type = BufferViewID;
};

template <>
struct IDConvType<noodles::PlotID> {
    using type = PlotID;
};

class AnyVar;
using AnyVarList = std::vector<AnyVar>;

flatbuffers::Offset<::noodles::Any> write_to(AnyVar const&,
                                             flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::AnyList>
write_to(QCborValueList const&, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::AnyID> write_to(AnyID const&,
                                               flatbuffers::FlatBufferBuilder&);

::flatbuffers::Offset<::noodles::Vec2 const*>
write_to(glm::vec2 const&, flatbuffers::FlatBufferBuilder&);

::flatbuffers::Offset<::noodles::Vec3 const*>
write_to(glm::vec3 const&, flatbuffers::FlatBufferBuilder&);

::flatbuffers::Offset<::noodles::Vec4 const*>
write_to(glm::vec4 const&, flatbuffers::FlatBufferBuilder&);

::flatbuffers::Offset<::noodles::Mat4 const*>
write_to(glm::mat4 const&, flatbuffers::FlatBufferBuilder&);

::flatbuffers::Offset<::noodles::RGB const*>
write_to(glm::u8vec3 const&, flatbuffers::FlatBufferBuilder&);


::noodles::Vec2 convert(glm::vec2 const&);
::noodles::Vec3 convert(glm::vec3 const&);
::noodles::Vec4 convert(glm::vec4 const&);
::noodles::Mat4 convert(glm::mat4 const&);
::noodles::RGB  convert(QColor const&);

glm::vec2 convert(::noodles::Vec2 const&);
glm::vec3 convert(::noodles::Vec3 const&);
glm::vec4 convert(::noodles::Vec4 const&);
glm::mat4 convert(::noodles::Mat4 const&);
QColor    convert(::noodles::RGB const&);

struct BoundingBox;
::noo::BoundingBox     convert(::noodles::BoundingBox const&);
::noodles::BoundingBox convert(::noo::BoundingBox const&);

// ====


void read_to(::noodles::Any const*, AnyVar&);
void read_to(::noodles::AnyID const*, AnyID&);

void read_to_array(noodles::AnyList const*, std::vector<AnyVar>&);

flatbuffers::Offset<::noodles::TextureID>
convert_id(TextureID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::BufferID>
convert_id(BufferID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::BufferViewID>
convert_id(BufferViewID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::TableID>
convert_id(TableID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::LightID>
convert_id(LightID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::MaterialID>
convert_id(MaterialID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::GeometryID>
convert_id(GeometryID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::EntityID>
convert_id(EntityID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::SignalID>
convert_id(SignalID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::MethodID>
convert_id(MethodID, flatbuffers::FlatBufferBuilder&);

flatbuffers::Offset<::noodles::PlotID>
convert_id(PlotID, flatbuffers::FlatBufferBuilder&);

template <class T>
auto convert_id(std::shared_ptr<T> const&       ptr,
                flatbuffers::FlatBufferBuilder& b) {
    using IDType = decltype(ptr->id());
    if (ptr) { return convert_id(ptr->id(), b); }
    return convert_id(IDType(), b);
}

ImageID      convert_id(::noodles::ImageID const&);
TextureID    convert_id(::noodles::TextureID const&);
SamplerID    convert_id(::noodles::SamplerID const&);
BufferID     convert_id(::noodles::BufferID const&);
BufferViewID convert_id(::noodles::BufferViewID const&);
TableID      convert_id(::noodles::TableID const&);
LightID      convert_id(::noodles::LightID const&);
MaterialID   convert_id(::noodles::MaterialID const&);
GeometryID   convert_id(::noodles::GeometryID const&);
EntityID     convert_id(::noodles::EntityID const&);
SignalID     convert_id(::noodles::SignalID const&);
MethodID     convert_id(::noodles::MethodID const&);
PlotID       convert_id(::noodles::PlotID const&);

ImageID      convert_id(::noodles::ImageID const*);
TextureID    convert_id(::noodles::TextureID const*);
SamplerID    convert_id(::noodles::SamplerID const*);
BufferID     convert_id(::noodles::BufferID const*);
BufferViewID convert_id(::noodles::BufferViewID const*);
TableID      convert_id(::noodles::TableID const*);
LightID      convert_id(::noodles::LightID const*);
MaterialID   convert_id(::noodles::MaterialID const*);
GeometryID   convert_id(::noodles::GeometryID const*);
EntityID     convert_id(::noodles::EntityID const*);
SignalID     convert_id(::noodles::SignalID const*);
MethodID     convert_id(::noodles::MethodID const*);
PlotID       convert_id(::noodles::PlotID const*);

/// Write an arbitrary iterable container to a flatbuffer.
template <class Container>
auto write_array_to(Container const& v, flatbuffers::FlatBufferBuilder& b) {

    using OffsetType = decltype(write_to(*v.begin(), b));

    std::vector<OffsetType> tmp;
    tmp.reserve(v.size());

    for (auto const& obj : v) {
        tmp.push_back(write_to(obj, b));
    }

    return b.CreateVector(tmp);
}

/// Turn a list of pointers to noodles components into a vector of their IDs
template <class Ptr>
auto make_id_list(std::vector<Ptr> const&         ps,
                  flatbuffers::FlatBufferBuilder& b) {
    using TID = decltype(convert_id(*ps.begin(), b));
    std::vector<TID> ret;

    for (auto const& ptr : ps) {
        ret.emplace_back(convert_id(ptr, b));
    }

    return ret;
}

/// Turn a list of pointers to noodles components into a vector of their IDS
template <class Iter>
auto make_id_list(Iter b, Iter e, flatbuffers::FlatBufferBuilder& blder) {
    using TID = decltype(convert_id(*b, blder));
    std::vector<TID> ret;

    for (auto i = b; i != e; ++i) {
        ret.emplace_back(convert_id(*i, blder));
    }

    return ret;
}

} // namespace noo

#endif

#endif // INTERFACE_TOOLS_H
