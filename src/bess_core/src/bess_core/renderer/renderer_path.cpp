#include "bess_core/renderer/renderer_path.h"
#include "common/bess_assert.h"
#include "common/logger.h"

namespace Bess::Core::Renderer {
    Path2D::Path2D(const std::vector<PathCommand> &commands) {
        setCommands(commands);
    }

    // Skip delimiters - space, comma, tab, newline, carriage return
    void skipSeparators(const char *&ptr, const char *end) {
        while (ptr < end && (*ptr == ' ' || *ptr == ',' || *ptr == '\t' ||
                             *ptr == '\n' || *ptr == '\r')) {
            ptr++;
        }
    }

    float parseNumber(const char *&ptr, const char *end) {
        skipSeparators(ptr, end);
        if (ptr >= end)
            return 0.0f;

        float value = 0.0f;

        auto result = std::from_chars(ptr, end, value);

        if (result.ec == std::errc()) {
            ptr = result.ptr;
        } else {
            if (ptr < end)
                ptr++;
        }
        return value;
    }

    glm::vec2 reflectPoint(glm::vec2 p, glm::vec2 pivot) {
        return 2.0f * pivot - p;
    }

    // Convers Arc to Cubics ---
    void arcToCubics(Path2D &path,
                     glm::vec2 cur,
                     glm::vec2 rxry,
                     float phi_deg,
                     bool large_arc,
                     bool sweep,
                     glm::vec2 end) {
        if (rxry.x == 0 || rxry.y == 0) {
            path.lineTo(end);
            return;
        }

        float phi = glm::radians(phi_deg);
        float cos_phi = std::cos(phi);
        float sin_phi = std::sin(phi);

        glm::vec2 dp = (cur - end) / 2.0f;
        float x1p = (cos_phi * dp.x) + (sin_phi * dp.y);
        float y1p = (-sin_phi * dp.x) + (cos_phi * dp.y);

        float rx = std::abs(rxry.x);
        float ry = std::abs(rxry.y);
        float rx2 = rx * rx;
        float ry2 = ry * ry;
        float x1p2 = x1p * x1p;
        float y1p2 = y1p * y1p;

        // Correct radii
        float lam = (x1p2 / rx2) + (y1p2 / ry2);
        if (lam > 1.0f) {
            float scale = std::sqrt(lam);
            rx *= scale;
            ry *= scale;
            rx2 = rx * rx;
            ry2 = ry * ry;
        }

        // Compute center prime
        float num = std::max(0.0f, (rx2 * ry2) - (rx2 * y1p2) - (ry2 * x1p2));
        float denom = (rx2 * y1p2) + (ry2 * x1p2);
        float cc = 0.0f;
        if (denom != 0.0f) {
            cc = std::sqrt(num / denom);
        }
        if (large_arc == sweep)
            cc = -cc;

        float cxp = cc * (rx * y1p / ry);
        float cyp = cc * (-ry * x1p / rx);

        // Compute center
        glm::vec2 center(
            (cos_phi * cxp) - (sin_phi * cyp) + ((cur.x + end.x) / 2.0f),
            (sin_phi * cxp) + (cos_phi * cyp) + ((cur.y + end.y) / 2.0f));

        // Angles
        auto angle = [&](float ux, float uy, float vx, float vy) {
            float dot = (ux * vx) + (uy * vy);
            float det = (ux * vy) - (uy * vx);
            return std::atan2(det, dot);
        };

        glm::vec2 u((x1p - cxp) / rx, (y1p - cyp) / ry);
        glm::vec2 v((-x1p - cxp) / rx, (-y1p - cyp) / ry);

        float theta1 = std::atan2(u.y, u.x);
        float delta = angle(u.x, u.y, v.x, v.y);

        if (!sweep && delta > 0)
            delta -= 2.0f * glm::pi<float>();
        else if (sweep && delta < 0)
            delta += 2.0f * glm::pi<float>();

        // Split segments
        int segs = std::max(
            1, (int)std::ceil(std::abs(delta) / (glm::pi<float>() / 2.0f)));
        float delta_per = delta / (float)segs;

        for (int i = 0; i < segs; ++i) {
            float t1 = theta1 + ((float)i * delta_per);
            float t2 = t1 + delta_per;
            float dt = t2 - t1;

            // Calculate points and derivatives for bezier approx
            auto getPoint = [&](float t) {
                float ct = std::cos(t);
                float st = std::sin(t);
                return glm::vec2(
                    center.x + (rx * cos_phi * ct) - (ry * sin_phi * st),
                    center.y + (rx * sin_phi * ct) + (ry * cos_phi * st));
            };

            auto getDeriv = [&](float t) {
                float ct = std::cos(t);
                float st = std::sin(t);
                return glm::vec2((-rx * cos_phi * st) - (ry * sin_phi * ct),
                                 (-rx * sin_phi * st) + (ry * cos_phi * ct));
            };

            glm::vec2 p1 = getPoint(t1);
            glm::vec2 p2 = getPoint(t2);
            glm::vec2 d1 = getDeriv(t1);
            glm::vec2 d2 = getDeriv(t2);

            float k = (4.0f / 3.0f) * std::tan(dt / 4.0f);

            glm::vec2 c1 = p1 + d1 * k;
            glm::vec2 c2 = p2 - d2 * k;

            path.cubicTo(c1, c2, p2);
        }
    }

