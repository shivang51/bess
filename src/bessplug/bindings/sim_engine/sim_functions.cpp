// included for type completion of DigCompSimData
#include "common/bess_assert.h"
#include "dig_sim_driver.h" // NOLINT
#include "expression_evalutator/expr_evaluator.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

struct PySimulationFunctionWrapper;

typedef std::shared_ptr<Bess::SimEngine::Drivers::Digital::DigCompSimData> TSimFnDataPtr;

TSimFnDataPtr exprEvalSimFunc(const TSimFnDataPtr &simData);

void bind_sim_functions(py::module_ &m) {
    using namespace Bess::SimEngine;

    m.def("expr_eval_sim_func",
          exprEvalSimFunc,
          py::arg("sim_data"),
          "Expression evaluator simulation function.");
}
