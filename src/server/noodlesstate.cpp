#include "noodlesstate.h"

#include "noodlesserver.h"
#include "src/common/serialize.h"
#include "src/server/assetstorage.h"

#include <QDebug>

namespace noo {

DocumentT::DocumentT(ServerT* s, ServerOptions const& options)
    : m_server(s),
      m_method_list(s),
      m_signal_list(s),
      m_buffer_list(s),
      m_buffer_view_list(s),
      m_image_list(s),
      m_light_list(s),
      m_mat_list(s),
      m_mesh_list(s),
      m_obj_list(s),
      m_sampler_list(s),
      m_tex_list(s),
      m_table_list(s),
      m_plot_list(s) {

    m_storage = new AssetStorage(options, this);
}

AssetStorage& DocumentT::storage() {
    return *m_storage;
}

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
PlotList& DocumentT::plot_list() {
    return m_plot_list;
}
TableList& DocumentT::table_list() {
    return m_table_list;
}
BufferViewList& DocumentT::buffer_view_list() {
    return m_buffer_view_list;
}
SamplerList& DocumentT::sampler_list() {
    return m_sampler_list;
}
ImageList& DocumentT::image_list() {
    return m_image_list;
}


AttachedMethodList& DocumentT::att_method_list() {
    return m_att_method_list_search;
}
AttachedSignalList& DocumentT::att_signal_list() {
    return m_att_signal_list_search;
}

void DocumentT::update(DocumentData const& d, SMsgWriter& w) {

    messages::MsgDocumentUpdate m;

    if (d.method_list) {
        m_doc_method_list        = d.method_list.value();
        m_att_method_list_search = m_doc_method_list;
        m.methods_list           = delegates_to_ids(m_doc_method_list);
    }
    if (d.signal_list) {
        m_doc_signal_list        = d.signal_list.value();
        m_att_signal_list_search = m_doc_signal_list;
        m.signals_list           = delegates_to_ids(m_doc_signal_list);
    }

    w.add(m);
}

void DocumentT::update(DocumentData const& d) {
    auto w = m_server->get_broadcast_writer();

    update(d, *w);
}

void DocumentT::write_refresh(SMsgWriter& w) {

    messages::MsgDocumentUpdate m;
    m.methods_list = delegates_to_ids(m_doc_method_list);
    m.signals_list = delegates_to_ids(m_doc_signal_list);

    w.add(m);
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
        throw MethodException(ErrorCodes::INVALID_REQUEST,
                              "Can only be called on a table.");
    return ctlb;
}

static QCborValue table_subscribe(MethodContext const& context,
                                  QCborArray const& /*args*/) {

    // qDebug() << Q_FUNC_INFO;

    auto tbl = get_table(context);

    QObject::connect(tbl.get(),
                     &TableT::send_data,
                     context.client,
                     &ClientT::send,
                     Qt::UniqueConnection);

    auto& source = *tbl->get_source();


    return make_table_init_data(source);
}

static QCborValue table_data_insert(MethodContext const& context,
                                    AnyListArg           ref) {
    auto tbl = get_table(context);

    tbl->get_source()->handle_insert(ref.list);

    return {};
}


static QCborValue table_data_update(MethodContext const& context,
                                    QCborValue           keys,
                                    AnyListArg           rows) {
    auto tbl = get_table(context);

    tbl->get_source()->handle_update(keys.toArray(), rows.list);

    return {};
}

static QCborValue table_data_remove(MethodContext const& context,
                                    QCborValue           keys) {
    auto tbl = get_table(context);

    tbl->get_source()->handle_deletion(keys.toArray());

    return {};
}

static QCborValue table_data_clear(MethodContext const& context) {
    auto tbl = get_table(context);

    tbl->get_source()->handle_reset();

    return {};
}

static QCborValue table_update_selection(MethodContext const& context,
                                         Selection            selection_ref) {
    // qDebug() << Q_FUNC_INFO;

    auto tbl = get_table(context);

    auto* src = tbl->get_source();

    src->handle_set_selection(selection_ref);

    return {};
}

// Object Builtins =============================================================

static ObjectTPtr get_object(MethodContext const& context) {
    auto ctlb = context.get_object();
    if (!ctlb)
        throw MethodException(ErrorCodes::INVALID_REQUEST,
                              "Can only be called on an object.");
    return ctlb;
}

static EntityCallbacks* get_callbacks(ObjectTPtr const& p) {
    auto* cb = p->callbacks();
    if (!cb) {
        throw MethodException(
            ErrorCodes::INTERNAL_ERROR,
            "Object supports methods, but does not have an implementation for "
            "the called method implementation. This is an application issue.");
    }
    return cb;
}

struct StringOrIntArgument : std::variant<std::monostate, int64_t, QString> {

