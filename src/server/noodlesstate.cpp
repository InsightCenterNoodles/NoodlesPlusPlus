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

#define GET_TABLE(CTX)                                                         \
    ({                                                                         \
        auto ctlb = CTX.get_table();                                           \
        if (!ctlb)                                                             \
            throw MethodException(MethodException::CLIENT,                     \
                                  "Can only be called on a table.");           \
        ctlb;                                                                  \
    })

static AnyVar table_subscribe(MethodContext const& context,
                              AnyVarList& /*args*/) {

    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    QObject::connect(tbl.get(),
                     &TableT::send_data,
                     context.client,
                     &ClientT::send,
                     Qt::UniqueConnection);

    return AnyVar();
}


static AnyVar table_get_columns(MethodContext const& context) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);
    // return a list of strings

    auto header = tbl->get_source()->get_columns();

    return AnyVar(header);
}

static int64_t table_get_num_rows(MethodContext const& context) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    return static_cast<int64_t>(tbl->get_source()->get_num_rows());
}

static std::vector<double> clear_query(QueryPtr& q) {
    std::vector<double> ret;
    if (!q) return ret;

    ret.reserve(128);

    while (true) {
        size_t n = q->next();

        if (n == 0) break;

        // allocate an extra chunk

        double* last = ret.data() + ret.size();

        ret.resize(ret.size() + n);

        q->write_next_to(std::span<double>(last, n));
    }

    return ret;
}

static std::vector<double> table_get_row(MethodContext const& context,
                                         int64_t              row,
                                         std::vector<int64_t> columns) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    {
        size_t max_row = src->get_num_rows() - 1;

        if (row < 0 or row > max_row) {
            throw MethodException(MethodException::CLIENT, "Row out of range");
        }
    }


    {
        size_t max_column = src->get_columns().size() - 1;

        for (auto c : columns) {
            if (c > max_column or c < 0) {
                throw MethodException(MethodException::CLIENT,
                                      "Row out of range");
            }
        }
    }

    auto query = tbl->get_source()->get_row(row, columns);

    return clear_query(query);
}

static auto get_builtin(TableT* tbl, BuiltinSignals b) {
    auto* server = server_from_component(tbl);
    return server->state()->document()->get_builtin(b);
}

static int64_t table_request_row_insert(MethodContext const& context,
                                        int64_t              row,
                                        RealListArg          data) {
    // qDebug() << Q_FUNC_INFO << QString::fromStdString(args.dump_string());

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    {
        size_t max_row = src->get_num_rows() - 1;

        if (row > max_row or row < 0)
            throw MethodException(MethodException::CLIENT, "Row out of range");
    }

    int64_t ok = tbl->get_source()->trigger_row_insert(row, data.list);

    if (ok) {
        auto sig = get_builtin(tbl.get(), BuiltinSignals::TABLE_SIG_ROWS_ADDED);

        issue_signal(
            tbl.get(), sig.get(), marshall_to_any(int64_t(row), int64_t(1)));
    }

    return ok;
}

static AnyVar table_request_row_update(MethodContext const& context,
                                       int64_t              row,
                                       RealListArg          data) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    {
        size_t max_row = src->get_num_rows() - 1;

        if (row > max_row or row < 0)
            throw MethodException(MethodException::CLIENT, "Row out of range");
    }

    int64_t ok = tbl->get_source()->trigger_row_update(row, data.list);

    if (ok) {
        Selection selection;

        selection.row_ranges.emplace_back(row, row + 1);

        auto sig =
            get_builtin(tbl.get(), BuiltinSignals::TABLE_SIG_DATA_UPDATED);

        issue_signal(tbl.get(), sig.get(), selection.to_any());
    }

    return ok;
}

struct ListOfListOfReals {
    std::vector<std::vector<double>> lol; // yeah... :(

    ListOfListOfReals() = default;

    ListOfListOfReals(AnyVar&& a) {
        auto l = a.steal_vector();

        for (auto& li : l) {
            lol.emplace_back(li.coerce_real_list());
        }
    }
};

static AnyVar table_request_row_append(MethodContext const& context,
                                       ListOfListOfReals    list) {
    // qDebug() << Q_FUNC_INFO << QString::fromStdString(args.dump_string());

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    size_t insert_row = src->get_num_rows();


    std::vector<std::span<double>> ref_list;

    for (auto& v : list.lol) {
        ref_list.emplace_back(v);
    }

    int64_t ok = src->trigger_row_append(ref_list);

    if (ok) {
        auto sig = get_builtin(tbl.get(), BuiltinSignals::TABLE_SIG_ROWS_ADDED);

        issue_signal(tbl.get(), sig.get(), insert_row, ref_list.size());
    }

    return ok;
}


