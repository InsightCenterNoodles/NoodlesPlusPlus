#include "componentlistbase.h"

#include "noodlesserver.h"
#include "src/common/serialize.h"

namespace noo {

ComponentListRock::ComponentListRock(ServerT* s) : m_server(s) { }

std::unique_ptr<SMsgWriter> ComponentListRock::new_bcast() {
    return m_server->get_broadcast_writer();
}

} // namespace noo
