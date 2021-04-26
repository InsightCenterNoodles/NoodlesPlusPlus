#include "componentlistbase.h"

#include "noodlesserver.h"
#include "serialize.h"

namespace noo {

ComponentListRock::ComponentListRock(ServerT* s) : m_server(s) { }

std::unique_ptr<Writer> ComponentListRock::new_bcast() {
    return m_server->get_broadcast_writer();
}

} // namespace noo