    Path2D Path2D::fromSvgString(const std::string &svgData) {
        Path2D path;
        const char *ptr = svgData.data();
        const char *end = ptr + svgData.size();

        glm::vec2 cur(0.0f);
        glm::vec2 subPathStart(0.0f);

        glm::vec2 prevCubicCtrl(0.0f);
        glm::vec2 prevQuadCtrl(0.0f);

        char lastCmd = 0;

        char cmd = 0;

        while (ptr < end) {
            skipSeparators(ptr, end);
            if (ptr >= end)
                break;

            char c = *ptr;

            // Check if it is a command char
            bool isCmdChar = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');

            if (isCmdChar) {
                cmd = c;
                ptr++;
            } else {
                // Implicit repetition of previous command
                if (lastCmd == 0) {
                    // Error: data started with number? skip
                    ptr++;
                    continue;
                }

                // Repetition Logic per SVG spec
                if (cmd == 'M')
                    cmd = 'L';
                else if (cmd == 'm')
                    cmd = 'l';
                // Other commands repeat as themselves
            }

            bool absolute = (cmd >= 'A' && cmd <= 'Z');
            char type =
                (char)(absolute ? cmd
                                : (cmd - 'a' + 'A')); // Normalize to Uppercase

            // Parse logic based on Normalized Type
            switch (type) {
            case 'M': { // Move
                float x = parseNumber(ptr, end);
                float y = parseNumber(ptr, end);
                glm::vec2 p(x, y);
                if (!absolute)
                    p += cur;

                path.moveTo(p);
                cur = p;
                subPathStart = p;
                prevCubicCtrl = p;
                prevQuadCtrl = p;
                break;
            }
            case 'L': { // Line
                float x = parseNumber(ptr, end);
                float y = parseNumber(ptr, end);
                glm::vec2 p(x, y);
                if (!absolute)
                    p += cur;

                path.lineTo(p);
                cur = p;
                break;
            }
            case 'H': { // Horizontal Line
                float x = parseNumber(ptr, end);
                glm::vec2 p(absolute ? x : cur.x + x, cur.y);
                path.lineTo(p);
                cur = p;
                break;
            }
            case 'V': { // Vertical Line
                float y = parseNumber(ptr, end);
                glm::vec2 p(cur.x, absolute ? y : cur.y + y);
                path.lineTo(p);
                cur = p;
                break;
            }
            case 'C': { // Cubic Bezier
                float c1x = parseNumber(ptr, end);
                float c1y = parseNumber(ptr, end);
                float c2x = parseNumber(ptr, end);
                float c2y = parseNumber(ptr, end);
                float ex = parseNumber(ptr, end);
                float ey = parseNumber(ptr, end);

                glm::vec2 c1(c1x, c1y);
                glm::vec2 c2(c2x, c2y);
                glm::vec2 e(ex, ey);

                if (!absolute) {
                    c1 += cur;
                    c2 += cur;
                    e += cur;
                }

                path.cubicTo(c1, c2, e);
                prevCubicCtrl = c2; // Save second control point
                cur = e;
                break;
            }
            case 'S': { // Smooth Cubic
                float c2x = parseNumber(ptr, end);
                float c2y = parseNumber(ptr, end);
                float ex = parseNumber(ptr, end);
                float ey = parseNumber(ptr, end);

                glm::vec2 c2(c2x, c2y);
                glm::vec2 e(ex, ey);

                if (!absolute) {
                    c2 += cur;
                    e += cur;
                }

                glm::vec2 c1 = cur;
                // If previous was C or S, reflect control point
                if (lastCmd == 'C' || lastCmd == 'c' || lastCmd == 'S' ||
                    lastCmd == 's') {
                    c1 = reflectPoint(prevCubicCtrl, cur);
                }

                path.cubicTo(c1, c2, e);
                prevCubicCtrl = c2;
                cur = e;
                break;
            }
            case 'Q': { // Quadratic
                float cx = parseNumber(ptr, end);
                float cy = parseNumber(ptr, end);
                float ex = parseNumber(ptr, end);
                float ey = parseNumber(ptr, end);

                glm::vec2 c(cx, cy);
                glm::vec2 e(ex, ey);

                if (!absolute) {
                    c += cur;
                    e += cur;
                }

                path.quadTo(c, e);
                prevQuadCtrl = c;
                cur = e;
                break;
            }
            case 'T': { // Smooth Quadratic
                float ex = parseNumber(ptr, end);
                float ey = parseNumber(ptr, end);
                glm::vec2 e(ex, ey);
                if (!absolute)
                    e += cur;

                glm::vec2 c = cur;
                if (lastCmd == 'Q' || lastCmd == 'q' || lastCmd == 'T' ||
                    lastCmd == 't') {
                    c = reflectPoint(prevQuadCtrl, cur);
                }

                path.quadTo(c, e);
                prevQuadCtrl = c;
                cur = e;
                break;
            }
            case 'A': { // Arc
                float rx = parseNumber(ptr, end);
                float ry = parseNumber(ptr, end);
                float rot = parseNumber(ptr, end);
                float laf = parseNumber(ptr, end); // Large arc flag
                float sf = parseNumber(ptr, end);  // Sweep flag
                float ex = parseNumber(ptr, end);
                float ey = parseNumber(ptr, end);

                glm::vec2 e(ex, ey);
                if (!absolute)
                    e += cur;

                arcToCubics(path,
                            cur,
                            glm::vec2(rx, ry),
                            rot,
                            laf != 0.0f,
                            sf != 0.0f,
                            e);

                // After an arc, the spec isn't strictly clear on "previous
                // control point" for subsequent smooth curves, usually assumed
                // to be the endpoint itself unless we calculate the derivative
                // of the last cubic segment. For simplicity/performance, we
                // treat it as no control point (linear join). If high precision
                // S/T after A is needed, calculate derivative of last cubic.
                cur = e;
                break;
            }
            case 'Z': { // Close
                if (path.commands().empty())
                    break;

                path.close();

                // // Physically close the loop for rendering consistency logic
                // // (Only if not already at start point)
                // if (glm::distance(cur, subPathStart) > 1e-5f) {
                //     path.lineTo(subPathStart);
                // }
                cur = subPathStart;
                break;
            }
            default:
                BESS_WARN("Unsupported SVG command '{}', skipping", cmd);
                break;
            }

            lastCmd = cmd;

            // Reset control points if command wasn't a curve of that specific
            // type
            if (type != 'C' && type != 'S')
                prevCubicCtrl = cur;
            if (type != 'Q' && type != 'T')
                prevQuadCtrl = cur;
        }

        return path;
    }

