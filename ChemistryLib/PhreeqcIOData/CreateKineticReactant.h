/**
 * \copyright
 * Copyright (c) 2012-2019, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#pragma once

#include <boost/optional/optional.hpp>
#include <vector>

namespace BaseLib
{
class ConfigTree;
}

namespace ChemistryLib
{
struct KineticReactant;
}

namespace ChemistryLib
{
std::vector<KineticReactant> createKineticReactants(
    boost::optional<BaseLib::ConfigTree> const& config);
}  // namespace ChemistryLib
