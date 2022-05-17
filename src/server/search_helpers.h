#ifndef SEARCH_HELPERS_H
#define SEARCH_HELPERS_H

#include "methodlist.h"

#include <unordered_set>

namespace noo {

class AttachedMethodList {
    // because we dont have heterogeneous lookup yet
    std::unordered_set<MethodTPtr> m_sptrs;
    std::unordered_set<MethodT*>   m_ptrs;

public:
    AttachedMethodList() = default;

    template <class Iter>
    AttachedMethodList(Iter first, Iter last) : m_sptrs(first, last) {
        for (auto iter = first; iter != last; iter++) {
            m_ptrs.insert(iter->get());
        }
    }

    void insert(MethodTPtr const& p) {
        assert(p);
        m_sptrs.insert(p);
        m_ptrs.insert(p.get());
    }

    MethodT* find(MethodID id) const {
        // todo FIX THIS
        for (auto* ptr : m_ptrs) {
            if (ptr->id() == id) return ptr;
        }
        return nullptr;
    }

    template <class T>
    AttachedMethodList& operator=(QVector<T> const& v) {
        m_sptrs.clear();
        m_ptrs.clear();
        for (auto const& t : v) {
            insert(t);
        }
        return *this;
    }

    auto   begin() const { return m_sptrs.begin(); }
    auto   end() const { return m_sptrs.end(); }
    size_t size() const { return m_sptrs.size(); }
};


class AttachedSignalList {
    // because we dont have heterogeneous lookup yet
    std::unordered_set<SignalTPtr> m_sptrs;
    std::unordered_set<SignalT*>   m_ptrs;

public:
    AttachedSignalList() = default;

    template <class Iter>
    AttachedSignalList(Iter first, Iter last) : m_sptrs(first, last) {
        for (auto iter = first; iter != last; iter++) {
            m_ptrs.insert(iter->get());
        }
    }

    void insert(SignalTPtr const& p) {
        assert(p);
        m_sptrs.insert(p);
        m_ptrs.insert(p.get());
    }

    bool has(SignalT* ptr) const { return m_ptrs.count(ptr); }

    SignalT* find_by_name(QString const& name) const {
        for (SignalT* p : m_ptrs) {
            if (p->name() == name) return p;
        }
        return nullptr;
    }

    template <class T>
    AttachedSignalList& operator=(QVector<T> const& v) {
        m_sptrs.clear();
        m_ptrs.clear();
        for (auto const& t : v) {
            insert(t);
        }
        return *this;
    }

    auto   begin() const { return m_sptrs.begin(); }
    auto   end() const { return m_sptrs.end(); }
    size_t size() const { return m_sptrs.size(); }
};

} // namespace noo

#endif // SEARCH_HELPERS_H