static int64_t table_request_row_remove(MethodContext const& context,
                                        Selection            selection) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    int64_t ok = src->trigger_deletion(selection);

    if (ok) {
        auto sig =
            get_builtin(tbl.get(), BuiltinSignals::TABLE_SIG_ROWS_DELETED);

        for (auto r : selection.rows) {
            issue_signal(tbl.get(), sig.get(), r, 1);
        }

        for (auto ra : selection.row_ranges) {
            issue_signal(tbl.get(), sig.get(), ra.first, ra.second - ra.second);
        }
    }

    return ok;
}

static AnyVar table_get_selection(MethodContext const& context,
                                  std::string          selection_id) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    auto sel_ref = src->get_selection(selection_id);

    return sel_ref.to_any();
}

static AnyVar table_get_selection_data(MethodContext const& context,
                                       std::string          selection_id) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    auto q = src->get_selection_data(selection_id);

    return clear_query(q);
}

static AnyVar table_set_selection(MethodContext const& context,
                                  Selection            selection,
                                  std::string          selection_id) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    SelectionRef ref;
    ref.rows       = selection.rows;
    ref.row_ranges = selection.row_ranges;

    int64_t ok = src->trigger_set_selection(selection_id, ref);

    if (ok) {
        auto sig =
            get_builtin(tbl.get(), BuiltinSignals::TABLE_SIG_SELECTION_CHANGED);

        issue_signal(tbl.get(), sig.get(), selection_id, ref.to_any());
    }


    return ok;
}

static AnyVar table_all_selections(MethodContext const& context) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    return src->get_all_selections();
}

