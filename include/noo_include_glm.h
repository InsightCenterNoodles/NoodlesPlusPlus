#pragma once

/// \file Normalize include of GLM with all desired options
#ifdef __clang__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-volatile"
#endif

#define GLM_ENABLE_EXPERIMENTAL

#define GLM_FORCE_SIZE_T_LENGTH
#define GLM_FORCE_SIZE_FUNC
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef __clang__
#    pragma GCC diagnostic pop
#endif
