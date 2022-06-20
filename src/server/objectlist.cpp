#include "objectlist.h"

#include "noodlesserver.h"
#include "noodlesstate.h"
#include "src/common/serialize.h"
#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"
#include "src/server/plotlist.h"

namespace noo {

ObjectList::ObjectList(ServerT* s) : ComponentListBase(s) { }

ObjectList::~ObjectList() { }

// =============================================================================

ObjectT::ObjectT(IDType id, ObjectList* host, ObjectData const& d)
    : ComponentMixin(id, host), m_data(d) {

    if (m_data.create_callbacks) {
        m_callback = m_data.create_callbacks(this);

        auto const& enabled = m_callback->callbacks_enabled();

        auto doc = get_document(m_parent_list->m_server);

        auto add_bt = [this, &doc](BuiltinMethods b) {
            if (!m_data.method_list) m_data.method_list.emplace();
            m_data.method_list->push_back(doc->get_builtin(b));
        };

        if (enabled.activation) {
            add_bt(BuiltinMethods::OBJ_ACTIVATE);
            add_bt(BuiltinMethods::OBJ_GET_ACTIVATE_CHOICES);
        }

        if (enabled.options) {
            add_bt(BuiltinMethods::OBJ_GET_KEYS);
            add_bt(BuiltinMethods::OBJ_VAR_OPTS);
            add_bt(BuiltinMethods::OBJ_GET_VAR);
            add_bt(BuiltinMethods::OBJ_SET_VAR);
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
                    &EntityCallbacks::signal_attention_plain,
                    this,
                    &ObjectT::on_signal_attention_plain);

            connect(m_callback.get(),
                    &EntityCallbacks::signal_attention_at,
                    this,
                    &ObjectT::on_signal_attention_at);

            connect(m_callback.get(),
                    &EntityCallbacks::signal_attention_anno,
                    this,
                    &ObjectT::on_signal_attention_anno);
        }
    }


    if (m_data.method_list) m_method_search = *m_data.method_list;
    if (m_data.signal_list) m_signal_search = *m_data.signal_list;
}

template <class Message, class UP>
void update_common(ObjectT* obj, UP const& data, Message& m) {

    m.id = obj->id();

    if (data.parent) { m.parent = (*data.parent)->id(); }

    if (data.transform) { m.transform = (*data.transform); }

    if (data.definition) {
        VMATCH(
            *data.definition,
            VCASE(std::monostate) { m.null_rep = 1.0; },
            VCASE(ObjectTextDefinition const& t) {
                m.text_rep = messages::TextRepresentation {
                    .txt    = t.text,
                    .font   = t.font,
                    .height = t.height,
                    .width  = t.width,
                };
            },
            VCASE(ObjectWebpageDefinition const& t) {
                m.web_rep = messages::WebRepresentation {
                    .source = t.url,
                    .height = t.height,
                    .width  = t.width,
                };
            },
            VCASE(ObjectRenderableDefinition const& t) {
                messages::RenderRepresentation rep;

                rep.mesh = t.mesh->id();

                if (t.instances) {
                    auto& new_inst = rep.instances.emplace();

                    new_inst.view   = t.instances->view->id();
                    new_inst.stride = t.instances->stride;

                    if (t.instances->instance_bb) {
                        new_inst.bb = t.instances->instance_bb.value();
                    }
                }

                m.render_rep = rep;
            });
    }

    if (data.lights) { m.lights = delegates_to_ids(data.lights.value()); }

    if (data.tables) { m.tables = delegates_to_ids(data.tables.value()); }

    if (data.tables) { m.plots = delegates_to_ids(data.plots.value()); }

    if (data.tags) { m.tags = data.tags.value(); }

    if (data.method_list) {
        m.methods_list = delegates_to_ids(data.method_list.value());
    }

    if (data.signal_list) {
        m.signals_list = delegates_to_ids(data.signal_list.value());
    }

    if (data.influence) { m.influence = data.influence.value(); }
}

void ObjectT::write_new_to(SMsgWriter& w) {
    messages::MsgEntityCreate m;
    m.id   = id();
    m.name = opt_string(m_data.name);

    update_common(this, m_data, m);
    w.add(m);
}

#define CHECK_UPDATE(FLD)                                                      \
    {                                                                          \
        if (data.FLD) { m_data.FLD = std::move(*data.FLD); }                   \
    }

void ObjectT::update(ObjectUpdateData& data, SMsgWriter& w) {

    messages::MsgEntityUpdate m;
    m.id = id();

    update_common(this, data, m);

    w.add(m);

    CHECK_UPDATE(parent)
    CHECK_UPDATE(transform)
    CHECK_UPDATE(definition)
    CHECK_UPDATE(lights)
    CHECK_UPDATE(tables)
    CHECK_UPDATE(tags)
    CHECK_UPDATE(method_list)
    CHECK_UPDATE(signal_list)
    CHECK_UPDATE(influence)

    if (data.method_list) { m_method_search = *m_data.method_list; }
    if (data.signal_list) { m_signal_search = *m_data.signal_list; }
}

void ObjectT::update(ObjectUpdateData& data) {
    auto w = m_parent_list->new_bcast();

    update(data, *w);
}

void ObjectT::write_delete_to(SMsgWriter& w) {
    messages::MsgEntityDelete m;
    m.id = id();
    w.add(m);
}

AttachedMethodList& ObjectT::att_method_list() {
    return m_method_search;
}
AttachedSignalList& ObjectT::att_signal_list() {
    return m_signal_search;
}

EntityCallbacks* ObjectT::callbacks() const {
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

    issue_signal(this, sig.get(), to_cbor(p));
}
void ObjectT::on_signal_attention_anno(glm::vec3 p, QString t) {
    auto doc = get_document(m_parent_list->m_server);

    auto sig = doc->get_builtin(BuiltinSignals::OBJ_SIG_ATT);

    issue_signal(this, sig.get(), to_cbor(p), t);
}

} // namespace noo