static AnyVar table_get_block(MethodContext const& context,
                              int64_t              row_start,
                              int64_t              row_end,
                              IntListArg           list) {
    qDebug() << Q_FUNC_INFO;

    auto tbl = GET_TABLE(context);

    auto* src = tbl->get_source();

    int64_t max_row = src->get_num_rows();

    if (row_end < 0) row_end = max_row;

    if (row_start < 0 or row_start >= max_row) {
        throw MethodException(MethodException::CLIENT, "Row out of range!");
    }

    if (row_end < 0 or row_end >= max_row) {
        throw MethodException(MethodException::CLIENT, "Row out of range!");
    }

    qDebug() << "Rows" << row_start << row_end;

    for (auto const& cid : list.list) {
        size_t max_column = src->get_columns().size() - 1;

        if (cid >= max_column or cid < 0) {
            throw MethodException(MethodException::CLIENT,
                                  "Column id out of range");
        }
    }

    if (list.list.empty()) {
        list.list.resize(src->get_columns().size());
        std::iota(list.list.begin(), list.list.end(), 0);
    }

    qDebug() << "Cols" << QVector<size_t>(list.list.begin(), list.list.end());

    auto query =
        tbl->get_source()->get_block({ row_start, row_end }, list.list);

    std::vector<double> ret = clear_query(query);

    qDebug() << "Returning" << ret.size();

    return ret;
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

    StringOrIntArgument(AnyVar&& a) {
        if (a.has_int()) {
            emplace<int64_t>(a.to_int());
        } else if (std::holds_alternative<std::string>(a)) {
            emplace<std::string>(a.steal_string());
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
                                        std::string          arg) {

    auto obj = get_object(context);

    auto* cb = get_callbacks(obj);

    cb->set_current_option(arg);

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
        d.method_name          = "subscribe"sv;
        d.documentation        = "Subscribe to this table's signals"sv;
        d.return_documentation = "None"sv;
        d.code                 = table_subscribe;

        m_builtin_methods[BuiltinMethods::TABLE_SUBSCRIBE] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = "get_columns"sv;
        d.documentation        = "Get the header of this table."sv;
        d.return_documentation = "List of strings"sv;
        d.set_code(table_get_columns);

        m_builtin_methods[BuiltinMethods::TABLE_GET_COLUMNS] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name          = "get_num_rows"sv;
        d.documentation        = "Get the number of rows in this table."sv;
        d.return_documentation = "Integer"sv;
        d.set_code(table_get_num_rows);

        m_builtin_methods[BuiltinMethods::TABLE_GET_NUM_ROWS] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name = "get_row"sv;
        d.documentation =
            "Get the data in a given row, and given columns. Data is returned in the order of columns given."sv;
        d.argument_documentation = {
            { "row", "Integer row to query" },
            { "cols",
              "List of column indicies to query. An empty list implies all "
              "columns." },
        };
        d.return_documentation = "List of reals"sv;
        d.set_code(table_get_row);

        m_builtin_methods[BuiltinMethods::TABLE_GET_ROW] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name = "get_block"sv;
        d.documentation =
            "Get the data in a given row range, and given columns. Data is returned as concatenated columns."sv;
        d.argument_documentation = {
            { "row_from", "Starting integer row to query" },
            { "row_to",
              "Ending integer row to query (exclusive). Can be set to a "
              "negative value to indicate all rows." },
            { "col_list",
              "List of columns indicies to query. An empty list implies all "
              "columns." }
        };
        d.return_documentation = "List of reals"sv;
        d.set_code(table_get_block);

        m_builtin_methods[BuiltinMethods::TABLE_GET_BLOCK] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name = "insert_row"sv;
        d.documentation =
            "Request that given data be inserted into the table."sv;
        d.argument_documentation = {
            { "row",
              "Integer row slot to insert data. Following data is shifted "
              "down." },
            { "[real]", "List of reals to insert" },
        };
        d.return_documentation = "Integer, >0 if successful"sv;
        d.set_code(table_request_row_insert);

        m_builtin_methods[BuiltinMethods::TABLE_REQUEST_ROW_INSERT] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name   = "update_row"sv;
        d.documentation = "Request that a row be updated with given data."sv;
        d.argument_documentation = {
            { "row", "Integer row slot to update data." },
            { "[real]", "List of reals to insert" },
        };
        d.return_documentation = "Integer, >0 if successful"sv;
        d.set_code(table_request_row_update);

        m_builtin_methods[BuiltinMethods::TABLE_REQUEST_ROW_UPDATE] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name   = "append_rows"sv;
        d.documentation = "Request that the be appended with given data."sv;
        d.argument_documentation = {
            { "[ [real] ]",
              "List of rows (list of a list of reals) to insert" },
        };
        d.return_documentation = "Integer, >0 if successful"sv;
        d.set_code(table_request_row_append);

        m_builtin_methods[BuiltinMethods::TABLE_REQUEST_ROW_APPEND] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name            = "remove_rows"sv;
        d.documentation          = "Request that a selection be deleted"sv;
        d.argument_documentation = {
            { "selection", "A Selection to delete. This is a Selection Object" }
        };
        d.return_documentation = "Integer, >0 if successful"sv;
        d.set_code(table_request_row_remove);

        m_builtin_methods[BuiltinMethods::TABLE_REQUEST_DELETION] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name   = "get_selection"sv;
        d.documentation = "Get the table selection at a given index."sv;
        d.argument_documentation = { { "selection_id",
                                       "String selection ID" } };
        d.return_documentation   = "Selection object"sv;
        d.set_code(table_get_selection);

        m_builtin_methods[BuiltinMethods::TABLE_GET_SELECTION] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = "get_selection_data"sv;
        d.documentation = "Get the table data from a given selection ID."sv;
        d.argument_documentation = { { "selection_id",
                                       "String selection ID" } };
        d.return_documentation   = "Returns a row-concatenated list of reals"sv;
        d.set_code(table_get_selection_data);

        m_builtin_methods[BuiltinMethods::TABLE_GET_SELECTION_DATA] =
            create_method(this, d);
    }


    {
        MethodData d;
        d.method_name   = "set_selection"sv;
        d.documentation = "Set the table selection at a given index."sv;
        d.argument_documentation = { {
            "Selection object",
            "String selection ID",
        } };
        d.return_documentation   = "Integer >0 if successful"sv;
        d.set_code(table_set_selection);

        m_builtin_methods[BuiltinMethods::TABLE_REQUEST_SET_SELECTION] =
            create_method(this, d);
    }

    {
        MethodData d;
        d.method_name   = "get_all_selections"sv;
        d.documentation = "Get all selections on a table"sv;
        // d.argument_documentation ;
        d.return_documentation = "List of strings."sv;
        d.set_code(table_all_selections);

        m_builtin_methods[BuiltinMethods::TABLE_GET_ALL_SELECTIONS] =
            create_method(this, d);
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
        d.signal_name   = "table_reset"sv;
        d.documentation = "The table has been reset, and cleared."sv;

        m_builtin_signals[BuiltinSignals::TABLE_SIG_RESET] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = "table_row_added"sv;
        d.documentation    = "Rows have been inserted in the table"sv;
        std::string args[] = {
            "Starting row",
            "Count of rows added at that point",
        };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_ROWS_ADDED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = "table_row_deleted"sv;
        d.documentation    = "Rows have been deleted from the table"sv;
        std::string args[] = {
            "Starting row",
            "Count of rows removed from that point on. Rows are shifted up.",
        };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_ROWS_DELETED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = "table_row_updated"sv;
        d.documentation    = "Certain rows have changed in the table"sv;
        std::string args[] = {
            "Starting row",
            "Count of rows updated from at that point",
        };

        m_builtin_signals[BuiltinSignals::TABLE_SIG_DATA_UPDATED] =
            create_signal(this, d);
    }

    {
        SignalData d;
        d.signal_name      = "table_selection_updated"sv;
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
