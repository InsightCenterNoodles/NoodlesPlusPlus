#ifndef METHODLIST_H
#define METHODLIST_H

#include "componentlistbase.h"
#include "include/noo_id.h"
#include "include/noo_interface_types.h"
#include "include/noo_server_interface.h"
#include "src/generated/interface_tools.h"

namespace noo {

class MethodList : public ComponentListBase<MethodList, MethodID, MethodT> {
public:
    MethodList(ServerT*);
    ~MethodList();
};


class MethodT : public ComponentMixin<MethodT, MethodList, MethodID> {
    MethodData m_data;

public:
    MethodT(IDType, MethodList*, MethodData const&);

    auto const& data() const { return m_data; }

    void write_new_to(SMsgWriter&);
    void write_delete_to(SMsgWriter&);

    auto const& function() const { return m_data.code; }
};

// void write_to(MethodTPtr const&, ::noodles_interface::MethodID::Builder);

// =======================================


class SignalList : public ComponentListBase<SignalList, SignalID, SignalT> {
public:
    SignalList(ServerT*);
    ~SignalList();
};


class SignalT : public ComponentMixin<SignalT, SignalList, SignalID> {
    SignalData m_data;

public:
    SignalT(IDType, SignalList*, SignalData const&);

    QString const& name() const { return m_data.signal_name; }
    auto const&    data() const { return m_data; }

    void write_new_to(SMsgWriter&);
    void write_delete_to(SMsgWriter&);


    void fire(InvokeID id, QCborArray&&);
};

// void write_to(NoodlesSignalTPtr const&,
// ::noodles_interface::SignalID::Builder);

} // namespace noo

#endif // METHODLIST_H
