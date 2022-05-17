#include "interface_tools.h"

#include "include/noo_interface_types.h"
#include "src/common/variant_tools.h"


#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>

#include <QColor>
#include <QDebug>

namespace noo {

#if 0

template <class T, class Function, class U>
auto set_from(Function f, U u, flatbuffers::FlatBufferBuilder& b) {
    auto handle = f(b, u.id_slot, u.id_gen);

    auto enum_value = ::noodles::AnyIDTypeTraits<T>::enum_value;

    Q_ASSERT(enum_value != ::noodles::AnyIDType::NONE);

    return ::noodles::CreateAnyID(b, enum_value, handle.Union());
}

flatbuffers::Offset<::noodles::AnyID>
write_to(AnyID const& anyid, flatbuffers::FlatBufferBuilder& b) {

    return VMATCH(
        anyid,
        VCASE(std::monostate) {
            return flatbuffers::Offset<::noodles::AnyID>();
        },
        VCASE(SamplerID id) {
            return set_from<noodles::SamplerID>(
                noodles::CreateSamplerID, id, b);
        },
        VCASE(TextureID id) {
            return set_from<noodles::TextureID>(
                noodles::CreateTextureID, id, b);
        },
        VCASE(ImageID id) {
            return set_from<noodles::ImageID>(noodles::CreateImageID, id, b);
        },
        VCASE(BufferID id) {
            return set_from<noodles::BufferID>(noodles::CreateBufferID, id, b);
        },
        VCASE(BufferViewID id) {
            return set_from<noodles::BufferViewID>(
                noodles::CreateBufferID, id, b);
        },
        VCASE(TableID id) {
            return set_from<noodles::TableID>(noodles::CreateTableID, id, b);
        },
        VCASE(LightID id) {
            return set_from<noodles::LightID>(noodles::CreateLightID, id, b);
        },
        VCASE(MaterialID id) {
            return set_from<noodles::MaterialID>(
                noodles::CreateMaterialID, id, b);
        },
        VCASE(GeometryID id) {
            return set_from<noodles::GeometryID>(
                noodles::CreateGeometryID, id, b);
        },
        VCASE(EntityID id) {
            return set_from<noodles::EntityID>(noodles::CreateEntityID, id, b);
        },
        VCASE(SignalID id) {
            return set_from<noodles::SignalID>(noodles::CreateSignalID, id, b);
        },
        VCASE(MethodID id) {
            return set_from<noodles::MethodID>(noodles::CreateMethodID, id, b);
        },
        VCASE(PlotID id) {
            return set_from<noodles::PlotID>(noodles::CreatePlotID, id, b);
        },

    );
}

flatbuffers::Offset<::noodles::AnyList>
write_to(QCborValueList const& list, flatbuffers::FlatBufferBuilder& b) {
    auto v = write_array_to(list, b);
    return ::noodles::CreateAnyList(b, v);
}

template <class T>
auto write_any_helper(flatbuffers::Offset<T>          v,
                      flatbuffers::FlatBufferBuilder& b) {

    using L = std::remove_pointer_t<T>;

    //    qDebug() << "WRITE HELPER " << typeid(T).name() << typeid(L).name()
    //             << (int)::noodles::AnyTypeTraits<L>::enum_value;


    auto enum_value =
        ::noodles::AnyTypeTraits<std::remove_const_t<L>>::enum_value;

    // we handle none's elsewhere
    Q_ASSERT(enum_value != ::noodles::AnyType::NONE);

    return ::noodles::CreateAny(b, enum_value, v.Union());
}

flatbuffers::Offset<::noodles::Any>
write_to(AnyVar const& any, flatbuffers::FlatBufferBuilder& b) {

    auto x = VMATCH(
        any,
        VCASE(std::monostate) { return ::noodles::CreateAny(b); },
        VCASE(int64_t integer) {
            auto l = noodles::CreateInteger(b, integer);

            return write_any_helper(l, b);
        },
        VCASE(double real) {
            auto l = noodles::CreateReal(b, real);

            return write_any_helper(l, b);
        },
        VCASE(std::string const& str) {
            auto l = ::noodles::CreateTextDirect(b, str.c_str());

            return write_any_helper(l, b);
        },
        VCASE(AnyID any_id) {
            auto l = write_to(any_id, b);

            return write_any_helper(l, b);
        },
        VCASE(std::vector<std::byte> const& bytes) {
            auto v =
                b.CreateVectorScalarCast<int8_t>(bytes.data(), bytes.size());
            auto l = ::noodles::CreateData(b, v);

            return write_any_helper(l, b);
        },
        VCASE(AnyVarMap const& map) {
            std::vector<flatbuffers::Offset<noodles::MapEntry>> entries;

            for (auto const& [k, v] : map) {
                auto handle = write_to(v, b);
                entries.push_back(CreateMapEntryDirect(b, k.c_str(), handle));
            }

            auto entry_list = b.CreateVectorOfSortedTables(&entries);

            auto l = ::noodles::CreateAnyMap(b, entry_list);

            return write_any_helper(l, b);
        },
        VCASE(AnyVarList const& list) {
            auto l = write_to(list, b);
            return write_any_helper(l, b);
        },
        VCASE(std::vector<double> const& reals) {
            auto v = b.CreateVector(reals.data(), reals.size());
            auto l = ::noodles::CreateRealList(b, v);

            return write_any_helper(l, b);
        },
        VCASE(std::vector<int64_t> const& ints) {
            auto v = b.CreateVector(ints.data(), ints.size());
            auto l = ::noodles::CreateIntegerList(b, v);

            return write_any_helper(l, b);
        },
        VCASE(glm::vec2 const& vector) {
            auto v = noodles::CreateAVec2(b, vector.x, vector.y);
            return write_any_helper(v, b);
        },
        VCASE(glm::vec3 const& vector) {
            auto v = noodles::CreateAVec3(b, vector.x, vector.y, vector.z);
            return write_any_helper(v, b);
        },
        VCASE(glm::vec4 const& vector) {
            auto v =
                noodles::CreateAVec4(b, vector.x, vector.y, vector.z, vector.w);
            return write_any_helper(v, b);
        });

    return x;
}

::flatbuffers::Offset<::noodles::Vec2 const*>
write_to(glm::vec2 const& v, flatbuffers::FlatBufferBuilder& b) {
    return b.CreateStruct(::noodles::Vec2(v.x, v.y));
}

::flatbuffers::Offset<::noodles::Vec3 const*>
write_to(glm::vec3 const& v, flatbuffers::FlatBufferBuilder& b) {
    return b.CreateStruct(::noodles::Vec3(v.x, v.y, v.z));
}

::flatbuffers::Offset<::noodles::Vec4 const*>
write_to(glm::vec4 const& v, flatbuffers::FlatBufferBuilder& b) {
    return b.CreateStruct(::noodles::Vec4(v.x, v.y, v.z, v.w));
}

::flatbuffers::Offset<::noodles::Mat4 const*>
write_to(glm::mat4 const& m, flatbuffers::FlatBufferBuilder& b) {

    ::noodles::Mat4 nm;

    static_assert(sizeof(m) == sizeof(nm));

    ::std::memcpy(&nm, glm::value_ptr(m), sizeof(m));

    return b.CreateStruct(nm);
}

::noodles::Vec2 convert(glm::vec2 const& v) {
    return { v.x, v.y };
}
::noodles::Vec3 convert(glm::vec3 const& v) {
    return { v.x, v.y, v.z };
}
::noodles::Vec4 convert(glm::vec4 const& v) {
    return { v.x, v.y, v.z, v.w };
}
::noodles::Mat4 convert(glm::mat4 const& v) {
    static_assert(sizeof(glm::mat4) == sizeof(::noodles::Mat4));
    ::noodles::Mat4 ret;

    memcpy(&ret, &v, sizeof(glm::mat4));

    return ret;
}
::noodles::RGB convert(QColor const& c) {
    return { (uint8_t)c.red(), (uint8_t)c.green(), (uint8_t)c.blue() };
}

glm::vec2 convert(::noodles::Vec2 const& v) {
    return { v.x(), v.y() };
}

glm::vec3 convert(::noodles::Vec3 const& v) {
    return { v.x(), v.y(), v.z() };
}

glm::vec4 convert(::noodles::Vec4 const& v) {
    return { v.x(), v.y(), v.z(), v.w() };
}
glm::mat4 convert(::noodles::Mat4 const& m) {
    static_assert(sizeof(glm::mat4) == sizeof(::noodles::Mat4));
    glm::mat4 ret;

    memcpy(&ret, &m, sizeof(glm::mat4));

    return ret;
}
QColor convert(::noodles::RGB const& m) {
    return QColor { m.r(), m.g(), m.b() };
}

::noo::BoundingBox convert(::noodles::BoundingBox const& bb) {
    static_assert(sizeof(::noodles::BoundingBox) == sizeof(::noo::BoundingBox));
    ::noo::BoundingBox ret;
    memcpy(&ret, &bb, sizeof(ret));
    return ret;
}
::noodles::BoundingBox convert(::noo::BoundingBox const& bb) {
    static_assert(sizeof(::noodles::BoundingBox) == sizeof(::noo::BoundingBox));
    ::noodles::BoundingBox ret;
    memcpy(&ret, &bb, sizeof(ret));
    return ret;
}

// =============================================================================

void read_to(::noodles::Any const* n_any, AnyVar& any) {
    if (!n_any) {
        any = AnyVar();
        return;
    }

    switch (n_any->any_type()) {
    case noodles::AnyType::NONE: any = AnyVar(); break;
    case noodles::AnyType::Text:
        any = AnyVar(n_any->any_as_Text()->text()->c_str());
        break;
    case noodles::AnyType::Integer:
        any = AnyVar(n_any->any_as_Integer()->integer());
        break;
    case noodles::AnyType::IntegerList: {
        auto& nlist = *n_any->any_as_IntegerList()->integers();

        std::vector<int64_t> list(nlist.size());

        std::memcpy(list.data(), nlist.data(), sizeof(int64_t) * nlist.size());

        any = std::move(list);
    } break;
    case noodles::AnyType::Real:
        any = AnyVar(n_any->any_as_Real()->real());
        break;
    case noodles::AnyType::RealList: {
        auto& nlist = *n_any->any_as_RealList()->reals();

        std::vector<double> list(nlist.size());

        std::memcpy(list.data(), nlist.data(), sizeof(double) * nlist.size());

        any = std::move(list);

    } break;
    case noodles::AnyType::Data: {
        auto& nlist = *n_any->any_as_Data()->data();

        std::vector<std::byte> bytes(nlist.size());

        std::memcpy(bytes.data(), nlist.data(), nlist.size());

        any = AnyVar(std::move(bytes));
    } break;
    case noodles::AnyType::AnyList: {
        auto& nlist = *n_any->any_as_AnyList()->list();

        AnyVarList list;
        list.reserve(nlist.size());

        for (auto n : nlist) {
            read_to(n, list.emplace_back());
        }

        any = std::move(list);
    } break;
    case noodles::AnyType::AnyMap: {
        auto& nmap = *n_any->any_as_AnyMap()->entries();

        AnyVarMap map;

        for (auto n : nmap) {
            AnyVar lv;
            read_to(n->value(), lv);

            map.try_emplace(n->name()->str(), std::move(lv));
        }

        any = std::move(map);

    } break;
    case noodles::AnyType::AnyID: {
        AnyID a_id;
        read_to(n_any->any_as_AnyID(), a_id);
        any = a_id;
    } break;
    case noodles::AnyType::AVec2: {
        auto sv = n_any->any_as_AVec2();
        any     = glm::vec2(sv->x(), sv->y());
    } break;
    case noodles::AnyType::AVec3: {
        auto sv = n_any->any_as_AVec3();
        any     = glm::vec3(sv->x(), sv->y(), sv->z());
    } break;
    case noodles::AnyType::AVec4: {
        auto sv = n_any->any_as_AVec4();
        any     = glm::vec4(sv->x(), sv->y(), sv->z(), sv->w());
    }
    }
}

template <class NT>
auto read_to_helper(NT* nt) {
    using CleanedNT = std::remove_cvref_t<NT>;
    using Ret       = typename IDConvType<CleanedNT>::type;


    if (!nt) return Ret();
    return Ret(nt->id_slot(), nt->id_gen());
}


void read_to(::noodles::AnyID const* n_any_id, AnyID& any_id) {
    if (!n_any_id) {
        any_id = AnyID();
        return;
    }

    switch (n_any_id->id_type()) {
    case noodles::AnyIDType::NONE: any_id = AnyID(); break;
    case noodles::AnyIDType::EntityID:
        any_id = read_to_helper(n_any_id->id_as_EntityID());
        break;
    case noodles::AnyIDType::TableID:
        any_id = read_to_helper(n_any_id->id_as_TableID());
        break;
    case noodles::AnyIDType::SignalID:
        any_id = read_to_helper(n_any_id->id_as_SignalID());
        break;
    case noodles::AnyIDType::MethodID:
        any_id = read_to_helper(n_any_id->id_as_MethodID());
        break;
    case noodles::AnyIDType::MaterialID:
        any_id = read_to_helper(n_any_id->id_as_MaterialID());
        break;
    case noodles::AnyIDType::GeometryID:
        any_id = read_to_helper(n_any_id->id_as_GeometryID());
        break;
    case noodles::AnyIDType::LightID:
        any_id = read_to_helper(n_any_id->id_as_LightID());
        break;
    case noodles::AnyIDType::ImageID:
        any_id = read_to_helper(n_any_id->id_as_ImageID());
        break;
    case noodles::AnyIDType::TextureID:
        any_id = read_to_helper(n_any_id->id_as_TextureID());
        break;
    case noodles::AnyIDType::SamplerID:
        any_id = read_to_helper(n_any_id->id_as_SamplerID());
        break;
    case noodles::AnyIDType::BufferID:
        any_id = read_to_helper(n_any_id->id_as_BufferID());
        break;
    case noodles::AnyIDType::BufferViewID:
        any_id = read_to_helper(n_any_id->id_as_BufferViewID());
        break;
    case noodles::AnyIDType::PlotID:
        any_id = read_to_helper(n_any_id->id_as_PlotID());
        break;
    }
}

void read_to_array(const noodles::AnyList* n_array, std::vector<AnyVar>& list) {

    if (!n_array) {
        list = {};
        return;
    }

    list.clear();

    for (auto n : *(n_array->list())) {
        read_to(n, list.emplace_back());
    }
}

flatbuffers::Offset<::noodles::TextureID>
convert_id(TextureID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateTextureID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::BufferID>
convert_id(BufferID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateBufferID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::TableID>
convert_id(TableID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateTableID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::LightID>
convert_id(LightID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateLightID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::MaterialID>
convert_id(MaterialID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateMaterialID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::GeometryID>
convert_id(GeometryID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateGeometryID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::EntityID>
convert_id(EntityID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateEntityID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::SignalID>
convert_id(SignalID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateSignalID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::MethodID>
convert_id(MethodID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateMethodID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::PlotID>
convert_id(PlotID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreatePlotID(b, id.id_slot, id.id_gen);
}


TextureID convert_id(::noodles::TextureID const& id) {
    return { id.id_slot(), id.id_gen() };
}
BufferID convert_id(::noodles::BufferID const& id) {
    return { id.id_slot(), id.id_gen() };
}
TableID convert_id(::noodles::TableID const& id) {
    return { id.id_slot(), id.id_gen() };
}
LightID convert_id(::noodles::LightID const& id) {
    return { id.id_slot(), id.id_gen() };
}
MaterialID convert_id(::noodles::MaterialID const& id) {
    return { id.id_slot(), id.id_gen() };
}
GeometryID convert_id(::noodles::GeometryID const& id) {
    return { id.id_slot(), id.id_gen() };
}
EntityID convert_id(::noodles::EntityID const& id) {
    return { id.id_slot(), id.id_gen() };
}
SignalID convert_id(::noodles::SignalID const& id) {
    return { id.id_slot(), id.id_gen() };
}
MethodID convert_id(::noodles::MethodID const& id) {
    return { id.id_slot(), id.id_gen() };
}
PlotID convert_id(::noodles::PlotID const& id) {
    return { id.id_slot(), id.id_gen() };
}

TextureID convert_id(::noodles::TextureID const* p) {
    if (p) return convert_id(*p);
    return {};
}
BufferID convert_id(::noodles::BufferID const* p) {
    if (p) return convert_id(*p);
    return {};
}
TableID convert_id(::noodles::TableID const* p) {
    if (p) return convert_id(*p);
    return {};
}
LightID convert_id(::noodles::LightID const* p) {
    if (p) return convert_id(*p);
    return {};
}
MaterialID convert_id(::noodles::MaterialID const* p) {
    if (p) return convert_id(*p);
    return {};
}
GeometryID convert_id(::noodles::GeometryID const* p) {
    if (p) return convert_id(*p);
    return {};
}
EntityID convert_id(::noodles::EntityID const* p) {
    if (p) return convert_id(*p);
    return {};
}
SignalID convert_id(::noodles::SignalID const* p) {
    if (p) return convert_id(*p);
    return {};
}
MethodID convert_id(::noodles::MethodID const* p) {
    if (p) return convert_id(*p);
    return {};
}
PlotID convert_id(::noodles::PlotID const* p) {
    if (p) return convert_id(*p);
    return {};
}

#endif

} // namespace noo
