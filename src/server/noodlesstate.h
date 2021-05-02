#ifndef NOODLESSTATE_H
#define NOODLESSTATE_H

#include "bufferlist.h"
#include "include/noo_server_interface.h"
#include "materiallist.h"
#include "meshlist.h"
#include "methodlist.h"
#include "objectlist.h"
#include "search_helpers.h"
#include "tablelist.h"
#include "texturelist.h"

#include <QObject>

#include <memory>
#include <unordered_map>

namespace noo {

enum class BuiltinMethods {
    TABLE_SUBSCRIBE,
    TABLE_GET_COLUMNS,
    TABLE_GET_NUM_ROWS,
    TABLE_GET_ROW,
    TABLE_GET_BLOCK,
    TABLE_REQUEST_ROW_INSERT,
    TABLE_REQUEST_ROW_UPDATE,
    TABLE_REQUEST_ROW_APPEND,
    TABLE_REQUEST_DELETION,
    TABLE_GET_SELECTION,
    TABLE_GET_SELECTION_DATA,
    TABLE_REQUEST_SET_SELECTION,
    TABLE_GET_ALL_SELECTIONS,

    OBJ_ACTIVATE,
    OBJ_GET_ACTIVATE_CHOICES,

    OBJ_GET_OPTS,
    OBJ_GET_CURR_OPT,
    OBJ_SET_CURR_OPT,

    OBJ_SET_POS,
    OBJ_SET_ROT,
    OBJ_SET_SCALE,

    OBJ_SEL_REGION,
    OBJ_SEL_SPHERE,
    OBJ_SEL_PLANE,

    OBJ_PROBE,
};

enum class BuiltinSignals {
    TABLE_SIG_RESET,
    TABLE_SIG_ROWS_ADDED,
    TABLE_SIG_ROWS_DELETED,
    TABLE_SIG_DATA_UPDATED,
    TABLE_SIG_SELECTION_CHANGED,

    OBJ_SIG_ATT,
};

class ServerT;

class DocumentT {
    ServerT* m_server;

    MethodList m_method_list;
    SignalList m_signal_list;

    BufferList m_buffer_list;
    LightList  m_light_list;

    MaterialList m_mat_list;
    MeshList     m_mesh_list;
    ObjectList   m_obj_list;
    TextureList  m_tex_list;
    TableList    m_table_list;


    std::vector<MethodTPtr> m_doc_method_list;
    std::vector<SignalTPtr> m_doc_signal_list;


    AttachedMethodList m_att_method_list_search;
    AttachedSignalList m_att_signal_list_search;

    std::unordered_map<BuiltinMethods, MethodTPtr> m_builtin_methods;
    std::unordered_map<BuiltinSignals, SignalTPtr> m_builtin_signals;

    void build_table_methods();
    void build_table_signals();

public:
    DocumentT(ServerT*);

    MethodList&   method_list();
    SignalList&   signal_list();
    BufferList&   buffer_list();
    LightList&    light_list();
    MaterialList& mat_list();
    MeshList&     mesh_list();
    ObjectList&   obj_list();
    TextureList&  tex_list();
    TableList&    table_list();


    AttachedMethodList& att_method_list();
    AttachedSignalList& att_signal_list();

    void update(DocumentData const&, Writer&);
    void update(DocumentData const&);

    void write_refresh(Writer&);

    // ==

    MethodTPtr get_builtin(BuiltinMethods);
    SignalTPtr get_builtin(BuiltinSignals);

    void build_table_builtins();
};

class NoodlesState : public QObject {
    Q_OBJECT

    ServerT* m_parent;

    std::shared_ptr<DocumentT> m_document;

public:
    NoodlesState(ServerT* parent);

    std::shared_ptr<DocumentT> const& document();
};

} // namespace noo

#endif // NOODLESSTATE_H