    void Path2D::translate(const glm::vec2 &pos) {
        auto offset = pos - m_bounds.min;
        if (offset == glm::vec2(0.f)) {
            return; // No translation needed
        }
        m_bounds.valid = false;
        for (PathCommand &command : m_commands) {
            command.p += offset;
            command.control += offset;
            command.control2 += offset;
            includeCommandBounds(command);
        }
        ++m_revision;
    }

    void Path2D::scale(const glm::vec2 &val) {
        if (val == m_currentScale)
            return;

        if (m_ogCommands.empty()) {
            m_ogCommands = m_commands;
        } else {
            m_commands = m_ogCommands;
        }

        m_currentScale = val;
        m_bounds.valid = false;
        for (PathCommand &command : m_commands) {
            command.p *= m_currentScale;
            command.control *= m_currentScale;
            command.control2 *= m_currentScale;
            includeCommandBounds(command);
        }
        ++m_revision;
    }

    void Path2D::normalize(const glm::vec2 &size) {
        glm::vec2 scaleSize = size;
        if (size.x == 0 && size.y == 0) {
            BESS_ASSERT(m_bounds.valid,
                        "Tried normalizing the path with invalid internal "
                        "bounds. Maybe me forgot to pass bounds?");

            scaleSize = m_bounds.size();
        }

        BESS_ASSERT(size.x > 0 && size.y > 0.f,
                    "Invalid size ({}, {}) for normalizing path.",
                    scaleSize.x,
                    scaleSize.y);

        scale(1.f / scaleSize);

        m_ogCommands = m_commands;
        m_ogbounds = m_bounds;
    }

