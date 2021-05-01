#include "serialize.h"

#include "materiallist.h"
#include "meshlist.h"
#include "objectlist.h"
#include "tablelist.h"
#include "texturelist.h"

#include "bufferlist.h"
#include "materiallist.h"
#include "meshlist.h"
#include "methodlist.h"
#include "objectlist.h"
#include "tablelist.h"
#include "texturelist.h"

#include <flatbuffers/flatbuffers.h>

#include <QDebug>

namespace noo {

Writer::Writer() { }
Writer::~Writer() noexcept {
    if (!m_written) { qWarning() << "Message should have been written!"; }
}

flatbuffers::FlatBufferBuilder& Writer::builder() {
    return m_builder;
}

void Writer::finished_writing_and_export() {

    auto* ptr  = m_builder.GetBufferPointer();
    auto  size = m_builder.GetSize();

    QByteArray array(reinterpret_cast<char*>(ptr), size);

    emit data_ready(array);

    m_written = true;
}

// =============================


// void read_from(::noodles_interface::Any::Reader r, NoodlesAnyVar& var) {
//    switch (r.which()) {
//    case ::noodles_interface::Any::NOTHING: var = NoodlesAnyVar(); break;
//    case ::noodles_interface::Any::TEXT: var = r.getText(); break;
//    case ::noodles_interface::Any::INTEGER: var = r.getInteger(); break;
//    case ::noodles_interface::Any::REAL: var = r.getReal(); break;
//    case ::noodles_interface::Any::ID: // uh oh
//        var = std::monostate();
//        break;
//    case ::noodles_interface::Any::DATA: {
//        auto  reader = r.getData();
//        auto* start  = reinterpret_cast<std::byte const*>(reader.begin());
//        var.emplace<std::vector<std::byte>>(start, start + reader.size());
//    } break;
//    case ::noodles_interface::Any::LIST: {
//        auto& list = var.emplace<NoodlesAnyVarList>();

//        for (auto reader : r.getList()) {
//            auto& new_i = list.emplace_back();
//            read_from(reader, new_i);
//        }
//    } break;
//    case ::noodles_interface::Any::MAP: {
//        auto& map = var.emplace<NoodlesAnyVarMap>();

//        for (auto reader : r.getMap().getEntries()) {
//            std::string key = reader.getKey();
//            auto [iter, _]  = map.try_emplace(key);

//            read_from(reader.getValue(), iter->second);
//        }
//    } break;
//    case ::noodles_interface::Any::REAL_LIST: {
//        auto& list = var.emplace<std::vector<double>>();

//        auto from_list = r.getRealList();

//        list.resize(from_list.size());

//        std::copy(from_list.begin(), from_list.end(), list.begin());
//    } break;
//    }
//}

} // namespace noo
