#include "methodlist.h"


#include "noodlesserver.h"
#include "noodlesstate.h"
#include "src/common/serialize.h"
#include "src/common/variant_tools.h"


#include <QDebug>

namespace noo {

MethodList::MethodList(ServerT* s) : ComponentListBase(s) { }
MethodList::~MethodList() = default;

MethodT::MethodT(IDType id, MethodList* host, MethodData const& d)
    : ComponentMixin(id, host), m_data(d) {

    qDebug() << "NEW METHOD" << id.id_slot << id.id_gen << m_data.method_name;
}

void MethodT::write_new_to(SMsgWriter& w) {
    messages::MsgMethodCreate m;
    m.id         = id();
    m.name       = m_data.method_name;
    m.doc        = opt_string(m_data.documentation);
    m.return_doc = opt_string(m_data.return_documentation);

    for (auto const& a : m_data.argument_documentation) {
        m.arg_doc << messages::MethodArg { .name        = a.name,
                                           .doc         = a.documentation,
                                           .editor_hint = a.editor_hint };
    }

    w.add(m);
}

template <class Builder>
auto write_to(MethodID const& id, Builder& b) {
    return b->template CreateStruct<MethodID>(convert_id(id, b));
}

void MethodT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgMethodDelete { .id = id() });
}


// ===========================

SignalList::SignalList(ServerT* s) : ComponentListBase(s) { }
SignalList::~SignalList() = default;

SignalT::SignalT(IDType id, SignalList* host, SignalData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void SignalT::write_new_to(SMsgWriter& w) {
    messages::MsgSignalCreate m;
    m.id   = id();
    m.name = m_data.signal_name;
    m.doc  = m_data.documentation;

    for (auto& a : m_data.argument_documentation) {
        m.arg_doc << messages::MethodArg { .name        = a.name,
                                           .doc         = a.documentation,
                                           .editor_hint = a.editor_hint };
    }

    w.add(m);
}

void SignalT::write_delete_to(SMsgWriter& w) {
    w.add(messages::MsgSignalDelete { .id = id() });
}


void SignalT::fire(InvokeID context, QCborArray&& v) {

    std::unique_ptr<SMsgWriter> w = [&]() {
        if (std::holds_alternative<TableID>(context)) {
            try {
                TableTPtr ptr = m_parent_list->server()
                                    ->state()
                                    ->document()
                                    ->table_list()
                                    .get_at(std::get<TableID>(context));

                return m_parent_list->server()->get_table_subscribers_writer(
                    *ptr);
            } catch (...) { return std::unique_ptr<SMsgWriter>(); }

        } else {
            return m_parent_list->new_bcast();
        }
    }();

    messages::MsgSignalInvoke m;
    m.id          = id();
    m.context     = context;
    m.signal_data = v;

    w->add(m);
}

} // namespace noo
