#pragma once

#include "cad_core/base/placement.h"

#include <array>

namespace cad_core::app {

struct Placement {
    std::array<double, 3> base;
    std::array<double, 4> rotation;
};

}  // namespace cad_core::app
