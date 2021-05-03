#include "client_common.h"

#include "include/noo_anyref.h"

namespace nooc {

void SubscribeInitReply::interpret() {

    auto arg_vec = m_var.to_vector();

    if (arg_vec.size() < 4) {
        emit recv_fail("Bad subscription reply!");
        return;
    }

    auto names = arg_vec[0].to_vector();
    auto keys  = arg_vec[1];
    auto cols  = arg_vec[2].to_vector();
    auto sels  = arg_vec[2].to_vector();

    emit recv(names, keys, cols, sels);
}

} // namespace nooc