    StringOrIntArgument() = default;

    StringOrIntArgument(QCborValue const& a) {
        if (a.isInteger()) {
            emplace<int64_t>(a.toInteger());
        } else if (a.isString()) {
            emplace<QString>(a.toString());
        }
    }
};

static QCborValue object_activate(MethodContext const& context,
                                  StringOrIntArgument  arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (std::holds_alternative<int64_t>(arg)) {
        cb->on_activate_int(std::get<int64_t>(arg));
        return {};
    }

    if (std::holds_alternative<QString>(arg)) {
        cb->on_activate_str(std::get<QString>(arg));
        return {};
    }

    throw MethodException(ErrorCodes::INVALID_PARAMS,
                          "Argument must be int or string!");
}

static QCborValue object_get_activate_choices(MethodContext const& context) {
    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    return QCborArray::fromStringList(cb->get_activation_choices());
}

static QCborValue object_get_var_keys(MethodContext const& context) {
    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    return QCborArray::fromStringList(cb->get_var_keys());
}

static QCborValue object_get_var_options(MethodContext const& context,
                                         QString              key) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    return cb->get_var_options(key);
}

static QCborValue object_get_var_value(MethodContext const& context,
                                       QString              key) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    return cb->get_var_value(key);
}

static QCborValue
object_set_var_value(MethodContext const& context, QString value, QString key) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    return cb->set_var_value(value, key);
}

static QCborValue object_set_position(MethodContext const& context,
                                      Vec3Arg              arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!arg)
        throw MethodException(ErrorCodes::INVALID_PARAMS,
                              "Need a vec3 argument");

    cb->set_position(*arg);

    return {};
}

static QCborValue object_set_rotation(MethodContext const& context,
                                      Vec4Arg              v4arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!v4arg)
        throw MethodException(ErrorCodes::INVALID_PARAMS,
                              "Need a vec4 argument");

    auto arg = *v4arg;

    cb->set_rotation(glm::quat(arg.w, arg.x, arg.y, arg.z));

    return {};
}

static QCborValue object_set_scale(MethodContext const& context, Vec3Arg arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!arg)
        throw MethodException(ErrorCodes::INVALID_PARAMS,
                              "Need a vec3 argument");

    cb->set_scale(*arg);

    return {};
}

static EntityCallbacks::SelAction decode_selection_action(int64_t i) {
    return EntityCallbacks::SelAction(std::clamp<int64_t>(i, -1, 1));
}

static QCborValue object_select_region(MethodContext const& context,
                                       Vec3Arg              min,
                                       Vec3Arg              max,
                                       IntArg               select) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!min or !max or !select)
        throw MethodException(ErrorCodes::INVALID_PARAMS,
                              "Need a vec3 min and max argument!");

    cb->select_region(*min, *max, decode_selection_action(*select));

    return {};
}

static QCborValue object_select_sphere(MethodContext const& context,
                                       Vec3Arg              p,
                                       double               radius,
                                       IntArg               select) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!p or !select)
        throw MethodException(ErrorCodes::INVALID_PARAMS,
                              "Need a vec3 position!");

    cb->select_sphere(*p, radius, decode_selection_action(*select));

    return {};
}

static QCborValue object_select_plane(MethodContext const& context,
                                      Vec3Arg              p,
                                      Vec3Arg              n,
                                      IntArg               select) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!p or !n or !select)
        throw MethodException(ErrorCodes::INVALID_PARAMS,
                              "Need vec3 position and normal!");

    cb->select_plane(*p, *n, decode_selection_action(*select));

    return {};
}

static QCborValue object_select_hull(MethodContext const& context,
                                     Vec3ListArg          point_list,
                                     IntListArg           index_list,
                                     IntArg               select) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (point_list.empty() or index_list.list.size() == 0 or !select)
        throw MethodException(ErrorCodes::INVALID_PARAMS,
                              "Need a list of positions, a list of indicies, "
                              "and a boolean argument!");

    if (index_list.list.size() % 3 != 0) {
        throw MethodException(ErrorCodes::INVALID_PARAMS, "Indicies should be");
    }

    cb->select_hull(point_list,
                    std::span(index_list.list),
                    decode_selection_action(*select));

    return {};
}

static QCborValue object_probe_at(MethodContext const& context, Vec3Arg p) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    if (!p)
        throw MethodException(ErrorCodes::INVALID_PARAMS,
                              "Need a vec3 position!");

    auto p_result = cb->probe_at(*p);

    QCborArray ret;
    ret << p_result.first;
    ret << to_cbor(p_result.second);

    return ret;
}

