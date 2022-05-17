#ifndef OBJECTLIST_H
#define OBJECTLIST_H

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_include_glm.h"
#include "include/noo_server_interface.h"
#include "search_helpers.h"

#include <unordered_set>

namespace noo {

class ObjectList : public ComponentListBase<ObjectList, EntityID, ObjectT> {
public:
    ObjectList(ServerT*);
    ~ObjectList();
};

class ObjectT : public ComponentMixin<ObjectT, ObjectList, EntityID> {
    ObjectData m_data;

    AttachedMethodList m_method_search;
    AttachedSignalList m_signal_search;

    std::unique_ptr<EntityCallbacks> m_callback;

public:
    ObjectT(IDType, ObjectList*, ObjectData const&);

    void write_new_to(SMsgWriter&);
    void update(ObjectUpdateData&, SMsgWriter&);
    void update(ObjectUpdateData&);
    void write_delete_to(SMsgWriter&);


    AttachedMethodList& att_method_list();
    AttachedSignalList& att_signal_list();

    EntityCallbacks* callbacks() const;


private slots:
    void on_signal_attention_plain();
    void on_signal_attention_at(glm::vec3);
    void on_signal_attention_anno(glm::vec3, QString);

signals:
};

} // namespace noo

#endif // OBJECTLIST_H
