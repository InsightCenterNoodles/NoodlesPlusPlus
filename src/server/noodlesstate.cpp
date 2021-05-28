#include "noodlesstate.h"

#include "noodlesserver.h"
#include "serialize.h"

#include <QDebug>

namespace noo {

DocumentT::DocumentT(ServerT* s)
    : m_server(s),
      m_method_list(s),
      m_signal_list(s),
      m_buffer_list(s),
      m_light_list(s),
      m_mat_list(s),
      m_mesh_list(s),
      m_obj_list(s),
      m_tex_list(s),
      m_table_list(s) { }

BufferList& DocumentT::buffer_list() {
    return m_buffer_list;
}
LightList& DocumentT::light_list() {
    return m_light_list;
}

MethodList& DocumentT::method_list() {
    return m_method_list;
}
SignalList& DocumentT::signal_list() {
    return m_signal_list;
}
MaterialList& DocumentT::mat_list() {
    return m_mat_list;
}
MeshList& DocumentT::mesh_list() {
    return m_mesh_list;
}
ObjectList& DocumentT::obj_list() {
    return m_obj_list;
}
TextureList& DocumentT::tex_list() {
    return m_tex_list;
}
TableList& DocumentT::table_list() {
    return m_table_list;
}


AttachedMethodList& DocumentT::att_method_list() {
    return m_att_method_list_search;
}
AttachedSignalList& DocumentT::att_signal_list() {
    return m_att_signal_list_search;
}

void DocumentT::update(DocumentData const& d, Writer& w) {
    m_doc_method_list = d.method_list;
    m_doc_signal_list = d.signal_list;

    m_att_method_list_search = m_doc_method_list;
    m_att_signal_list_search = m_doc_signal_list;


    auto mv = make_id_list(m_doc_method_list, w);
    auto sv = make_id_list(m_doc_signal_list, w);

    auto x = noodles::CreateDocumentUpdateDirect(w, &mv, &sv);

    w.complete_message(x);
}

void DocumentT::update(DocumentData const& d) {
    auto w = m_server->get_broadcast_writer();

    update(d, *w);
}

void DocumentT::write_refresh(Writer& w) {
    auto mv = make_id_list(m_doc_method_list, w);
    auto sv = make_id_list(m_doc_signal_list, w);

    auto x = noodles::CreateDocumentUpdateDirect(w, &mv, &sv);

    w.complete_message(x);
}


MethodTPtr DocumentT::get_builtin(BuiltinMethods e) {
    if (!m_builtin_methods.count(e)) { build_table_builtins(); }
    return m_builtin_methods.at(e);
}

SignalTPtr DocumentT::get_builtin(BuiltinSignals e) {
    if (!m_builtin_signals.count(e)) { build_table_builtins(); }
    return m_builtin_signals.at(e);
}

// Table Builtins ==============================================================

static TableTPtr get_table(MethodContext const& context) {
    auto ctlb = context.get_table();
    if (!ctlb)
        throw MethodException(MethodException::CLIENT,
                              "Can only be called on a table.");
    return ctlb;
}


static AnyVar table_subscribe(MethodContext const& context,
                              AnyVarListRef const& /*args*/) {

    qDebug() << Q_FUNC_INFO;

    auto tbl = get_table(context);

    QObject::connect(tbl.get(),
                     &TableT::send_data,
                     context.client,
                     &ClientT::send,
                     Qt::UniqueConnection);

    auto& source = *tbl->get_source();


    AnyVarMap return_obj;

    return_obj["columns"] = source.get_headers();

    auto q = source.get_all_data();

    {
        std::vector<int64_t> keys(q->num_rows);

        q->get_keys_to(keys);

        return_obj["keys"] = std::move(keys);
    }

    {
        AnyVarList lv(q->num_cols);

        for (size_t ci = 0; ci < lv.size(); ci++) {
            if (q->is_column_string(ci)) {
                AnyVarList data(q->num_rows);

                for (size_t ri = 0; ri < data.size(); ri++) {
                    std::string_view view;
                    q->get_cell_to(ci, ri, view);
                    data[ri] = std::string(view);
                }

                lv[ci] = std::move(data);

            } else {
                std::vector<double> data(q->num_rows);

                q->get_reals_to(ci, data);

                lv[ci] = std::move(data);
            }
        }

        return_obj["data"] = std::move(lv);
    }

    {
        auto const& selections = source.get_all_selections();

        AnyVarList lv;

        for (auto const& [k, v] : selections) {
            AnyVarList entry;
            entry.push_back(k);
            entry.emplace_back(v.to_any());

            lv.push_back(std::move(entry));
        }

        return_obj["selections"] = std::move(lv);
    }


    return return_obj;
}

static auto get_builtin(TableT* tbl, BuiltinSignals b) {
    auto* server = server_from_component(tbl);
    return server->state()->document()->get_builtin(b);
}


static AnyVar table_data_insert(MethodContext const& context, AnyListArg ref) {
    auto tbl = get_table(context);

    bool ok = tbl->get_source()->ask_insert(ref.list);

    if (!ok) {
        throw MethodException(MethodException::CLIENT, "Unable to insert data");
    }

    return {};
}


static AnyVar table_data_update(MethodContext const& context,
                                AnyVarRef            keys,
                                AnyListArg           cols) {
    auto tbl = get_table(context);

    bool ok = tbl->get_source()->ask_update(keys, cols.list);

    if (!ok) {
        throw MethodException(MethodException::CLIENT, "Unable to update data");
    }

    return {};
}

static AnyVar table_data_remove(MethodContext const& context, AnyVarRef keys) {
    auto tbl = get_table(context);

    bool ok = tbl->get_source()->ask_delete(keys);

    if (!ok) {
        throw MethodException(MethodException::CLIENT, "Unable to remove data");
    }

    return {};
}

static AnyVar table_data_clear(MethodContext const& context) {
    auto tbl = get_table(context);

    bool ok = tbl->get_source()->ask_clear();

    if (!ok) {
        throw MethodException(MethodException::CLIENT, "Unable to clear table");
    }

    return {};
}

static AnyVar table_update_selection(MethodContext const& context,
                                     std::string_view     selection_id,
                                     SelectionRef         selection_ref) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = get_table(context);

