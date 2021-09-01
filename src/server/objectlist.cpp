#include "objectlist.h"

#include "noodlesserver.h"
#include "noodlesstate.h"
#include "serialize.h"
#include "src/generated/interface_tools.h"

namespace noo {

ObjectList::ObjectList(ServerT* s) : ComponentListBase(s) { }

ObjectList::~ObjectList() { }

// =============================================================================

class ObjectTUpdateHelper {
public:
    bool name        = false;
    bool parent      = false;
    bool transform   = false;
    bool material    = false;
    bool mesh        = false;
    bool lights      = false;
    bool tables      = false;
    bool instances   = false;
    bool tags        = false;
    bool method_list = false;
    bool signal_list = false;
    bool text        = false;
};

ObjectT::ObjectT(IDType id, ObjectList* host, ObjectData const& d)
    : ComponentMixin(id, host), m_data(d) {

    if (m_data.create_callbacks) {
        m_callback = m_data.create_callbacks(this);

        auto const& enabled = m_callback->callbacks_enabled();

        auto doc = get_document(m_parent_list->m_server);

        auto add_bt = [this, &doc](BuiltinMethods b) {
            m_data.method_list.push_back(doc->get_builtin(b));
        };

        if (enabled.activation) {
            add_bt(BuiltinMethods::OBJ_ACTIVATE);
            add_bt(BuiltinMethods::OBJ_GET_ACTIVATE_CHOICES);
        }

        if (enabled.options) {
            add_bt(BuiltinMethods::OBJ_GET_OPTS);
            add_bt(BuiltinMethods::OBJ_GET_CURR_OPT);
            add_bt(BuiltinMethods::OBJ_SET_CURR_OPT);
        }

        if (enabled.transform_position) { add_bt(BuiltinMethods::OBJ_SET_POS); }
        if (enabled.transform_rotation) { add_bt(BuiltinMethods::OBJ_SET_ROT); }
        if (enabled.transform_scale) { add_bt(BuiltinMethods::OBJ_SET_SCALE); }

        if (enabled.selection) {
            add_bt(BuiltinMethods::OBJ_SEL_REGION);
            add_bt(BuiltinMethods::OBJ_SEL_SPHERE);
            add_bt(BuiltinMethods::OBJ_SEL_PLANE);
            add_bt(BuiltinMethods::OBJ_SEL_HULL);
        }

        if (enabled.probing) { add_bt(BuiltinMethods::OBJ_PROBE); }

        if (enabled.attention_signals) {
            connect(m_callback.get(),
                    &ObjectCallbacks::signal_attention_plain,
                    this,
                    &ObjectT::on_signal_attention_plain);

            connect(m_callback.get(),
                    &ObjectCallbacks::signal_attention_at,
                    this,
                    &ObjectT::on_signal_attention_at);

            connect(m_callback.get(),
                    &ObjectCallbacks::signal_attention_anno,
                    this,
                    &ObjectT::on_signal_attention_anno);
        }
    }


    m_method_search = m_data.method_list;
    m_signal_search = m_data.signal_list;
}

static std::vector<noodles::Mat4>
make_inst_list(std::vector<glm::mat4> const& v) {
    static_assert(sizeof(noodles::Mat4) == sizeof(glm::mat4));

    std::vector<noodles::Mat4> ret;

    if (v.empty()) return ret;

    ret.resize(v.size());

    memcpy(ret.data(), v.data(), sizeof(glm::mat4) * v.size());

    return ret;
}

template <class T>
using OffsetList = std::vector<flatbuffers::Offset<T>>;

void ObjectT::update_common(ObjectTUpdateHelper const& opt, Writer& w) {

    auto lid = convert_id(id(), w);


    std::optional<flatbuffers::Offset<noodles::ObjectID>>   update_parent;
    std::optional<noodles::Mat4>                            update_transform;
    std::optional<flatbuffers::Offset<noodles::MaterialID>> update_material;
    std::optional<flatbuffers::Offset<noodles::GeometryID>> update_mesh;
    std::optional<OffsetList<noodles::LightID>>             update_lights;
    std::optional<OffsetList<noodles::TableID>>             update_tables;
    std::optional<std::vector<noodles::Mat4>>               update_instances;
    std::optional<std::vector<flatbuffers::Offset<flatbuffers::String>>>
                                                 update_tags;
    std::optional<OffsetList<noodles::MethodID>> update_methods_list;
    std::optional<OffsetList<noodles::SignalID>> update_signals_list;
    std::optional<flatbuffers::Offset<noodles::TextDefinition>> update_text;

    if (opt.parent) { update_parent = convert_id(m_data.parent, w); }
    if (opt.transform) { update_transform = convert(m_data.transform); }
    if (opt.material) { update_material = convert_id(m_data.material, w); }
    if (opt.mesh) { update_mesh = convert_id(m_data.mesh, w); }
    if (opt.lights) { update_lights = make_id_list(m_data.lights, w); }
    if (opt.tables) { update_tables = make_id_list(m_data.tables, w); }
    if (opt.instances) { update_instances = make_inst_list(m_data.instances); }
    if (opt.tags) {
        auto& ret = update_tags.emplace();
        for (auto const& s : m_data.tags) {
            ret.push_back(w->CreateString(s));
        }
    }
    if (opt.method_list) {
        update_methods_list = make_id_list(m_data.method_list, w);
    }
    if (opt.signal_list) {
        update_signals_list = make_id_list(m_data.signal_list, w);
    }

    if (opt.text and m_data.text) {
        update_text =
            noodles::CreateTextDefinitionDirect(w,
                                                m_data.text->text.c_str(),
                                                m_data.text->font.c_str(),
                                                m_data.text->height,
                                                m_data.text->width);
    }

    auto opt_or    = [](auto& o) { return o ? &o.value() : nullptr; };
    auto id_opt_or = [](auto& o) {
        if (o) return o.value();
        using T = std::remove_cvref_t<decltype(o.value())>;
        return T();
    };


    auto x = noodles::CreateObjectCreateUpdateDirect(
        w,
        lid,
        opt.name ? m_data.name.c_str() : nullptr,
        id_opt_or(update_parent),
        opt_or(update_transform),
        id_opt_or(update_material),
        id_opt_or(update_mesh),
        opt_or(update_lights),
        opt_or(update_tables),
        opt_or(update_instances),
        opt_or(update_tags),
        opt_or(update_methods_list),
        opt_or(update_signals_list),
        update_text.value_or(flatbuffers::Offset<noodles::TextDefinition>()));

    w.complete_message(x);
}

void ObjectT::write_new_to(Writer& w) {
    ObjectTUpdateHelper update_opts;

    update_opts.name        = true;
    update_opts.parent      = true;
    update_opts.transform   = true;
    update_opts.material    = true;
    update_opts.mesh        = true;
    update_opts.lights      = true;
    update_opts.tables      = true;
    update_opts.instances   = true;
    update_opts.tags        = true;
    update_opts.method_list = true;
    update_opts.signal_list = true;
    update_opts.text        = true;

    update_common(update_opts, w);
}

// moving from an optional does NOT reset the optional! we can thus use it as
// change flags

#define CHECK_UPDATE(FLD)                                                      \
    {                                                                          \
        if (data.FLD) {                                                        \
            update_opts.FLD = true;                                            \
            m_data.FLD      = std::move(*data.FLD);                            \
        }                                                                      \
    }

void ObjectT::update(ObjectUpdateData& data, Writer& w) {
    ObjectTUpdateHelper update_opts;

    CHECK_UPDATE(name)
    CHECK_UPDATE(parent)
    CHECK_UPDATE(transform)
    CHECK_UPDATE(material)
    CHECK_UPDATE(mesh)
    CHECK_UPDATE(lights)
    CHECK_UPDATE(tables)
    CHECK_UPDATE(instances)
    CHECK_UPDATE(tags)
    CHECK_UPDATE(method_list)
    CHECK_UPDATE(signal_list)
    CHECK_UPDATE(text)

    if (update_opts.method_list) { m_method_search = m_data.method_list; }
    if (update_opts.signal_list) { m_signal_search = m_data.signal_list; }

    update_common(update_opts, w);
}

void ObjectT::update(ObjectUpdateData& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void ObjectT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateObjectDelete(w, lid);

    w.complete_message(x);
}

AttachedMethodList& ObjectT::att_method_list() {
    return m_method_search;
}
AttachedSignalList& ObjectT::att_signal_list() {
    return m_signal_search;
}

ObjectCallbacks* ObjectT::callbacks() const {
    return m_callback.get();
}

void ObjectT::on_signal_attention_plain() {
    auto doc = get_document(m_parent_list->m_server);

    auto sig = doc->get_builtin(BuiltinSignals::OBJ_SIG_ATT);

    issue_signal(this, sig.get());
}
void ObjectT::on_signal_attention_at(glm::vec3 p) {
    auto doc = get_document(m_parent_list->m_server);

    auto sig = doc->get_builtin(BuiltinSignals::OBJ_SIG_ATT);

    issue_signal(this, sig.get(), to_any(p));
}
void ObjectT::on_signal_attention_anno(glm::vec3 p, std::string t) {
    auto doc = get_document(m_parent_list->m_server);

    auto sig = doc->get_builtin(BuiltinSignals::OBJ_SIG_ATT);

    issue_signal(this, sig.get(), to_any(p), t);
}

} // namespace noo
