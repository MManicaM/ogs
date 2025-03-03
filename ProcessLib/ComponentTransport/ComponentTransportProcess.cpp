/**
 * \copyright
 * Copyright (c) 2012-2019, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#include "ComponentTransportProcess.h"

#include <cassert>

#include "ProcessLib/SurfaceFlux/SurfaceFlux.h"
#include "ProcessLib/SurfaceFlux/SurfaceFluxData.h"
#include "ProcessLib/Utils/CreateLocalAssemblers.h"

namespace ProcessLib
{
namespace ComponentTransport
{
ComponentTransportProcess::ComponentTransportProcess(
    std::string name,
    MeshLib::Mesh& mesh,
    std::unique_ptr<ProcessLib::AbstractJacobianAssembler>&& jacobian_assembler,
    std::vector<std::unique_ptr<ParameterLib::ParameterBase>> const& parameters,
    unsigned const integration_order,
    std::vector<std::vector<std::reference_wrapper<ProcessVariable>>>&&
        process_variables,
    ComponentTransportProcessData&& process_data,
    SecondaryVariableCollection&& secondary_variables,
    NumLib::NamedFunctionCaller&& named_function_caller,
    bool const use_monolithic_scheme,
    std::unique_ptr<ProcessLib::SurfaceFluxData>&& surfaceflux,
    std::vector<std::pair<int, std::string>>&& process_id_to_component_name_map)
    : Process(std::move(name), mesh, std::move(jacobian_assembler), parameters,
              integration_order, std::move(process_variables),
              std::move(secondary_variables), std::move(named_function_caller),
              use_monolithic_scheme),
      _process_data(std::move(process_data)),
      _surfaceflux(std::move(surfaceflux)),
      _process_id_to_component_name_map(
          std::move(process_id_to_component_name_map))
{
}

void ComponentTransportProcess::initializeConcreteProcess(
    NumLib::LocalToGlobalIndexMap const& dof_table,
    MeshLib::Mesh const& mesh,
    unsigned const integration_order)
{
    const int process_id = 0;
    ProcessLib::ProcessVariable const& pv = getProcessVariables(process_id)[0];

    std::vector<std::reference_wrapper<ProcessLib::ProcessVariable>>
        transport_process_variables;
    if (_use_monolithic_scheme)
    {
        for (auto pv_iter = std::next(_process_variables[process_id].begin());
             pv_iter != _process_variables[process_id].end();
             ++pv_iter)
            transport_process_variables.push_back(*pv_iter);
    }
    else
    {
        for (auto pv_iter = std::next(_process_variables.begin());
             pv_iter != _process_variables.end();
             ++pv_iter)
            transport_process_variables.push_back((*pv_iter)[0]);

        _xs_previous_timestep.resize(_process_variables.size());
    }

    ProcessLib::createLocalAssemblers<LocalAssemblerData>(
        mesh.getDimension(), mesh.getElements(), dof_table,
        pv.getShapeFunctionOrder(), _local_assemblers,
        mesh.isAxiallySymmetric(), integration_order, _process_data,
        transport_process_variables);

    _secondary_variables.addSecondaryVariable(
        "darcy_velocity",
        makeExtrapolator(
            mesh.getDimension(), getExtrapolator(), _local_assemblers,
            &ComponentTransportLocalAssemblerInterface::getIntPtDarcyVelocity));
}

void ComponentTransportProcess::assembleConcreteProcess(
    const double t,
    GlobalVector const& x,
    GlobalMatrix& M,
    GlobalMatrix& K,
    GlobalVector& b)
{
    DBUG("Assemble ComponentTransportProcess.");

    const int process_id = 0;
    ProcessLib::ProcessVariable const& pv = getProcessVariables(process_id)[0];

    std::vector<std::reference_wrapper<NumLib::LocalToGlobalIndexMap>>
        dof_tables;
    if (_use_monolithic_scheme)
    {
        dof_tables.push_back(std::ref(*_local_to_global_index_map));
    }
    else
    {
        setCoupledSolutionsOfPreviousTimeStep();
        std::generate_n(
            std::back_inserter(dof_tables), _process_variables.size(),
            [&]() { return std::ref(*_local_to_global_index_map); });
    }
    // Call global assembler for each local assembly item.
    GlobalExecutor::executeSelectedMemberDereferenced(
        _global_assembler, &VectorMatrixAssembler::assemble, _local_assemblers,
        pv.getActiveElementIDs(), dof_tables, t, x, M, K, b,
        _coupled_solutions);
}

void ComponentTransportProcess::setCoupledSolutionsOfPreviousTimeStep()
{
    unsigned const number_of_coupled_solutions =
        _coupled_solutions->coupled_xs.size();
    _coupled_solutions->coupled_xs_t0.clear();
    _coupled_solutions->coupled_xs_t0.reserve(number_of_coupled_solutions);
    for (unsigned i = 0; i < number_of_coupled_solutions; ++i)
    {
        auto const& x_t0 = _xs_previous_timestep[i];
        _coupled_solutions->coupled_xs_t0.emplace_back(x_t0.get());
    }
}

void ComponentTransportProcess::assembleWithJacobianConcreteProcess(
    const double t, GlobalVector const& x, GlobalVector const& xdot,
    const double dxdot_dx, const double dx_dx, GlobalMatrix& M, GlobalMatrix& K,
    GlobalVector& b, GlobalMatrix& Jac)
{
    DBUG("AssembleWithJacobian ComponentTransportProcess.");

    const int process_id = 0;
    ProcessLib::ProcessVariable const& pv = getProcessVariables(process_id)[0];
    std::vector<std::reference_wrapper<NumLib::LocalToGlobalIndexMap>>
        dof_table = {std::ref(*_local_to_global_index_map)};
    // Call global assembler for each local assembly item.
    GlobalExecutor::executeSelectedMemberDereferenced(
        _global_assembler, &VectorMatrixAssembler::assembleWithJacobian,
        _local_assemblers, pv.getActiveElementIDs(), dof_table, t, x,
        xdot, dxdot_dx, dx_dx, M, K, b, Jac, _coupled_solutions);
}

Eigen::Vector3d ComponentTransportProcess::getFlux(std::size_t const element_id,
                                                   MathLib::Point3d const& p,
                                                   double const t,
                                                   GlobalVector const& x) const
{
    std::vector<GlobalIndexType> indices_cache;
    auto const r_c_indices = NumLib::getRowColumnIndices(
        element_id, *_local_to_global_index_map, indices_cache);

    if (_use_monolithic_scheme)
    {
        std::vector<double> local_x(x.get(r_c_indices.rows));

        return _local_assemblers[element_id]->getFlux(p, t, local_x);
    }
    else
    {
        std::vector<std::vector<GlobalIndexType>>
            indices_of_all_coupled_processes{
                _coupled_solutions->coupled_xs.size(), r_c_indices.rows};
        auto const local_xs = getCurrentLocalSolutions(
            *(this->_coupled_solutions), indices_of_all_coupled_processes);

        return _local_assemblers[element_id]->getFlux(p, t, local_xs);
    }
}

void ComponentTransportProcess::
    setCoupledTermForTheStaggeredSchemeToLocalAssemblers()
{
    DBUG("Set the coupled term for the staggered scheme to local assembers.");

    const int process_id =
        _use_monolithic_scheme ? 0 : _coupled_solutions->process_id;
    ProcessLib::ProcessVariable const& pv = getProcessVariables(process_id)[0];
    GlobalExecutor::executeSelectedMemberOnDereferenced(
        &ComponentTransportLocalAssemblerInterface::
            setStaggeredCoupledSolutions,
        _local_assemblers, pv.getActiveElementIDs(), _coupled_solutions);
}

void ComponentTransportProcess::preTimestepConcreteProcess(
    GlobalVector const& x, const double /*t*/, const double /*delta_t*/,
    int const process_id)
{
    if (_use_monolithic_scheme)
    {
        return;
    }

    if (!_xs_previous_timestep[process_id])
    {
        _xs_previous_timestep[process_id] =
            MathLib::MatrixVectorTraits<GlobalVector>::newInstance(x);
    }
    else
    {
        auto& x0 = *_xs_previous_timestep[process_id];
        MathLib::LinAlg::copy(x, x0);
    }
}

void ComponentTransportProcess::postTimestepConcreteProcess(
    GlobalVector const& x,
    const double t,
    const double /*delta_t*/,
    int const process_id)
{
    // For the monolithic scheme, process_id is always zero.
    if (_use_monolithic_scheme && process_id != 0)
    {
        OGS_FATAL(
            "The condition of process_id = 0 must be satisfied for "
            "monolithic ComponentTransportProcess, which is a single process.");
    }
    if (!_use_monolithic_scheme && process_id != 0)
    {
        DBUG(
            "This is the transport part of the staggered "
            "ComponentTransportProcess.");
        return;
    }
    if (!_surfaceflux)  // computing the surfaceflux is optional
    {
        return;
    }

    ProcessLib::ProcessVariable const& pv = getProcessVariables(process_id)[0];

    _surfaceflux->integrate(x, t, *this, process_id, _integration_order,
                            _mesh, pv.getActiveElementIDs());
    _surfaceflux->save(t);
}

}  // namespace ComponentTransport
}  // namespace ProcessLib