    auto* src = tbl->get_source();

    bool ok = src->ask_update_selection(selection_id, selection_ref);

    if (!ok) {
        throw MethodException(MethodException::CLIENT,
                              "Unable to update selection");
    }


    return {};
}

// Object Builtins =============================================================

static ObjectTPtr get_object(MethodContext const& context) {
    auto ctlb = context.get_object();
    if (!ctlb)
        throw MethodException(MethodException::CLIENT,
                              "Can only be called on an object.");
    return ctlb;
}

static ObjectCallbacks* get_callbacks(ObjectTPtr const& p) {
    auto* cb = p->callbacks();
    if (!cb) {
        throw MethodException(
            MethodException::CLIENT,
            "Object supports methods, but does not have a method "
            "implementation. This is an application issue.");
    }
    return cb;
}

struct StringOrIntArgument
    : std::variant<std::monostate, int64_t, std::string> {

    StringOrIntArgument() = default;

    StringOrIntArgument(AnyVarRef const& a) {
        if (a.has_int()) {
            emplace<int64_t>(a.to_int());
        } else if (a.has_string()) {
            emplace<std::string>(a.to_string());
        }
    }
};

static AnyVar object_activate(MethodContext const& context,
                              StringOrIntArgument  arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (std::holds_alternative<int64_t>(arg)) {
        cb->on_activate_int(std::get<int64_t>(arg));
        return {};
    }

    if (std::holds_alternative<std::string>(arg)) {
        cb->on_activate_str(std::get<std::string>(arg));
        return {};
    }

    throw MethodException(MethodException::CLIENT,
                          "Argument must be int or string!");
}

static AnyVar object_get_activate_choices(MethodContext const& context) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    return cb->get_activation_choices();
}

static AnyVar object_get_option_choices(MethodContext const& context) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    return cb->get_option_choices();
}

static AnyVar object_get_current_option(MethodContext const& context) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    return cb->get_current_option();
}

static AnyVar object_set_current_option(MethodContext const& context,
                                        std::string_view     arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    cb->set_current_option(std::string(arg));

    return {};
}

static AnyVar object_set_position(MethodContext const& context, Vec3Arg arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!arg)
        throw MethodException(MethodException::CLIENT, "Need a vec3 argument");

    cb->set_position(*arg);

    return {};
}

static AnyVar object_set_rotation(MethodContext const& context, Vec4Arg v4arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!v4arg)
        throw MethodException(MethodException::CLIENT, "Need a vec4 argument");

    auto arg = *v4arg;

    cb->set_rotation(glm::quat(arg.w, arg.x, arg.y, arg.z));

    return {};
}

static AnyVar object_set_scale(MethodContext const& context, Vec3Arg arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!arg)
        throw MethodException(MethodException::CLIENT, "Need a vec3 argument");

    cb->set_scale(*arg);

    return {};
}

static AnyVar object_select_region(MethodContext const& context,
                                   Vec3Arg              min,
                                   Vec3Arg              max,
                                   BoolArg              select) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!min or !max or !select)
        throw MethodException(MethodException::CLIENT,
                              "Need a vec3 min and max argument!");

    cb->select_region(*min,
                      *max,
                      *select ? ObjectCallbacks::SELECT
                              : ObjectCallbacks::DESELECT);

    return {};
}

