#include "methodlist.h"


#include "../shared/noodles_server_generated.h"
#include "../shared/variant_tools.h"
#include "noodlesserver.h"
#include "noodlesstate.h"
#include "serialize.h"

#include <QDebug>

namespace noo {

MethodList::MethodList(ServerT* s) : ComponentListBase(s) { }
MethodList::~MethodList() = default;

MethodT::MethodT(IDType id, MethodList* host, MethodData const& d)
    : ComponentMixin(id, host), m_data(d) {

    qDebug() << "NEW METHOD" << id.id_slot << id.id_gen
             << m_data.method_name.c_str();
}

auto write_to(Arg const& ma, flatbuffers::FlatBufferBuilder& b) {
    return noodles::CreateMethodArgDirect(b, ma.name.c_str(), ma.doc.c_str());
}

void MethodT::write_new_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto doc_array = write_array_to(m_data.argument_documentation, w);

    auto x = noodles::CreateMethodCreate(
        w,
        lid,
        w->CreateString(m_data.method_name),
        w->CreateString(m_data.documentation),
        w->CreateString(m_data.return_documentation),
        doc_array);

    w.complete_message(x);
}

template <class Builder>
auto write_to(MethodID const& id, Builder& b) {
    return b->template CreateStruct<MethodID>(convert_id(id, b));
}

void MethodT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);
    auto x   = noodles::CreateMethodDelete(w, lid);
    w.complete_message(x);
}

// void write_to(MethodTPtr const& ptr, ::noodles_interface::MethodID::Builder
// b) {
//    write_ptr_id_to(ptr, b);
//}


// ===========================

SignalList::SignalList(ServerT* s) : ComponentListBase(s) { }
SignalList::~SignalList() = default;

SignalT::SignalT(IDType id, SignalList* host, SignalData const& d)
    : ComponentMixin(id, host), m_data(d) { }

void SignalT::write_new_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateSignalCreate(
        w,
        lid,
        w->CreateString(m_data.signal_name),
        w->CreateString(m_data.documentation),
        write_array_to(m_data.argument_documentation, w));

    w.complete_message(x);
}

void SignalT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);
    auto x   = noodles::CreateSignalDelete(w, lid);
    w.complete_message(x);
}


void SignalT::fire(std::variant<std::monostate, TableID, ObjectID> context,
                   AnyVarList&&                                    v) {

    std::unique_ptr<Writer> w = [&]() {
        if (std::holds_alternative<TableID>(context)) {
            try {
                TableTPtr ptr = m_parent_list->server()
                                    ->state()
                                    ->document()
                                    ->table_list()
                                    .get_at(std::get<TableID>(context));

                return m_parent_list->server()->get_table_subscribers_writer(
                    *ptr);
            } catch (...) { return std::unique_ptr<Writer>(); }

        } else {
            return m_parent_list->new_bcast();
        }
    }();


    auto noodles_id = convert_id(id(), *w);

    auto var = write_to(std::move(v), *w);


    auto x = VMATCH(
        context,
        VCASE(std::monostate) {
            return noodles::CreateSignalInvoke(*w, noodles_id, {}, {}, var);
        },
        VCASE(TableID tid) {
            auto noodles_tbl_id = convert_id(tid, *w);
            return CreateSignalInvoke(*w, noodles_id, {}, noodles_tbl_id, var);
        },
        VCASE(ObjectID oid) {
            auto noodles_obj_id = convert_id(oid, *w);
            return CreateSignalInvoke(*w, noodles_id, noodles_obj_id, {}, var);
        });

    w->complete_message(x);
}

// void write_to(SignalTPtr const& ptr, ::noodles_interface::SignalID::Builder
// b) {
//    write_ptr_id_to(ptr, b);
//}

} // namespace noo
