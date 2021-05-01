#include "interface_tools.h"

#include "include/noo_interface_types.h"
#include "src/common/variant_tools.h"
#include "src/generated/noodles_generated.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>

#include <QDebug>

namespace noo {

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
        VCASE(TextureID id) {
            return set_from<noodles::TextureID>(
                noodles::CreateTextureID, id, b);
        },
        VCASE(BufferID id) {
            return set_from<noodles::BufferID>(noodles::CreateBufferID, id, b);
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
        VCASE(MeshID id) {
            return set_from<noodles::GeometryID>(
                noodles::CreateGeometryID, id, b);
        },
        VCASE(ObjectID id) {
            return set_from<noodles::ObjectID>(noodles::CreateObjectID, id, b);
        },
        VCASE(SignalID id) {
            return set_from<noodles::SignalID>(noodles::CreateSignalID, id, b);
        },
        VCASE(MethodID id) {
            return set_from<noodles::MethodID>(noodles::CreateMethodID, id, b);
        },

    );
}

flatbuffers::Offset<::noodles::AnyList>
write_to(noo::AnyVarList const& list, flatbuffers::FlatBufferBuilder& b) {
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
    case noodles::AnyIDType::ObjectID:
        any_id = read_to_helper(n_any_id->id_as_ObjectID());
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
    case noodles::AnyIDType::TextureID:
        any_id = read_to_helper(n_any_id->id_as_TextureID());
        break;
    case noodles::AnyIDType::BufferID:
        any_id = read_to_helper(n_any_id->id_as_BufferID());
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
convert_id(MeshID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateGeometryID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::ObjectID>
convert_id(ObjectID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateObjectID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::SignalID>
convert_id(SignalID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateSignalID(b, id.id_slot, id.id_gen);
}
flatbuffers::Offset<::noodles::MethodID>
convert_id(MethodID id, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateMethodID(b, id.id_slot, id.id_gen);
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
MeshID convert_id(::noodles::GeometryID const& id) {
    return { id.id_slot(), id.id_gen() };
}
ObjectID convert_id(::noodles::ObjectID const& id) {
    return { id.id_slot(), id.id_gen() };
}
SignalID convert_id(::noodles::SignalID const& id) {
    return { id.id_slot(), id.id_gen() };
}
MethodID convert_id(::noodles::MethodID const& id) {
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
MeshID convert_id(::noodles::GeometryID const* p) {
    if (p) return convert_id(*p);
    return {};
}
ObjectID convert_id(::noodles::ObjectID const* p) {
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


} // namespace noo
