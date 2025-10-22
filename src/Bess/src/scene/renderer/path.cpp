#include "scene/renderer/path.h"
#include "common/log.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "spdlog/fmt/bundled/base.h"

namespace Bess::Renderer {
    Path *Path::moveTo(const glm::vec2 &pos) {
        Path::PathCommand cmd;
        cmd.kind = Path::PathCommand::Kind::Move;
        cmd.move.p = pos;
        return addCommand(cmd);
    }

    Path *Path::lineTo(const glm::vec2 &pos) {
        Path::PathCommand cmd;
        cmd.kind = Path::PathCommand::Kind::Line;
        cmd.line.p = pos;
        return addCommand(cmd);
    }

    Path *Path::quadTo(const glm::vec2 &c, const glm::vec2 &pos) {
        Path::PathCommand cmd;
        cmd.kind = Path::PathCommand::Kind::Quad;
        cmd.quad.p = pos;
        cmd.quad.c = c;
        return addCommand(cmd);
    }

    Path *Path::cubicTo(const glm::vec2 &c1, const glm::vec2 &c2, const glm::vec2 &pos) {
        Path::PathCommand cmd;
        cmd.kind = Path::PathCommand::Kind::Quad;
        cmd.cubic.p = pos;
        cmd.cubic.c1 = c1;
        cmd.cubic.c2 = c2;
        return addCommand(cmd);
    }

    Path *Path::addCommand(PathCommand cmd) {
        m_cmds.emplace_back(cmd);
        return this;
    }
    const std::vector<Path::PathCommand> &Path::getCmds() const {
        return m_cmds;
    }

    void Path::setCommands(const std::vector<PathCommand> &cmds) {
        m_cmds = cmds;
    }

    using PathPoint = Renderer2D::Vulkan::PathPoint;
    const std::vector<std::vector<PathPoint>> &Path::getContours() {
        if (m_cmds.empty()) {
            return m_contours;
        }

        if (!m_contours.empty())
            return m_contours;

        formContours();
        assert(!m_contours.empty());

        return m_contours;
    }

    void Path::formContours() {
        glm::vec3 currentPos = glm::vec3(0.f);
        std::vector<PathPoint> points = {PathPoint{currentPos, 1.f, -1}};
        for (auto &c : m_cmds) {
            using Kind = Renderer::Path::PathCommand::Kind;
            switch (c.kind) {
            case Kind::Move: {
                const auto &pos = c.move.p;
                currentPos = {c.move.p, c.z};
                auto prevPoint = points.back();
                if (points.size() == 1) {
                    points[0] = PathPoint{{pos, c.z}, c.weight, c.id};
                    continue;
                }

                m_contours.emplace_back(std::move(points));
                points.clear();
                points.emplace_back(PathPoint{{pos, c.z}, c.weight, c.id});
            } break;
            case Kind::Line: {
                points.emplace_back(PathPoint{{c.line.p, c.z}, c.weight, c.id});
                currentPos = {c.line.p, c.z};
            } break;
            case Kind::Quad: {
                auto positions = Renderer2D::Vulkan::PathRenderer::generateQuadBezierPoints(currentPos, c.quad.c, {c.quad.p, c.z});
                points.reserve(points.size() + positions.size());
                for (const auto &pos : positions) {
                    points.emplace_back(PathPoint{pos, c.weight, c.id});
                }
                currentPos = {c.quad.p, c.z};
            } break;
            case Kind::Cubic: {
                auto positions = Renderer2D::Vulkan::PathRenderer::generateCubicBezierPoints(currentPos,
                                                                                             c.cubic.c1, c.cubic.c2,
                                                                                             {c.cubic.p, c.z});
                points.reserve(points.size() + positions.size());
                for (const auto &pos : positions) {
                    points.emplace_back(PathPoint{pos, c.weight, c.id});
                }
                currentPos = {c.cubic.p, c.z};
            } break;
            }
        }

        if (!points.empty()) {
            m_contours.emplace_back(std::move(points));
            points.clear();
        }
    }

    bool Path::scale(const glm::vec2 &factor) {
        if (factor == m_currentScale) {
            return false;
        }

        m_currentScale = factor;

        m_contours.clear();
        for (auto &cmd : m_cmds) {
            using Kind = Renderer::Path::PathCommand::Kind;
            switch (cmd.kind) {
            case Kind::Move: {
                cmd.move.p *= factor;
            } break;
            case Kind::Line: {
                cmd.line.p *= factor;
            } break;
            case Kind::Quad: {
                cmd.quad.p *= factor;
                cmd.quad.c *= factor;
            } break;
            case Kind::Cubic: {
                cmd.cubic.p *= factor;
                cmd.cubic.c1 *= factor;
                cmd.cubic.c2 *= factor;
            } break;
            }
        }

        return true;
    }

    PathProperties &Path::getPropsRef() { return m_props; }

    const PathProperties &Path::getProps() const { return m_props; }

    void Path::setProps(const PathProperties &props) { m_props = props; }
} // namespace Bess::Renderer