static AnyVar object_select_sphere(MethodContext const& context,
                                   Vec3Arg              p,
                                   double               radius,
                                   BoolArg              select) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!p or !select)
        throw MethodException(MethodException::CLIENT, "Need a vec3 position!");

    cb->select_sphere(*p,
                      radius,
                      *select ? ObjectCallbacks::SELECT
                              : ObjectCallbacks::DESELECT);

    return {};
}

static AnyVar object_select_plane(MethodContext const& context,
                                  Vec3Arg              p,
                                  Vec3Arg              n,
                                  BoolArg              select) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!p or !n or !select)
        throw MethodException(MethodException::CLIENT,
                              "Need vec3 position and normal!");

    cb->select_plane(
        *p, *n, *select ? ObjectCallbacks::SELECT : ObjectCallbacks::DESELECT);

    return {};
}


static AnyVar object_probe_at(MethodContext const& context, Vec3Arg p) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!p)
        throw MethodException(MethodException::CLIENT, "Need a vec3 position!");

    return cb->probe_at(*p);
}

void DocumentT::build_table_methods() {
    using namespace std::string_view_literals;

    {
        MethodData d;
        d.method_name          = "tbl_subscribe"sv;
        d.documentation        = "Subscribe to this table's signals"sv;
        d.return_documentation = "A table initialization object."sv;
        d.code                 = table_subscribe;

        m_builtin_methods[BuiltinMethods::TABLE_SUBSCRIBE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name = "tbl_insert"sv;
        d.documentation =
            "Request that given data be inserted into the table."sv;
        d.argument_documentation = {
            { "[ col ]",
              "A list of columns to insert. Columns must be the same length." },
        };
        d.return_documentation = "None"sv;
        d.set_code(table_data_insert);

        m_builtin_methods[BuiltinMethods::TABLE_INSERT] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name   = "tbl_update"sv;
        d.documentation = "Request that rows be updated with given data."sv;
        d.argument_documentation = {
            { "[keys]", "Integer list of keys to update" },
            { "[cols]", "Data to use to update the table" },
        };
        d.return_documentation = "None"sv;
        d.set_code(table_data_update);

        m_builtin_methods[BuiltinMethods::TABLE_UPDATE] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name            = "tbl_remove"sv;
        d.documentation          = "Request that data be deleted"sv;
        d.argument_documentation = { { "[keys]", "A list of keys to delete" } };
        d.return_documentation   = "None"sv;
        d.set_code(table_data_remove);

        m_builtin_methods[BuiltinMethods::TABLE_REMOVE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = "tbl_update_selection"sv;
        d.documentation          = "Set the table selection."sv;
        d.argument_documentation = {
            {
                "string",
                "Name of the selection to update",
            },
            { "selection_data", "A SelectionObject" },
        };
        d.return_documentation = "None"sv;
        d.set_code(table_update_selection);

        m_builtin_methods[BuiltinMethods::TABLE_UPDATE_SELECTION] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = "tbl_clear"sv;
        d.documentation        = "Request to clear all data and selections"sv;
        d.return_documentation = "None"sv;
        d.set_code(table_data_clear);

        m_builtin_methods[BuiltinMethods::TABLE_CLEAR] = create_method(this, d);
    }


    // === object methods

    {
        MethodData d;
        d.method_name            = "activate"sv;
        d.documentation          = "Activate the object"sv;
        d.argument_documentation = {
            { "int | string",
              "Either a string (for the activation name) or an integer for the "
              "activation index." }
        };
        d.return_documentation = "None"sv;

        d.set_code(object_activate);

        m_builtin_methods[BuiltinMethods::OBJ_ACTIVATE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = "get_activation_choices"sv;
        d.documentation        = "Get the names of activations on the object"sv;
        d.return_documentation = "[string]"sv;

        d.set_code(object_get_activate_choices);

        m_builtin_methods[BuiltinMethods::OBJ_GET_ACTIVATE_CHOICES] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = "get_option_choices"sv;
        d.documentation        = "Get the names of options on the object"sv;
        d.return_documentation = "[string]"sv;

        d.set_code(object_get_option_choices);

        m_builtin_methods[BuiltinMethods::OBJ_GET_OPTS] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = "get_current_option"sv;
        d.documentation        = "Get the current option name"sv;
        d.return_documentation = "string"sv;

        d.set_code(object_get_current_option);

        m_builtin_methods[BuiltinMethods::OBJ_GET_CURR_OPT] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = "set_current_option"sv;
        d.documentation          = "Set the current option on an object"sv;
        d.argument_documentation = { { "string", "The option name to set." } };
        d.return_documentation   = "None"sv;

        d.set_code(object_set_current_option);

        m_builtin_methods[BuiltinMethods::OBJ_SET_CURR_OPT] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = "set_position"sv;
        d.documentation          = "Ask to set the object position."sv;
        d.argument_documentation = {
            { "vec3", "A list of 3 reals as an object local position" }
        };
        d.return_documentation = "None"sv;

        d.set_code(object_set_position);

        m_builtin_methods[BuiltinMethods::OBJ_SET_POS] = create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = "set_rotation"sv;
        d.documentation          = "Ask to set the object rotation."sv;
        d.argument_documentation = { { "vec4",
                                       "A list of 4 reals as an object local "
                                       "rotation in quaternion form." } };
        d.return_documentation   = "None"sv;

        d.set_code(object_set_rotation);

        m_builtin_methods[BuiltinMethods::OBJ_SET_ROT] = create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = "set_scale"sv;
        d.documentation          = "Ask to set the object scale."sv;
        d.argument_documentation = {
            { "vec3", "A list of 3 reals as an object local scale." }
        };
        d.return_documentation = "None"sv;

        d.set_code(object_set_scale);

        m_builtin_methods[BuiltinMethods::OBJ_SET_SCALE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = "select_region"sv;
        d.documentation          = "Ask the object to select an AABB region."sv;
        d.argument_documentation = {
            { "vec3", "The minimum extent of the BB" },
            { "vec3", "The maximum extent of the BB" },
            { "bool", "Select (true) or deselect (false)" },
        };
        d.return_documentation = "None"sv;

        d.set_code(object_select_region);

        m_builtin_methods[BuiltinMethods::OBJ_SEL_REGION] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = "select_sphere"sv;
        d.documentation = "Ask the object to select a spherical region."sv;
        d.argument_documentation = {
            { "vec3", "The position of the sphere center" },
            { "real", "The radius of the sphere" },
            { "bool", "Select (true) or deselect (false)" },
        };
        d.return_documentation = "None"sv;

        d.set_code(object_select_sphere);

        m_builtin_methods[BuiltinMethods::OBJ_SEL_SPHERE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = "select_plane"sv;
        d.documentation = "Ask the object to select a half plane region"sv;
        d.argument_documentation = {
            { "vec3", "A position on the plane" },
            { "vec3", "The normal of the plane" },
            { "bool", "Select (true) or deselect (false)" },
        };
        d.return_documentation = "None"sv;

        d.set_code(object_select_plane);

        m_builtin_methods[BuiltinMethods::OBJ_SEL_PLANE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name = "probe_at"sv;
        d.documentation =
            "Ask the object to probe a given point. Returns a list of a string to display, and a possibly edited (or snapped) point."sv;
        d.argument_documentation = { { "vec3",
                                       "A position to probe, object local" } };
        d.return_documentation   = "[ string, vec3 ]"sv;

        d.set_code(object_probe_at);

        m_builtin_methods[BuiltinMethods::OBJ_PROBE] = create_method(this, d);
    }
}

void DocumentT::build_table_signals() {
    using namespace std::string_view_literals;

    {
        SignalData d;
        d.signal_name   = "tbl_reset"sv;
        d.documentation = "The table has been reset, and cleared."sv;

        m_builtin_signals[BuiltinSignals::TABLE_SIG_RESET] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name   = "tbl_updated"sv;
        d.documentation = "Rows have been inserted or updated in the table"sv;
        std::string args[] = {
            "[key]",
            "[col]",
        };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_DATA_UPDATED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = "tbl_rows_removed"sv;
        d.documentation    = "Rows have been deleted from the table"sv;
        std::string args[] = { "[key]" };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_ROWS_DELETED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = "tbl_selection_updated"sv;
        d.documentation    = "A selection of the table has changed"sv;
        std::string args[] = {
            "Selection ID",
            "Selection Object",
        };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_SELECTION_CHANGED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name = "signal_attention"sv;
        d.documentation =
            "User attention is requested. The two arguments may be omitted from the signal."sv;
        std::string args[] = {
            "position",
            "text",
        };

        m_builtin_signals[BuiltinSignals::OBJ_SIG_ATT] = create_signal(this, d);
    }
}

void DocumentT::build_table_builtins() {
    build_table_methods();
    build_table_signals();
}


NoodlesState::NoodlesState(ServerT* parent)
    : QObject(parent), m_parent(parent) {
    m_document = std::make_shared<DocumentT>(m_parent);
}

std::shared_ptr<DocumentT> const& NoodlesState::document() {
    return m_document;
}

} // namespace noo