    void Path2D::setPos(const glm::vec2 &pos) {
        translate(pos);
        m_ogCommands = m_commands;
        m_ogbounds = m_bounds;
    }

    void Path2D::clear() noexcept {
        m_commands.clear();
        m_bounds = {};
        ++m_revision;
    }

    void Path2D::reserve(std::size_t commandCount) {
        m_commands.reserve(commandCount);
    }

    void Path2D::setCommands(const std::vector<PathCommand> &commands) {
        m_commands.clear();
        m_bounds = {};
        reserve(commands.size());
        if (commands.empty()) {
            ++m_revision;
            return;
        }
        append(commands);
    }

    Path2D &Path2D::append(const Path2D &path) {
        return append(path.commands());
    }

    Path2D &Path2D::append(const std::vector<PathCommand> &commands) {
        if (commands.empty()) {
            return *this;
        }

        m_commands.insert(m_commands.end(), commands.begin(), commands.end());
        for (const PathCommand &command : commands) {
            includeCommandBounds(command);
        }
        ++m_revision;
        return *this;
    }

    Path2D &Path2D::addCommand(const PathCommand &command) {
        m_commands.push_back(command);
        includeCommandBounds(command);
        ++m_revision;
        return *this;
    }

    Path2D &Path2D::moveTo(const glm::vec2 &pos) {
        return addCommand(PathCommand::moveTo(pos));
    }

    Path2D &Path2D::lineTo(const glm::vec2 &pos) {
        return addCommand(PathCommand::lineTo(pos));
    }

    Path2D &Path2D::lineTo(const glm::vec2 &pos,
                           const PathCommandStroke &stroke) {
        return addCommand(PathCommand::lineTo(pos, stroke));
    }

    Path2D &Path2D::lineTo(const glm::vec2 &pos, float strokeWidth) {
        return lineTo(pos, PathCommandStroke::withWidth(strokeWidth));
    }

    Path2D &
    Path2D::lineTo(const glm::vec2 &pos, float strokeWidth, PickingId id) {
        return lineTo(pos, PathCommandStroke::withWidthAndId(strokeWidth, id));
    }

    Path2D &Path2D::quadTo(const glm::vec2 &control, const glm::vec2 &pos) {
        return addCommand(PathCommand::quadTo(control, pos));
    }

