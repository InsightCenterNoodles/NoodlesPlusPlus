#ifndef COMPONENTLISTBASE_H
#define COMPONENTLISTBASE_H

#include "serialize.h"

#include <QDebug>
#include <QObject>

#include <vector>

namespace noo {

class Writer;

class ServerT;

template <class Derived, class IDType, class T>
class ComponentListBase;

template <class Derived, class List, class _IDType>
class ComponentMixin : public QObject {
    Derived&       as_derived() { return *static_cast<Derived*>(this); }
    Derived const& as_derived() const {
        return *static_cast<Derived const*>(this);
    }

protected:
    _IDType m_id;
    List*   m_parent_list;

public:
    using IDType = _IDType;

    ComponentMixin(_IDType id, List* d) : m_id(id), m_parent_list(d) { }

    ~ComponentMixin() {
        auto w = m_parent_list->new_bcast();
        as_derived().write_delete_to(*w);

        m_parent_list->mark_free(m_id);
    }

    ComponentMixin(ComponentMixin const&) = delete;
    ComponentMixin& operator=(ComponentMixin const&) = delete;

    ComponentMixin(ComponentMixin&&) = delete;
    ComponentMixin& operator=(ComponentMixin&&) = delete;

    auto  id() const { return m_id; }
    List* hosting_list() const { return m_parent_list; }
};

struct ComponentListRock {
    ServerT* m_server;

    std::unique_ptr<Writer> new_bcast();

    ComponentListRock(ServerT*);

    ServerT* server() const { return m_server; }
};


template <class Derived, class IDType, class T>
class ComponentListBase : public ComponentListRock {
    Derived&       as_derived() { return *static_cast<Derived*>(this); }
    Derived const& as_derived() const {
        return *static_cast<Derived const*>(this);
    }

protected:
    using CompPtr = std::weak_ptr<T>;

    std::vector<CompPtr> m_list;
    std::vector<IDType>  m_free_list;

public:
    ComponentListBase(ServerT* s) : ComponentListRock(s) { }
    ~ComponentListBase() = default;

    template <class... Args>
    std::shared_ptr<T> provision_next(Args&&... args) {

        IDType place;

        if (!m_free_list.empty()) {
            place = m_free_list.back();
            m_free_list.pop_back();
        } else {
            size_t slot = m_list.size();

            place = IDType(slot, 0);

            m_list.emplace_back();
        }

        auto nptr = std::make_shared<T>(
            place, &as_derived(), std::forward<Args>(args)...);

        auto& slot = m_list[place.id_slot];

        Q_ASSERT(slot.expired());

        slot = nptr;

        on_create(*nptr);

        return nptr;
    }

    void mark_free(IDType id) {
        qDebug() << typeid(Derived).name() << "Marking free" << id.id_slot
                 << id.id_gen;
        auto& slot = m_list.at(id.id_slot);

        auto ptr = slot.lock();

        if (ptr) {
            // by the semantics of smart pointers, the ref counts will be bad by
            // the time we get here.
            // but just in case we DO have a lock...
            if (ptr->id() != id)
                throw std::runtime_error("Trying to free bad id!");
        }

        m_free_list.push_back(id);

        slot = {};
    }

    std::shared_ptr<T> get_at(IDType id) const {
        auto& slot = m_list.at(id.id_slot);

        auto ptr = slot.lock();

        if (!ptr) return nullptr;

        if (ptr->id() != id) return nullptr;

        return ptr;
    }

    void on_create(T& t) {
        auto w = new_bcast();
        t.write_new_to(*w);
    }

    template <class Function>
    void for_all(Function&& f) const {
        for (auto const& wptr : m_list) {
            auto ptr = wptr.lock();
            if (!ptr) continue;
            f(*ptr);
        }
    }
};

template <class Comp>
ServerT* server_from_component(Comp* c) {
    if (!c) return nullptr;
    return c->hosting_list()->server();
}

} // namespace noo

#endif // COMPONENTLISTBASE_H
