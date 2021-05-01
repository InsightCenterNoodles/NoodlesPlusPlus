#include "noo_common.h"

#include <QString>

namespace noo {

QString to_qstring(std::string_view s) {
    return QString::fromLocal8Bit(s.data(), s.size());
}

} // namespace noo
