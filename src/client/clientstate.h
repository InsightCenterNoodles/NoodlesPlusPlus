#ifndef CLIENTSTATE_H
#define CLIENTSTATE_H

#include "include/noo_client_interface.h"

#include <deque>

#include <QObject>
#include <QPointer>
#include <QWebSocket>

namespace nooc {

template <class Delegate, class IDType, class NewData>
class ComponentList {
    std::deque<std::unique_ptr<Delegate>> m_list;

    using Maker =
        std::function<std::unique_ptr<Delegate>(IDType, NewData const&)>;

    Maker const& m_maker;

public:
    ComponentList(Maker const& m) : m_maker(m) { }
    ~ComponentList() { qDebug() << Q_FUNC_INFO << typeid(Delegate).name(); }

    template <class... PostArgs>
    void handle_new(IDType at, NewData const& nd, PostArgs&&... args) {
        qDebug() << "Handle new" << at.to_qstring();
        if (!at.valid()) return;

        if (at.id_slot == m_list.size()) {
            // optimize to emplace_back
            auto& p = m_list.emplace_back(m_maker(at, std::move(nd)));
            p->post_create(std::forward<PostArgs>(args)...);
            return;
        }

        if (at.id_slot > m_list.size()) {
            // this shouldnt really happen. ids are supposed to be sequential,
            // but ok. make the index valid and continue
            m_list.resize(at.id_slot + 1);
        }

        auto& slot = m_list[at.id_slot];

        if (slot) {
            qDebug()
                << "Server attempted to create an entity that already exists!";
            return;
        }

        slot = m_maker(at, std::move(nd));

        slot->post_create(std::forward<PostArgs>(args)...);
    }
    template <class UpdateData>
    bool handle_update(IDType at, UpdateData const& ud) {
        //        if constexpr (Delegate::CAN_UPDATE) {
        //            qWarning() << "Server attempted to update non-updatable
        //            object!"; return false;
        //        }

        if (!at.valid()) return false;
        if (at.id_slot >= m_list.size()) return false;

        auto& slot = m_list[at.id_slot];

        if (!slot) {
            qWarning()
                << "Server is trying to update an object that does not exist!";
            return false;
        }

        if (slot->id() != at) {
            qWarning()
                << "Server is trying to update an object that does not exist!";
            return false;
        }

        slot->update(ud);
        emit slot->updated();
        return true;
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

        slot.reset();

        return true;
    }

    Delegate* comp_at(IDType at) {
        if (at.id_slot >= m_list.size()) return nullptr;
        auto& slot = m_list[at.id_slot];

        if (slot->id() != at) return nullptr;
        return slot.get();
    }

    void clear() { m_list.clear(); }
};


class InternalClientState : public QObject {
    Q_OBJECT

    QWebSocket& m_socket;

    std::shared_ptr<DocumentDelegate> m_document;

    ComponentList<MethodDelegate, noo::MethodID, MethodInit> m_method_list;
    ComponentList<SignalDelegate, noo::SignalID, SignalInit> m_signal_list;

    ComponentList<BufferDelegate, noo::BufferID, BufferInit> m_buffer_list;
    ComponentList<BufferViewDelegate, noo::BufferViewID, BufferViewInit>
        m_buffer_view_list;

    ComponentList<TableDelegate, noo::TableID, TableInit>       m_table_list;
    ComponentList<TextureDelegate, noo::TextureID, TextureInit> m_texture_list;
    ComponentList<LightDelegate, noo::LightID, LightInit>       m_light_list;
    ComponentList<MaterialDelegate, noo::MaterialID, MaterialInit>
                                                                m_material_list;
    ComponentList<MeshDelegate, noo::GeometryID, MeshInit>      m_mesh_list;
    ComponentList<EntityDelegate, noo::EntityID, EntityInit>    m_object_list;
    ComponentList<PlotDelegate, noo::PlotID, PlotInit>          m_plot_list;
    ComponentList<SamplerDelegate, noo::SamplerID, SamplerInit> m_sampler_list;
    ComponentList<ImageDelegate, noo::ImageID, ImageInit>       m_image_list;

    size_t                                       m_last_invoke_id = 0;
    QHash<QString, QPointer<PendingMethodReply>> m_in_flight_methods;

    QNetworkAccessManager* m_network_manager;

public:
    InternalClientState(QWebSocket& s, ClientDelegates&);
    ~InternalClientState();

    DocumentDelegate& document() { return *m_document; }

    auto& method_list() { return m_method_list; }
    auto& signal_list() { return m_signal_list; }
    auto& buffer_list() { return m_buffer_list; }
    auto& buffer_view_list() { return m_buffer_view_list; }
    auto& table_list() { return m_table_list; }
    auto& texture_list() { return m_texture_list; }
    auto& light_list() { return m_light_list; }
    auto& material_list() { return m_material_list; }
    auto& mesh_list() { return m_mesh_list; }
    auto& object_list() { return m_object_list; }
    auto& plot_list() { return m_plot_list; }
    auto& sampler_list() { return m_sampler_list; }
    auto& image_list() { return m_image_list; }

    auto& inflight_methods() { return m_in_flight_methods; }

    MethodDelegate* lookup(noo::MethodID id) {
        return m_method_list.comp_at(id);
    }
    SignalDelegate* lookup(noo::SignalID id) {
        return m_signal_list.comp_at(id);
    }
    BufferDelegate* lookup(noo::BufferID id) {
        return m_buffer_list.comp_at(id);
    }
    BufferViewDelegate* lookup(noo::BufferViewID id) {
        return m_buffer_view_list.comp_at(id);
    }
    TableDelegate* lookup(noo::TableID id) { return m_table_list.comp_at(id); }
    PlotDelegate*  lookup(noo::PlotID id) { return m_plot_list.comp_at(id); }
    TextureDelegate* lookup(noo::TextureID id) {
        return m_texture_list.comp_at(id);
    }
    LightDelegate* lookup(noo::LightID id) { return m_light_list.comp_at(id); }
    MaterialDelegate* lookup(noo::MaterialID id) {
        return m_material_list.comp_at(id);
    }
    MeshDelegate* lookup(noo::GeometryID id) { return m_mesh_list.comp_at(id); }
    EntityDelegate* lookup(noo::EntityID id) {
        return m_object_list.comp_at(id);
    }
    SamplerDelegate* lookup(noo::SamplerID id) {
        return m_sampler_list.comp_at(id);
    }
    ImageDelegate* lookup(noo::ImageID id) { return m_image_list.comp_at(id); }

    QNetworkAccessManager* network_manager() { return m_network_manager; }

    void clear();

signals:
    void server_done_init();

public slots:
    void on_new_binary_message(QByteArray m);
    void on_new_text_message(QString t);

    // Takes ownership of the reply!
    void on_method_ask_invoke(noo::MethodID,
                              nooc::MethodContextPtr,
                              QCborArray const&,
                              nooc::PendingMethodReply*);
};

} // namespace nooc

#endif // CLIENTSTATE_H
