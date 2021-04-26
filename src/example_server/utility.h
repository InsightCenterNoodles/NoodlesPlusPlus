#ifndef UTILITY_H
#define UTILITY_H


#define GLM_ENABLE_EXPERIMENTAL

#define GLM_FORCE_SIZE_T_LENGTH
#define GLM_FORCE_SIZE_FUNC
#include <glm/glm.hpp>

#include <span>

std::pair<glm::vec3, glm::vec3> min_max_of(std::span<glm::vec3 const>);


#endif // UTILITY_H
