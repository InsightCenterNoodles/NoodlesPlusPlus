#ifndef NOODLESSTATE_H
#define NOODLESSTATE_H

#include "bufferlist.h"
#include "include/noo_server_interface.h"
#include "materiallist.h"
#include "meshlist.h"
#include "methodlist.h"
#include "objectlist.h"
#include "plotlist.h"
#include "search_helpers.h"
#include "src/server/assetstorage.h"
#include "tablelist.h"
#include "texturelist.h"

#include <QObject>

#include <memory>
#include <unordered_map>

namespace noo {

enum class BuiltinMethods {
    TABLE_SUBSCRIBE,
    TABLE_INSERT,
    TABLE_UPDATE,
    TABLE_REMOVE,
    TABLE_CLEAR,
    TABLE_UPDATE_SELECTION,

    OBJ_ACTIVATE,
    OBJ_GET_ACTIVATE_CHOICES,

    OBJ_GET_KEYS,
    OBJ_VAR_OPTS,
    OBJ_GET_VAR,
    OBJ_SET_VAR,

    OBJ_SET_POS,
    OBJ_SET_ROT,
    OBJ_SET_SCALE,

    OBJ_SEL_REGION,
    OBJ_SEL_SPHERE,
    OBJ_SEL_PLANE,
    OBJ_SEL_HULL,

    OBJ_PROBE,
};

enum class BuiltinSignals {
    TABLE_SIG_RESET,
    TABLE_SIG_ROWS_DELETED,
    TABLE_SIG_DATA_UPDATED,
    TABLE_SIG_SELECTION_CHANGED,

    OBJ_SIG_ATT,
};

class ServerT;

class DocumentT : public QObject {
    Q_OBJECT

    ServerT* m_server;

    AssetStorage* m_storage;

    MethodList m_method_list;
    SignalList m_signal_list;

    BufferList     m_buffer_list;
    BufferViewList m_buffer_view_list;
    ImageList      m_image_list;
    LightList      m_light_list;

    MaterialList m_mat_list;
    MeshList     m_mesh_list;
    ObjectList   m_obj_list;
    SamplerList  m_sampler_list;
    TextureList  m_tex_list;
    TableList    m_table_list;
    PlotList     m_plot_list;


    QVector<MethodTPtr> m_doc_method_list;
    QVector<SignalTPtr> m_doc_signal_list;


    AttachedMethodList m_att_method_list_search;
    AttachedSignalList m_att_signal_list_search;

    std::unordered_map<BuiltinMethods, MethodTPtr> m_builtin_methods;
    std::unordered_map<BuiltinSignals, SignalTPtr> m_builtin_signals;

    void build_table_methods();
    void build_table_signals();

public:
    DocumentT(ServerT*, ServerOptions const& options);

    AssetStorage& storage();

    MethodList&     method_list();
    SignalList&     signal_list();
    BufferList&     buffer_list();
    BufferViewList& buffer_view_list();
    ImageList&      image_list();
    SamplerList&    sampler_list();
    LightList&      light_list();
    MaterialList&   mat_list();
    MeshList&       mesh_list();
    ObjectList&     obj_list();
    TextureList&    tex_list();
    PlotList&       plot_list();
    TableList&      table_list();


    AttachedMethodList& att_method_list();
    AttachedSignalList& att_signal_list();

    void update(DocumentData const&, SMsgWriter&);
    void update(DocumentData const&);

    void write_refresh(SMsgWriter&);

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
    NoodlesState(ServerT* parent, ServerOptions const& options);

    std::shared_ptr<DocumentT> const& document();
};

} // namespace noo

#endif // NOODLESSTATE_H