    Path2D &Path2D::quadTo(const glm::vec2 &control,
                           const glm::vec2 &pos,
                           const PathCommandStroke &stroke) {
        return addCommand(PathCommand::quadTo(control, pos, stroke));
    }

    Path2D &Path2D::quadTo(const glm::vec2 &control,
                           const glm::vec2 &pos,
                           float strokeWidth) {
        return quadTo(control, pos, PathCommandStroke::withWidth(strokeWidth));
    }

    Path2D &Path2D::quadTo(const glm::vec2 &control,
                           const glm::vec2 &pos,
                           float strokeWidth,
                           PickingId id) {
        return quadTo(
            control, pos, PathCommandStroke::withWidthAndId(strokeWidth, id));
    }

    Path2D &Path2D::quadraticTo(const glm::vec2 &control,
                                const glm::vec2 &pos) {
        return quadTo(control, pos);
    }

    Path2D &Path2D::quadraticTo(const glm::vec2 &control,
                                const glm::vec2 &pos,
                                const PathCommandStroke &stroke) {
        return quadTo(control, pos, stroke);
    }

    Path2D &Path2D::quadraticTo(const glm::vec2 &control,
                                const glm::vec2 &pos,
                                float strokeWidth) {
        return quadTo(control, pos, strokeWidth);
    }

    Path2D &Path2D::quadraticTo(const glm::vec2 &control,
                                const glm::vec2 &pos,
                                float strokeWidth,
                                PickingId id) {
        return quadTo(control, pos, strokeWidth, id);
    }

    Path2D &Path2D::quadraticBezierTo(const glm::vec2 &control,
                                      const glm::vec2 &pos) {
        return quadTo(control, pos);
    }

    Path2D &Path2D::quadraticBezierTo(const glm::vec2 &control,
                                      const glm::vec2 &pos,
                                      const PathCommandStroke &stroke) {
        return quadTo(control, pos, stroke);
    }

    Path2D &Path2D::quadraticBezierTo(const glm::vec2 &control,
                                      const glm::vec2 &pos,
                                      float strokeWidth) {
        return quadTo(control, pos, strokeWidth);
    }

    Path2D &Path2D::quadraticBezierTo(const glm::vec2 &control,
                                      const glm::vec2 &pos,
                                      float strokeWidth,
                                      PickingId id) {
        return quadTo(control, pos, strokeWidth, id);
    }

    Path2D &Path2D::cubicTo(const glm::vec2 &control1,
                            const glm::vec2 &control2,
                            const glm::vec2 &pos) {
        return addCommand(PathCommand::cubicTo(control1, control2, pos));
    }

    Path2D &Path2D::cubicTo(const glm::vec2 &control1,
                            const glm::vec2 &control2,
                            const glm::vec2 &pos,
                            const PathCommandStroke &stroke) {
        return addCommand(
            PathCommand::cubicTo(control1, control2, pos, stroke));
    }

    Path2D &Path2D::cubicTo(const glm::vec2 &control1,
                            const glm::vec2 &control2,
                            const glm::vec2 &pos,
                            float strokeWidth) {
        return cubicTo(
            control1, control2, pos, PathCommandStroke::withWidth(strokeWidth));
    }

    Path2D &Path2D::cubicTo(const glm::vec2 &control1,
                            const glm::vec2 &control2,
                            const glm::vec2 &pos,
                            float strokeWidth,
                            PickingId id) {
        return cubicTo(control1,
                       control2,
                       pos,
                       PathCommandStroke::withWidthAndId(strokeWidth, id));
    }

    Path2D &Path2D::cubicBezierTo(const glm::vec2 &control1,
                                  const glm::vec2 &control2,
                                  const glm::vec2 &pos) {
        return cubicTo(control1, control2, pos);
    }

    Path2D &Path2D::cubicBezierTo(const glm::vec2 &control1,
                                  const glm::vec2 &control2,
                                  const glm::vec2 &pos,
                                  const PathCommandStroke &stroke) {
        return cubicTo(control1, control2, pos, stroke);
    }

