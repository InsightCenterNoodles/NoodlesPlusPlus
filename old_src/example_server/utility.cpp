#include "utility.h"

std::pair<glm::vec3, glm::vec3> min_max_of(std::span<glm::vec3 const> v) {

    if (v.empty()) return { {}, {} };

    glm::vec3 lmin = v[0];
    glm::vec3 lmax = v[0];

    for (auto const& lv : v) {
        lmin = glm::min(lv, lmin);
        lmax = glm::max(lv, lmax);
    }

    return { lmin, lmax };
}