void DocumentT::build_table_methods() {
    using namespace std::string_view_literals;

    {
        MethodData d;
        d.method_name          = noo::names::mthd_tbl_subscribe;
        d.documentation        = "Subscribe to this table's signals";
        d.return_documentation = "A table initialization object.";
        d.code                 = table_subscribe;

        m_builtin_methods[BuiltinMethods::TABLE_SUBSCRIBE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = noo::names::mthd_tbl_insert;
        d.documentation = "Request that given data be inserted into the table.";
        d.argument_documentation = {
            { "[ rows ]", "A list of rows to insert", QString() },
        };
        d.return_documentation = "None";
        d.set_code(table_data_insert);

        m_builtin_methods[BuiltinMethods::TABLE_INSERT] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name   = noo::names::mthd_tbl_update;
        d.documentation = "Request that rows be updated with given data.";
        d.argument_documentation = {
            { "[keys]", "Integer list of keys to update", QString() },
            { "[rows]", "Rows to use to update the table", QString() },
        };
        d.return_documentation = "None";
        d.set_code(table_data_update);

        m_builtin_methods[BuiltinMethods::TABLE_UPDATE] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name            = noo::names::mthd_tbl_remove;
        d.documentation          = "Request that data be deleted";
        d.argument_documentation = {
            { "[keys]", "A list of keys to delete", QString() }
        };
        d.return_documentation = "None";
        d.set_code(table_data_remove);

        m_builtin_methods[BuiltinMethods::TABLE_REMOVE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = noo::names::mthd_tbl_update_selection;
        d.documentation          = "Set the table selection.";
        d.argument_documentation = {
            { "string", "Name of the selection to update", QString() },
            { "selection_data", "A SelectionObject", QString() },
        };
        d.return_documentation = "None";
        d.set_code(table_update_selection);

        m_builtin_methods[BuiltinMethods::TABLE_UPDATE_SELECTION] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = noo::names::mthd_tbl_clear;
        d.documentation        = "Request to clear all data and selections";
        d.return_documentation = "None";
        d.set_code(table_data_clear);

        m_builtin_methods[BuiltinMethods::TABLE_CLEAR] = create_method(this, d);
    }


    // === object methods

    {
        MethodData d;
        d.method_name            = noo::names::mthd_activate;
        d.documentation          = "Activate the object";
        d.argument_documentation = {
            { "int | string",
              "Either a string (for the activation name) or an integer for the "
              "activation index.",
              QString() }
        };
        d.return_documentation = "None";

        d.set_code(object_activate);

        m_builtin_methods[BuiltinMethods::OBJ_ACTIVATE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = noo::names::mthd_get_activation_choices;
        d.documentation        = "Get the names of activations on the object";
        d.return_documentation = "[string]";

        d.set_code(object_get_activate_choices);

        m_builtin_methods[BuiltinMethods::OBJ_GET_ACTIVATE_CHOICES] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = noo::names::mthd_get_var_keys;
        d.documentation        = "Get the keys of any options on the object";
        d.return_documentation = "[string]";

        d.set_code(object_get_var_keys);

        m_builtin_methods[BuiltinMethods::OBJ_GET_KEYS] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = noo::names::mthd_get_var_options;
        d.documentation = "Get the list of valid options for this variable";
        d.argument_documentation = {
            { "key", "The optional variable key", QString() }
        };
        d.return_documentation = "[any]";

        d.set_code(object_get_var_options);

        m_builtin_methods[BuiltinMethods::OBJ_VAR_OPTS] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = noo::names::mthd_get_var_value;
        d.documentation          = "Get the given variable content";
        d.argument_documentation = {
            { "key", "The optional variable key", QString() }
        };
        d.return_documentation = "any";

        d.set_code(object_get_var_value);

        m_builtin_methods[BuiltinMethods::OBJ_GET_VAR] = create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = noo::names::mthd_set_var_value;
        d.documentation          = "Set a given variable's value";
        d.argument_documentation = {
            { "value", "The new variable value content", QString() }
        };
        d.return_documentation = "success ";

        d.set_code(object_set_var_value);

        m_builtin_methods[BuiltinMethods::OBJ_SET_VAR] = create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = noo::names::mthd_set_position;
        d.documentation          = "Ask to set the object position.";
        d.argument_documentation = {
            { "vec3",
              "A list of 3 reals as an object local position",
              QString() }
        };
        d.return_documentation = "None";

        d.set_code(object_set_position);

        m_builtin_methods[BuiltinMethods::OBJ_SET_POS] = create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = noo::names::mthd_set_rotation;
        d.documentation          = "Ask to set the object rotation.";
        d.argument_documentation = { { "vec4",
                                       "A list of 4 reals as an object local "
                                       "rotation in quaternion form.",
                                       QString() } };
        d.return_documentation   = "None";

        d.set_code(object_set_rotation);

        m_builtin_methods[BuiltinMethods::OBJ_SET_ROT] = create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = noo::names::mthd_set_scale;
        d.documentation          = "Ask to set the object scale.";
        d.argument_documentation = {
            { "vec3", "A list of 3 reals as an object local scale.", QString() }
        };
        d.return_documentation = "None";

        d.set_code(object_set_scale);

        m_builtin_methods[BuiltinMethods::OBJ_SET_SCALE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name            = noo::names::mthd_select_region;
        d.documentation          = "Ask the object to select an AABB region.";
        d.argument_documentation = {
            { "vec3", "The minimum extent of the BB", QString() },
            { "vec3", "The maximum extent of the BB", QString() },
            { "bool", "Select (true) or deselect (false)", QString() },
        };
        d.return_documentation = "None";

        d.set_code(object_select_region);

        m_builtin_methods[BuiltinMethods::OBJ_SEL_REGION] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = noo::names::mthd_select_sphere;
        d.documentation = "Ask the object to select a spherical region.";
        d.argument_documentation = {
            { "vec3", "The position of the sphere center", QString() },
            { "real", "The radius of the sphere", QString() },
            { "bool", "Select (true) or deselect (false)", QString() },
        };
        d.return_documentation = "None";

        d.set_code(object_select_sphere);

        m_builtin_methods[BuiltinMethods::OBJ_SEL_SPHERE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = noo::names::mthd_select_half_plane;
        d.documentation = "Ask the object to select a half plane region";
        d.argument_documentation = {
            { "vec3", "A position on the plane", QString() },
            { "vec3", "The normal of the plane", QString() },
            { "bool", "Select (true) or deselect (false)", QString() },
        };
        d.return_documentation = "None";

        d.set_code(object_select_plane);

        m_builtin_methods[BuiltinMethods::OBJ_SEL_PLANE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = noo::names::mthd_select_hull;
        d.documentation = "Ask the object to select a convex hull region";
        d.argument_documentation = {
            { "[ vec3 ]", "A list of points", QString() },
            { "[ int64 ]",
              "A list of indicies, each triple is a triangle",
              QString() },
            { "bool", "Select (true) or deselect (false)", QString() },
        };
        d.return_documentation = "None";

        d.set_code(object_select_hull);

        m_builtin_methods[BuiltinMethods::OBJ_SEL_HULL] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name = noo::names::mthd_probe_at;
        d.documentation =
            "Ask the object to probe a given point. Returns a list of a string "
            "to display, and a possibly edited (or snapped) point.";
        d.argument_documentation = {
            { "vec3", "A position to probe, object local", QString() }
        };
        d.return_documentation = "[ string, vec3 ]";

        d.set_code(object_probe_at);

        m_builtin_methods[BuiltinMethods::OBJ_PROBE] = create_method(this, d);
    }
}

void DocumentT::build_table_signals() {
    using namespace std::string_view_literals;

    {
        SignalData d;
        d.signal_name   = noo::names::sig_tbl_reset;
        d.documentation = "The table has been reset, and cleared.";

        m_builtin_signals[BuiltinSignals::TABLE_SIG_RESET] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = noo::names::sig_tbl_updated;
        d.documentation    = "Rows have been inserted or updated in the table";
        std::string args[] = {
            "[key]",
            "[rows]",
        };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_DATA_UPDATED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = noo::names::sig_tbl_rows_removed;
        d.documentation    = "Rows have been deleted from the table";
        std::string args[] = { "[key]" };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_ROWS_DELETED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = noo::names::sig_tbl_selection_updated;
        d.documentation    = "A selection of the table has changed";
        std::string args[] = {
            "Selection ID",
            "Selection Object",
        };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_SELECTION_CHANGED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name   = noo::names::sig_signal_attention;
        d.documentation = "User attention is requested. The two arguments may "
                          "be omitted from the signal.";
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


NoodlesState::NoodlesState(ServerT* parent, ServerOptions const& options)
    : QObject(parent), m_parent(parent) {
    m_document = std::make_shared<DocumentT>(m_parent, options);
}

std::shared_ptr<DocumentT> const& NoodlesState::document() {
    return m_document;
}

} // namespace noo
