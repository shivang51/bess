#pragma once

#include "bess_core/renderer/renderer_types.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Bess::Core::Renderer {

    enum class PathCommandKind : uint8_t {
        Move,
        Line,
        Quad,
        Cubic,
        Close,
    };

    struct PathCommandStroke {
        // 0 inherits PathProps::strokeSize for this command.
        float width = 0.f;
        // Dash and gap are in path/world units. A non-positive value disables
        // the dashed pattern for this command.
        float dashLength = 0.f;
        float gapLength = 0.f;
        float dashOffset = 0.f;
        // Starts/ends this command as a separate open stroke contour.
        bool breakBefore = false;
        bool breakAfter = false;

        [[nodiscard]] static constexpr PathCommandStroke
        withWidth(float strokeWidth) noexcept {
            return {.width = strokeWidth};
        }

        [[nodiscard]] static constexpr PathCommandStroke
        dashed(float dash, float gap, float strokeWidth = 0.f,
               float offset = 0.f) noexcept {
            return {.width = strokeWidth,
                    .dashLength = dash,
                    .gapLength = gap,
                    .dashOffset = offset};
        }

        [[nodiscard]] static constexpr PathCommandStroke
        broken(float strokeWidth = 0.f) noexcept {
            return {
                .width = strokeWidth, .breakBefore = true, .breakAfter = true};
        }

        [[nodiscard]] constexpr bool hasWidthOverride() const noexcept {
            return width > 0.f;
        }

        [[nodiscard]] constexpr bool isDashed() const noexcept {
            return dashLength > 0.f && gapLength > 0.f;
        }

        [[nodiscard]] constexpr bool isStyled() const noexcept {
            return hasWidthOverride() || isDashed() || breakBefore ||
                   breakAfter;
        }
    };

    struct PathCommand {
        PathCommandKind kind = PathCommandKind::Move;
        glm::vec2 p{0.f};
        glm::vec2 control{0.f};
        glm::vec2 control2{0.f};
        PathCommandStroke stroke{};

        [[nodiscard]] static PathCommand moveTo(const glm::vec2 &pos) noexcept {
            return {.kind = PathCommandKind::Move, .p = pos};
        }

        [[nodiscard]] static PathCommand lineTo(const glm::vec2 &pos) noexcept {
            return {.kind = PathCommandKind::Line, .p = pos};
        }

        [[nodiscard]] static PathCommand
        lineTo(const glm::vec2 &pos, const PathCommandStroke &stroke) noexcept {
            return {.kind = PathCommandKind::Line, .p = pos, .stroke = stroke};
        }

        [[nodiscard]] static PathCommand lineTo(const glm::vec2 &pos,
                                                float strokeWidth) noexcept {
            return lineTo(pos, PathCommandStroke::withWidth(strokeWidth));
        }

        [[nodiscard]] static PathCommand quadTo(const glm::vec2 &control,
                                                const glm::vec2 &pos) noexcept {
            return {
                .kind = PathCommandKind::Quad, .p = pos, .control = control};
        }

        [[nodiscard]] static PathCommand
        quadTo(const glm::vec2 &control, const glm::vec2 &pos,
               const PathCommandStroke &stroke) noexcept {
            return {.kind = PathCommandKind::Quad,
                    .p = pos,
                    .control = control,
                    .stroke = stroke};
        }

        [[nodiscard]] static PathCommand quadTo(const glm::vec2 &control,
                                                const glm::vec2 &pos,
                                                float strokeWidth) noexcept {
            return quadTo(control, pos,
                          PathCommandStroke::withWidth(strokeWidth));
        }

        [[nodiscard]] static PathCommand
        cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                const glm::vec2 &pos) noexcept {
            return {.kind = PathCommandKind::Cubic,
                    .p = pos,
                    .control = control1,
                    .control2 = control2};
        }

        [[nodiscard]] static PathCommand
        cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                const glm::vec2 &pos,
                const PathCommandStroke &stroke) noexcept {
            return {.kind = PathCommandKind::Cubic,
                    .p = pos,
                    .control = control1,
                    .control2 = control2,
                    .stroke = stroke};
        }

        [[nodiscard]] static PathCommand cubicTo(const glm::vec2 &control1,
                                                 const glm::vec2 &control2,
                                                 const glm::vec2 &pos,
                                                 float strokeWidth) noexcept {
            return cubicTo(control1, control2, pos,
                           PathCommandStroke::withWidth(strokeWidth));
        }

        [[nodiscard]] static PathCommand closePath() noexcept {
            return {.kind = PathCommandKind::Close};
        }

        [[nodiscard]] static PathCommand
        closePath(const PathCommandStroke &stroke) noexcept {
            return {.kind = PathCommandKind::Close, .stroke = stroke};
        }

        [[nodiscard]] static PathCommand closePath(float strokeWidth) noexcept {
            return closePath(PathCommandStroke::withWidth(strokeWidth));
        }
    };

    struct PathBounds {
        glm::vec2 min{0.f};
        glm::vec2 max{0.f};
        bool valid = false;

        [[nodiscard]] glm::vec2 size() const noexcept {
            return valid ? max - min : glm::vec2(0.f);
        }
    };

    class Path2D final {
      public:
        using Command = PathCommand;

        Path2D() = default;
        explicit Path2D(std::span<const PathCommand> commands) {
            setCommands(commands);
        }

        void clear() noexcept {
            m_commands.clear();
            m_bounds = {};
            ++m_revision;
        }

        void reserve(std::size_t commandCount) {
            m_commands.reserve(commandCount);
        }

        void setCommands(std::span<const PathCommand> commands) {
            m_commands.clear();
            m_bounds = {};
            reserve(commands.size());
            if (commands.empty()) {
                ++m_revision;
                return;
            }
            append(commands);
        }

        Path2D &append(const Path2D &path) { return append(path.commands()); }

        Path2D &append(std::span<const PathCommand> commands) {
            if (commands.empty()) {
                return *this;
            }

            m_commands.insert(m_commands.end(), commands.begin(),
                              commands.end());
            for (const PathCommand &command : commands) {
                includeCommandBounds(command);
            }
            ++m_revision;
            return *this;
        }

        Path2D &addCommand(const PathCommand &command) {
            m_commands.push_back(command);
            includeCommandBounds(command);
            ++m_revision;
            return *this;
        }

        Path2D &moveTo(const glm::vec2 &pos) {
            return addCommand(PathCommand::moveTo(pos));
        }

        Path2D &lineTo(const glm::vec2 &pos) {
            return addCommand(PathCommand::lineTo(pos));
        }

        Path2D &lineTo(const glm::vec2 &pos, const PathCommandStroke &stroke) {
            return addCommand(PathCommand::lineTo(pos, stroke));
        }

        Path2D &lineTo(const glm::vec2 &pos, float strokeWidth) {
            return lineTo(pos, PathCommandStroke::withWidth(strokeWidth));
        }

        Path2D &quadTo(const glm::vec2 &control, const glm::vec2 &pos) {
            return addCommand(PathCommand::quadTo(control, pos));
        }

        Path2D &quadTo(const glm::vec2 &control, const glm::vec2 &pos,
                       const PathCommandStroke &stroke) {
            return addCommand(PathCommand::quadTo(control, pos, stroke));
        }

        Path2D &quadTo(const glm::vec2 &control, const glm::vec2 &pos,
                       float strokeWidth) {
            return quadTo(control, pos,
                          PathCommandStroke::withWidth(strokeWidth));
        }

        Path2D &quadraticTo(const glm::vec2 &control, const glm::vec2 &pos) {
            return quadTo(control, pos);
        }

        Path2D &quadraticTo(const glm::vec2 &control, const glm::vec2 &pos,
                            const PathCommandStroke &stroke) {
            return quadTo(control, pos, stroke);
        }

        Path2D &quadraticTo(const glm::vec2 &control, const glm::vec2 &pos,
                            float strokeWidth) {
            return quadTo(control, pos, strokeWidth);
        }

        Path2D &quadraticBezierTo(const glm::vec2 &control,
                                  const glm::vec2 &pos) {
            return quadTo(control, pos);
        }

        Path2D &quadraticBezierTo(const glm::vec2 &control,
                                  const glm::vec2 &pos,
                                  const PathCommandStroke &stroke) {
            return quadTo(control, pos, stroke);
        }

        Path2D &quadraticBezierTo(const glm::vec2 &control,
                                  const glm::vec2 &pos, float strokeWidth) {
            return quadTo(control, pos, strokeWidth);
        }

        Path2D &cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                        const glm::vec2 &pos) {
            return addCommand(PathCommand::cubicTo(control1, control2, pos));
        }

        Path2D &cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                        const glm::vec2 &pos, const PathCommandStroke &stroke) {
            return addCommand(
                PathCommand::cubicTo(control1, control2, pos, stroke));
        }

        Path2D &cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                        const glm::vec2 &pos, float strokeWidth) {
            return cubicTo(control1, control2, pos,
                           PathCommandStroke::withWidth(strokeWidth));
        }

        Path2D &cubicBezierTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos) {
            return cubicTo(control1, control2, pos);
        }

        Path2D &cubicBezierTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              const PathCommandStroke &stroke) {
            return cubicTo(control1, control2, pos, stroke);
        }

        Path2D &cubicBezierTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              float strokeWidth) {
            return cubicTo(control1, control2, pos, strokeWidth);
        }

        Path2D &bezierCurveTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos) {
            return cubicTo(control1, control2, pos);
        }

        Path2D &bezierCurveTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              const PathCommandStroke &stroke) {
            return cubicTo(control1, control2, pos, stroke);
        }

        Path2D &bezierCurveTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              float strokeWidth) {
            return cubicTo(control1, control2, pos, strokeWidth);
        }

        Path2D &closePath() { return addCommand(PathCommand::closePath()); }

        Path2D &closePath(const PathCommandStroke &stroke) {
            return addCommand(PathCommand::closePath(stroke));
        }

        Path2D &closePath(float strokeWidth) {
            return closePath(PathCommandStroke::withWidth(strokeWidth));
        }

        Path2D &close() { return closePath(); }

        Path2D &close(const PathCommandStroke &stroke) {
            return closePath(stroke);
        }

        Path2D &close(float strokeWidth) { return closePath(strokeWidth); }

        [[nodiscard]] bool empty() const noexcept { return m_commands.empty(); }

        [[nodiscard]] std::size_t commandCount() const noexcept {
            return m_commands.size();
        }

        [[nodiscard]] std::span<const PathCommand> commands() const noexcept {
            return {m_commands.data(), m_commands.size()};
        }

        [[nodiscard]] const PathCommand *data() const noexcept {
            return m_commands.data();
        }

        [[nodiscard]] PathBounds bounds() const noexcept { return m_bounds; }

        [[nodiscard]] bool hasBounds() const noexcept { return m_bounds.valid; }

        [[nodiscard]] uint64_t revision() const noexcept { return m_revision; }

      private:
        void includePoint(const glm::vec2 &point) noexcept {
            if (!m_bounds.valid) {
                m_bounds.min = point;
                m_bounds.max = point;
                m_bounds.valid = true;
                return;
            }

            m_bounds.min = glm::min(m_bounds.min, point);
            m_bounds.max = glm::max(m_bounds.max, point);
        }

        void includeCommandBounds(const PathCommand &command) noexcept {
            switch (command.kind) {
            case PathCommandKind::Move:
            case PathCommandKind::Line:
                includePoint(command.p);
                break;
            case PathCommandKind::Quad:
                includePoint(command.control);
                includePoint(command.p);
                break;
            case PathCommandKind::Cubic:
                includePoint(command.control);
                includePoint(command.control2);
                includePoint(command.p);
                break;
            case PathCommandKind::Close:
                break;
            }
        }

        std::vector<PathCommand> m_commands;
        PathBounds m_bounds;
        uint64_t m_revision = 0;
    };

} // namespace Bess::Core::Renderer
