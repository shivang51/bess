// included for type completion of DigCompSimData
#include "drivers/digital_sim_driver.h" // NOLINT
#include "expression_evalutator/expr_evaluator.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

struct PySimulationFunctionWrapper;

void bind_sim_functions(py::module_ &m) {
    using namespace Bess::SimEngine;

    m.def("expr_eval_sim_func",
          ExprEval::exprEvalSimFunc,
          py::arg("sim_data"),
          "Expression evaluator simulation function.");
}
