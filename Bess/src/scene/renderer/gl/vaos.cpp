#include "scene/renderer/gl/vaos.h"

namespace Bess::Gl {
    BufferLayout QuadVao::getInstanceLayout() {
        return BufferLayout{
            {GL_FLOAT, 3, sizeof(QuadVertex), offsetof(QuadVertex, position), false, true},
            {GL_FLOAT, 4, sizeof(QuadVertex), offsetof(QuadVertex, color), false, true},
            {GL_FLOAT, 4, sizeof(QuadVertex), offsetof(QuadVertex, borderRadius), false, true},
            {GL_FLOAT, 4, sizeof(QuadVertex), offsetof(QuadVertex, borderColor), false, true},
            {GL_FLOAT, 4, sizeof(QuadVertex), offsetof(QuadVertex, borderSize), false, true},
            {GL_FLOAT, 2, sizeof(QuadVertex), offsetof(QuadVertex, size), false, true},
            {GL_INT, 1, sizeof(QuadVertex), offsetof(QuadVertex, id), false, true},
            {GL_INT, 1, sizeof(QuadVertex), offsetof(QuadVertex, isMica), false, true},
            {GL_INT, 1, sizeof(QuadVertex), offsetof(QuadVertex, texSlotIdx), false, true},
            {GL_FLOAT, 4, sizeof(QuadVertex), offsetof(QuadVertex, texData), false, true}};
    }

    BufferLayout CircleVao::getInstanceLayout() {
        return BufferLayout{
            {GL_FLOAT, 3, sizeof(CircleVertex), offsetof(CircleVertex, position), false, true},
            {GL_FLOAT, 4, sizeof(CircleVertex), offsetof(CircleVertex, color), false, true},
            {GL_FLOAT, 1, sizeof(CircleVertex), offsetof(CircleVertex, radius), false, true},
            {GL_FLOAT, 1, sizeof(CircleVertex), offsetof(CircleVertex, innerRadius), false, true},
            {GL_INT, 1, sizeof(CircleVertex), offsetof(CircleVertex, id), false, true},
        };
    }
}