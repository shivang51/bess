set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ALL_SOURCE_FILES
"cpp/palettes/tones.cc"
"cpp/utils/utils.cc"
"cpp/contrast/contrast.cc"
"cpp/scheme/scheme_tonal_spot.cc"
"cpp/scheme/scheme_neutral.cc"
"cpp/scheme/scheme_rainbow.cc"
"cpp/scheme/scheme_fruit_salad.cc"
"cpp/scheme/scheme_monochrome.cc"
"cpp/scheme/scheme_fidelity.cc"
"cpp/scheme/scheme_vibrant.cc"
"cpp/scheme/scheme_content.cc"
"cpp/scheme/scheme_expressive.cc"
"cpp/dislike/dislike.cc"
"cpp/blend/blend.cc"
"cpp/score/score.cc"
"cpp/quantize/wsmeans.cc"
"cpp/quantize/wu.cc"
"cpp/quantize/celebi.cc"
"cpp/quantize/lab.cc"
"cpp/cam/hct_solver.cc"
"cpp/cam/hct.cc"
"cpp/cam/cam.cc"
"cpp/cam/viewing_conditions.cc"
"cpp/temperature/temperature_cache.cc"
"cpp/dynamiccolor/material_dynamic_colors.cc"
"cpp/dynamiccolor/dynamic_scheme.cc"
"cpp/dynamiccolor/dynamic_color.cc"
)

add_library(material-color-utils OBJECT ${ALL_SOURCE_FILES})

target_include_directories(material-color-utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