    Path2D &Path2D::cubicBezierTo(const glm::vec2 &control1,
                                  const glm::vec2 &control2,
                                  const glm::vec2 &pos,
                                  float strokeWidth) {
        return cubicTo(control1, control2, pos, strokeWidth);
    }

    Path2D &Path2D::cubicBezierTo(const glm::vec2 &control1,
                                  const glm::vec2 &control2,
                                  const glm::vec2 &pos,
                                  float strokeWidth,
                                  PickingId id) {
        return cubicTo(control1, control2, pos, strokeWidth, id);
    }

    Path2D &Path2D::bezierCurveTo(const glm::vec2 &control1,
                                  const glm::vec2 &control2,
                                  const glm::vec2 &pos) {
        return cubicTo(control1, control2, pos);
    }

    Path2D &Path2D::bezierCurveTo(const glm::vec2 &control1,
                                  const glm::vec2 &control2,
                                  const glm::vec2 &pos,
                                  const PathCommandStroke &stroke) {
        return cubicTo(control1, control2, pos, stroke);
    }

    Path2D &Path2D::bezierCurveTo(const glm::vec2 &control1,
                                  const glm::vec2 &control2,
                                  const glm::vec2 &pos,
                                  float strokeWidth) {
        return cubicTo(control1, control2, pos, strokeWidth);
    }

    Path2D &Path2D::bezierCurveTo(const glm::vec2 &control1,
                                  const glm::vec2 &control2,
                                  const glm::vec2 &pos,
                                  float strokeWidth,
                                  PickingId id) {
        return cubicTo(control1, control2, pos, strokeWidth, id);
    }

    Path2D &Path2D::closePath() { return addCommand(PathCommand::closePath()); }

    Path2D &Path2D::closePath(const PathCommandStroke &stroke) {
        return addCommand(PathCommand::closePath(stroke));
    }

    Path2D &Path2D::closePath(float strokeWidth) {
        return closePath(PathCommandStroke::withWidth(strokeWidth));
    }

    Path2D &Path2D::closePath(float strokeWidth, PickingId id) {
        return closePath(PathCommandStroke::withWidthAndId(strokeWidth, id));
    }

    Path2D &Path2D::close() { return closePath(); }

    Path2D &Path2D::close(const PathCommandStroke &stroke) {
        return closePath(stroke);
    }

    Path2D &Path2D::close(float strokeWidth) { return closePath(strokeWidth); }

    Path2D &Path2D::close(float strokeWidth, PickingId id) {
        return closePath(strokeWidth, id);
    }

    [[nodiscard]] bool Path2D::empty() const noexcept {
        return m_commands.empty();
    }

    [[nodiscard]] std::size_t Path2D::commandCount() const noexcept {
        return m_commands.size();
    }

    [[nodiscard]] std::vector<PathCommand> Path2D::commands() const noexcept {
        return m_commands;
    }

    [[nodiscard]] const PathCommand *Path2D::data() const noexcept {
        return m_commands.data();
    }

    [[nodiscard]] PathBounds Path2D::bounds() const noexcept {
        return m_bounds;
    }

    [[nodiscard]] PathBounds Path2D::ogBounds() const noexcept {
        return m_ogbounds;
    }

    [[nodiscard]] bool Path2D::hasBounds() const noexcept {
        return m_bounds.valid;
    }

    [[nodiscard]] uint64_t Path2D::revision() const noexcept {
        return m_revision;
    }

    void Path2D::includePoint(const glm::vec2 &point) noexcept {
        if (!m_bounds.valid) {
            m_bounds.min = point;
            m_bounds.max = point;
            m_bounds.valid = true;
            return;
        }

        m_bounds.min = glm::min(m_bounds.min, point);
        m_bounds.max = glm::max(m_bounds.max, point);
    }

    void Path2D::includeCommandBounds(const PathCommand &command) noexcept {
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

} // namespace Bess::Core::Renderer
