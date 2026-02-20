cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

add_subdirectory(common)
add_subdirectory(bess_json)
add_subdirectory(bessplug/bindings)
add_subdirectory(plugin_manager)
add_subdirectory(event_system)
add_subdirectory(sim_engine)
add_subdirectory(bess_vulkan)
add_subdirectory(command_system)
add_subdirectory(bess)

