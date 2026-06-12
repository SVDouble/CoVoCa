#pragma once

#include <rfl/Validator.hpp>
#include <rfl/comparisons.hpp>

namespace covoca::config {

using PositiveInt = rfl::Validator<int, rfl::ExclusiveMinimum<0>>;
using PositiveDouble = rfl::Validator<double, rfl::ExclusiveMinimum<0>>;
using NonNegativeDouble = rfl::Validator<double, rfl::Minimum<0>>;

} // namespace covoca::config
