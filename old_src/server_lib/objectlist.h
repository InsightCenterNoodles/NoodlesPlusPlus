#ifndef OBJECTLIST_H
#define OBJECTLIST_H

#include "../shared/id.h"
#include "componentlistbase.h"
#include "search_helpers.h"
#include "server_interface.h"

#include <unordered_set>

namespace noo {

class ObjectList : public ComponentListBase<ObjectList, ObjectID, ObjectT> {
public:
    ObjectList(ServerT*);
    ~ObjectList();
};

class ObjectTUpdateHelper;

class ObjectT : public ComponentMixin<ObjectT, ObjectList, ObjectID> {
    ObjectData m_data;

    AttachedMethodList m_method_search;
    AttachedSignalList m_signal_search;

    std::unique_ptr<ObjectCallbacks> m_callback;

    void update_common(ObjectTUpdateHelper const&, Writer&);

public:
    ObjectT(IDType, ObjectList*, ObjectData const&);

    void write_new_to(Writer&);
    void update(ObjectUpdateData&, Writer&);
    void update(ObjectUpdateData&);
    void write_delete_to(Writer&);


    AttachedMethodList& att_method_list();
    AttachedSignalList& att_signal_list();

    ObjectCallbacks* callbacks() const;


private slots:
    void on_signal_attention_plain();
    void on_signal_attention_at(glm::vec3);
    void on_signal_attention_anno(glm::vec3, std::string);

signals:
};

} // namespace noo

#endif // OBJECTLIST_H
