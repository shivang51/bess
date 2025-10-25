#include "scene/components/non_sim_comp.h"



namespace Bess::Canvas::Components {
    std::vector<NSComponent> getNSComponents(){
        return {
            {NSComponentType::text, "Text"}
        };
    }
}