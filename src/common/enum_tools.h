#pragma once

#include <magic_enum.hpp>

#include <QString>

namespace noo {

template <class E>
QString enum_name(E e) {
    auto v = magic_enum::enum_name(e);
    return QString::fromUtf8(v.data(), v.size());
}

}
