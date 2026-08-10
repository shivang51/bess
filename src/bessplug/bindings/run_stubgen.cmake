if(NOT Python3_EXECUTABLE OR NOT BESS_RUNTIME_DIR OR NOT BESS_MODULE_DIR OR NOT OUT_DIR)
    message(FATAL_ERROR "run_stubgen.cmake: missing required -D arguments")
endif()

set(_stubgen_py "${CMAKE_CURRENT_LIST_DIR}/run_stubgen.py")
if(NOT EXISTS "${_stubgen_py}")
    message(FATAL_ERROR "run_stubgen.cmake: missing ${_stubgen_py}")
endif()

# Run in-process via a helper so Windows can os.add_dll_directory(runtime_dir).
# Python 3.8+ ignores PATH when loading extension-module DLL dependencies.
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}"
        "${_stubgen_py}"
        "${BESS_RUNTIME_DIR}"
        "${BESS_MODULE_DIR}"
        "${OUT_DIR}"
    RESULT_VARIABLE _stubgen_result
    ERROR_VARIABLE _stubgen_error
    OUTPUT_VARIABLE _stubgen_output
)

if(NOT _stubgen_result EQUAL 0)
    message(FATAL_ERROR
        "pybind11-stubgen failed (exit ${_stubgen_result})\n"
        "${_stubgen_output}${_stubgen_error}\n"
        "Python: ${Python3_EXECUTABLE}\n"
        "Module dir: ${BESS_MODULE_DIR}\n"
        "Runtime dir: ${BESS_RUNTIME_DIR}")
endif()
