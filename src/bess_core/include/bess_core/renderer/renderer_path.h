#pragma once

#include "common/class_helpers.h"
#include "common/types.h"
#include <cstddef>
#include <cstdint>
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
        PickingId id = PickingId::invalid();
        bool hasId = false;

        [[nodiscard]] static constexpr PathCommandStroke
        withWidth(float strokeWidth) noexcept {
            return {.width = strokeWidth};
        }

        [[nodiscard]] static constexpr PathCommandStroke
        withPickingId(PickingId pickingId, float strokeWidth = 0.f) noexcept {
            return {.width = strokeWidth, .id = pickingId, .hasId = true};
        }

        [[nodiscard]] static constexpr PathCommandStroke
        withId(PickingId pickingId, float strokeWidth = 0.f) noexcept {
            return withPickingId(pickingId, strokeWidth);
        }

        [[nodiscard]] static constexpr PathCommandStroke
        withWidthAndId(float strokeWidth, PickingId pickingId) noexcept {
            return withPickingId(pickingId, strokeWidth);
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
        dashedWithId(float dash, float gap, PickingId pickingId,
                     float strokeWidth = 0.f, float offset = 0.f) noexcept {
            return {.width = strokeWidth,
                    .dashLength = dash,
                    .gapLength = gap,
                    .dashOffset = offset,
                    .id = pickingId,
                    .hasId = true};
        }

        [[nodiscard]] static constexpr PathCommandStroke
        broken(float strokeWidth = 0.f) noexcept {
            return {
                .width = strokeWidth, .breakBefore = true, .breakAfter = true};
        }

        [[nodiscard]] static constexpr PathCommandStroke
        brokenWithId(PickingId pickingId, float strokeWidth = 0.f) noexcept {
            return {.width = strokeWidth,
                    .breakBefore = true,
                    .breakAfter = true,
                    .id = pickingId,
                    .hasId = true};
        }

        [[nodiscard]] constexpr bool hasWidthOverride() const noexcept {
            return width > 0.f;
        }

        [[nodiscard]] constexpr bool hasIdOverride() const noexcept {
            return hasId;
        }

        [[nodiscard]] constexpr bool isDashed() const noexcept {
            return dashLength > 0.f && gapLength > 0.f;
        }

        [[nodiscard]] constexpr bool isStyled() const noexcept {
            return hasWidthOverride() || isDashed() || breakBefore ||
                   breakAfter || hasIdOverride();
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

        [[nodiscard]] static PathCommand
        lineTo(const glm::vec2 &pos, float strokeWidth, PickingId id) noexcept {
            return lineTo(pos,
                          PathCommandStroke::withWidthAndId(strokeWidth, id));
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

        [[nodiscard]] static PathCommand quadTo(const glm::vec2 &control,
                                                const glm::vec2 &pos,
                                                float strokeWidth,
                                                PickingId id) noexcept {
            return quadTo(control, pos,
                          PathCommandStroke::withWidthAndId(strokeWidth, id));
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

        [[nodiscard]] static PathCommand cubicTo(const glm::vec2 &control1,
                                                 const glm::vec2 &control2,
                                                 const glm::vec2 &pos,
                                                 float strokeWidth,
                                                 PickingId id) noexcept {
            return cubicTo(control1, control2, pos,
                           PathCommandStroke::withWidthAndId(strokeWidth, id));
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

        [[nodiscard]] static PathCommand closePath(float strokeWidth,
                                                   PickingId id) noexcept {
            return closePath(
                PathCommandStroke::withWidthAndId(strokeWidth, id));
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
        explicit Path2D(const std::vector<PathCommand> &commands);

        static Path2D fromSvgString(const std::string &svgData);

        void translate(const glm::vec2 &pos);
        void scale(const glm::vec2 &val);

        // This also overrides ogcommands
        void normalize(const glm::vec2 &size = glm::vec2(0.f));

        // This updates ogCommands
        void setPos(const glm::vec2 &pos);

        void clear() noexcept;

        void reserve(std::size_t commandCount);

        void setCommands(const std::vector<PathCommand> &commands);

        Path2D &append(const Path2D &path);

        Path2D &append(const std::vector<PathCommand> &commands);

        Path2D &addCommand(const PathCommand &command);

        Path2D &moveTo(const glm::vec2 &pos);

        Path2D &lineTo(const glm::vec2 &pos);

        Path2D &lineTo(const glm::vec2 &pos, const PathCommandStroke &stroke);

        Path2D &lineTo(const glm::vec2 &pos, float strokeWidth);

        Path2D &lineTo(const glm::vec2 &pos, float strokeWidth, PickingId id);

        Path2D &quadTo(const glm::vec2 &control, const glm::vec2 &pos);

        Path2D &quadTo(const glm::vec2 &control, const glm::vec2 &pos,
                       const PathCommandStroke &stroke);

        Path2D &quadTo(const glm::vec2 &control, const glm::vec2 &pos,
                       float strokeWidth);

        Path2D &quadTo(const glm::vec2 &control, const glm::vec2 &pos,
                       float strokeWidth, PickingId id);

        Path2D &quadraticTo(const glm::vec2 &control, const glm::vec2 &pos);

        Path2D &quadraticTo(const glm::vec2 &control, const glm::vec2 &pos,
                            const PathCommandStroke &stroke);

        Path2D &quadraticTo(const glm::vec2 &control, const glm::vec2 &pos,
                            float strokeWidth);

        Path2D &quadraticTo(const glm::vec2 &control, const glm::vec2 &pos,
                            float strokeWidth, PickingId id);

        Path2D &quadraticBezierTo(const glm::vec2 &control,
                                  const glm::vec2 &pos);

        Path2D &quadraticBezierTo(const glm::vec2 &control,
                                  const glm::vec2 &pos,
                                  const PathCommandStroke &stroke);

        Path2D &quadraticBezierTo(const glm::vec2 &control,
                                  const glm::vec2 &pos, float strokeWidth);

        Path2D &quadraticBezierTo(const glm::vec2 &control,
                                  const glm::vec2 &pos, float strokeWidth,
                                  PickingId id);

        Path2D &cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                        const glm::vec2 &pos);

        Path2D &cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                        const glm::vec2 &pos, const PathCommandStroke &stroke);

        Path2D &cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                        const glm::vec2 &pos, float strokeWidth);

        Path2D &cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                        const glm::vec2 &pos, float strokeWidth, PickingId id);

        Path2D &cubicBezierTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos);

        Path2D &cubicBezierTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              const PathCommandStroke &stroke);

        Path2D &cubicBezierTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              float strokeWidth);

        Path2D &cubicBezierTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              float strokeWidth, PickingId id);

        Path2D &bezierCurveTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos);

        Path2D &bezierCurveTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              const PathCommandStroke &stroke);

        Path2D &bezierCurveTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              float strokeWidth);

        Path2D &bezierCurveTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos,
                              float strokeWidth, PickingId id);

        Path2D &closePath();

        Path2D &closePath(const PathCommandStroke &stroke);

        Path2D &closePath(float strokeWidth);

        Path2D &closePath(float strokeWidth, PickingId id);

        Path2D &close();

        Path2D &close(const PathCommandStroke &stroke);

        Path2D &close(float strokeWidth);

        Path2D &close(float strokeWidth, PickingId id);

        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] std::size_t commandCount() const noexcept;

        [[nodiscard]] std::vector<PathCommand> commands() const noexcept;

        [[nodiscard]] const PathCommand *data() const noexcept;

        [[nodiscard]] PathBounds bounds() const noexcept;
        [[nodiscard]] PathBounds ogBounds() const noexcept;

        [[nodiscard]] bool hasBounds() const noexcept;

        [[nodiscard]] uint64_t revision() const noexcept;

        MAKE_GETTER_SETTER(bool, Fill, m_fill)

      private:
        void includePoint(const glm::vec2 &point) noexcept;

        void includeCommandBounds(const PathCommand &command) noexcept;

        std::vector<PathCommand> m_commands;
        std::vector<PathCommand> m_ogCommands;
        PathBounds m_bounds, m_ogbounds;
        uint64_t m_revision = 0;
        glm::vec2 m_currentScale = {1.f, 1.f};
        bool m_fill = true;
    };

} // namespace Bess::Core::Renderer
