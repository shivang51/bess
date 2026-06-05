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

    struct PathCommand {
        PathCommandKind kind = PathCommandKind::Move;
        glm::vec2 p{0.f};
        glm::vec2 control{0.f};
        glm::vec2 control2{0.f};

        [[nodiscard]] static PathCommand moveTo(const glm::vec2 &pos) noexcept {
            return {.kind = PathCommandKind::Move, .p = pos};
        }

        [[nodiscard]] static PathCommand lineTo(const glm::vec2 &pos) noexcept {
            return {.kind = PathCommandKind::Line, .p = pos};
        }

        [[nodiscard]] static PathCommand quadTo(const glm::vec2 &control,
                                                const glm::vec2 &pos) noexcept {
            return {
                .kind = PathCommandKind::Quad, .p = pos, .control = control};
        }

        [[nodiscard]] static PathCommand
        cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                const glm::vec2 &pos) noexcept {
            return {.kind = PathCommandKind::Cubic,
                    .p = pos,
                    .control = control1,
                    .control2 = control2};
        }

        [[nodiscard]] static PathCommand closePath() noexcept {
            return {.kind = PathCommandKind::Close};
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

        Path2D &quadTo(const glm::vec2 &control, const glm::vec2 &pos) {
            return addCommand(PathCommand::quadTo(control, pos));
        }

        Path2D &quadraticTo(const glm::vec2 &control, const glm::vec2 &pos) {
            return quadTo(control, pos);
        }

        Path2D &quadraticBezierTo(const glm::vec2 &control,
                                  const glm::vec2 &pos) {
            return quadTo(control, pos);
        }

        Path2D &cubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                        const glm::vec2 &pos) {
            return addCommand(PathCommand::cubicTo(control1, control2, pos));
        }

        Path2D &cubicBezierTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos) {
            return cubicTo(control1, control2, pos);
        }

        Path2D &bezierCurveTo(const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &pos) {
            return cubicTo(control1, control2, pos);
        }

        Path2D &closePath() { return addCommand(PathCommand::closePath()); }

        Path2D &close() { return closePath(); }

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
