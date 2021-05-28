#ifndef CLIENTSTATE_H
#define CLIENTSTATE_H

#include "client_interface.h"

#include <deque>

#include <QObject>
#include <QPointer>
#include <QWebSocket>

namespace nooc {

template <class Delegate, class IDType, class NewData>
class ComponentList {
    std::deque<std::shared_ptr<Delegate>> m_list;

    using Maker =
        std::function<std::shared_ptr<Delegate>(IDType, NewData const&)>;

    Maker const& m_maker;

public:
    ComponentList(Maker const& m) : m_maker(m) { }
    ~ComponentList() { qDebug() << Q_FUNC_INFO << typeid(Delegate).name(); }

    Delegate* handle_new(IDType at, NewData const& nd) {
        qDebug() << "Handle new" << at.to_qstring();
        if (!at.valid()) return nullptr;

        if (at.id_slot == m_list.size()) {
            // optimize to emplace_back
            auto& p = m_list.emplace_back(m_maker(at, std::move(nd)));
            return p.get();
        }

        if (at.id_slot > m_list.size()) {
            // this shouldnt really happen. ids are supposed to be sequential,
            // but ok. make the index valid and continue
            m_list.resize(at.id_slot + 1);
        }

        auto& slot = m_list[at.id_slot];

        if (slot) {
            if (slot->id() != at) {
                qDebug() << "Server attempted to update an entity that does "
                            "not exist!";
                return nullptr;
            }

            if constexpr (Delegate::CAN_UPDATE) {
                // this is an update
                slot->update(nd);
                emit slot->updated();
                return slot.get();
            } else {
                qDebug() << "Server attempted to update non-updatable entity";
                return nullptr;
            }
        }

        slot = m_maker(at, std::move(nd));

        return slot.get();
    }
    bool handle_delete(IDType at) {
        if (!at.valid()) {
            qDebug() << "Server asking to delete invalid id";
            return false;
        }

        if (at.id_slot >= m_list.size()) {
            qDebug() << "Server attempted to update non-extant id";
            return false;
        }

        auto& slot = m_list[at.id_slot];

        if (!slot) {
            qDebug() << "Server attempted to update non-extant id";
            return false;
        }

        if (slot->id() != at) {
            qDebug() << "Server attempted to update id with wrong generation";
            return false;
        }

        slot->prepare_delete();

        slot.reset();

        return true;
    }

    std::shared_ptr<Delegate> comp_at(IDType at) {
        if (at.id_slot >= m_list.size()) return nullptr;
        auto& slot = m_list[at.id_slot];

        if (slot->id() != at) return nullptr;
        return slot;
    }

    void clear() { m_list.clear(); }
};

// struct InflightMethod {
//    std::function<void(::noo::AnyVar)> on_done;

//    InflightMethod(std::function<void(::noo::AnyVar)>&& m)
//        : on_done(std::move(m)) { }
//};

class ClientState : public QObject {
    QWebSocket& m_socket;

    std::shared_ptr<DocumentDelegate> m_document;

    ComponentList<MethodDelegate, noo::MethodID, MethodData> m_method_list;
    ComponentList<SignalDelegate, noo::SignalID, SignalData> m_signal_list;

    ComponentList<BufferDelegate, noo::BufferID, BufferData> m_buffer_list;

    ComponentList<TableDelegate, noo::TableID, TableData>       m_table_list;
    ComponentList<TextureDelegate, noo::TextureID, TextureData> m_texture_list;
    ComponentList<LightDelegate, noo::LightID, LightData>       m_light_list;
    ComponentList<MaterialDelegate, noo::MaterialID, MaterialData>
                                                       m_material_list;
    ComponentList<MeshDelegate, noo::MeshID, MeshData> m_mesh_list;
    ComponentList<ObjectDelegate, noo::ObjectID, ObjectUpdateData>
        m_object_list;

    size_t m_last_invoke_id = 0;
    std::unordered_map<std::string, QPointer<PendingMethodReply>>
        m_in_flight_methods;

public:
    ClientState(QWebSocket& s, ClientDelegates&);
    ~ClientState();

    DocumentDelegate& document() { return *m_document; }

    auto& method_list() { return m_method_list; }
    auto& signal_list() { return m_signal_list; }
    auto& buffer_list() { return m_buffer_list; }
    auto& table_list() { return m_table_list; }
    auto& texture_list() { return m_texture_list; }
    auto& light_list() { return m_light_list; }
    auto& material_list() { return m_material_list; }
    auto& mesh_list() { return m_mesh_list; }
    auto& object_list() { return m_object_list; }

    auto& inflight_methods() { return m_in_flight_methods; }


    //    void invoke_method(MethodDelegatePtr const&,
    //                       MethodContext const&,
    //                       noo::AnyVarList&&,
    //                       std::function<void(::noo::AnyVar)>&&);

public slots:
    void on_new_binary_message(QByteArray m);
    void on_new_text_message(QString t);

    // Takes ownership of the reply!
    void on_method_ask_invoke(noo::MethodID,
                              MethodContext,
                              noo::AnyVarList const&,
                              PendingMethodReply*);
};

} // namespace nooc

#endif // CLIENTSTATE_H
