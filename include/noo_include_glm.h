#ifndef INCLUDE_GLM_H
#define INCLUDE_GLM_H

/// \file Normalize include of GLM with all desired options

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-volatile"

#define GLM_ENABLE_EXPERIMENTAL

#define GLM_FORCE_SIZE_T_LENGTH
#define GLM_FORCE_SIZE_FUNC
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#pragma GCC diagnostic pop

#endif // INCLUDE_GLM_H
