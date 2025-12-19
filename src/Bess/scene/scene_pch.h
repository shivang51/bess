#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/viewport.h"

#include "scene/artist/artist_manager.h"
#include "scene/artist/base_artist.h"
#include "scene/artist/nodes_artist.h"
#include "scene/artist/schematic_artist.h"

#include "scene/components/components.h"
#include "scene/components/json_converters.h"
#include "scene/components/non_sim_comp.h"

#include "scene/renderer/font.h"
#include "scene/renderer/glyph_extractor.h"
#include "scene/renderer/material.h"
#include "scene/renderer/material_renderer.h"
#include "scene/renderer/msdf_font.h"
#include "scene/renderer/path.h"

#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/renderer/vulkan/text_renderer.h"

#include "scene/renderer/vulkan/pipelines/circle_pipeline.h"
#include "scene/renderer/vulkan/pipelines/grid_pipeline.h"
#include "scene/renderer/vulkan/pipelines/path_fill_pipeline.h"
#include "scene/renderer/vulkan/pipelines/path_stroke_pipeline.h"
#include "scene/renderer/vulkan/pipelines/pipeline.h"
#include "scene/renderer/vulkan/pipelines/quad_pipeline.h"
#include "scene/renderer/vulkan/pipelines/text_pipeline.h"

#include "scene/commands/add_command.h"
#include "scene/commands/connect_command.h"
#include "scene/commands/del_connection_command.h"
#include "scene/commands/delete_comp_command.h"
#include "scene/commands/set_input_command.h"
