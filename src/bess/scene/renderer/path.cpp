#include "scene/renderer/path.h"

#include <cmath>

#include "scene/renderer/vulkan/path_renderer.h"
#include <algorithm>

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

        const auto cacheKey = std::format("{}_{}", m_currentScale.x, m_currentScale.y);

        const auto &itr = m_scaledContoursCache.find(cacheKey);
        if (itr != m_scaledContoursCache.end()) {
            return itr->second;
        }

        formContours();
        assert(!m_contours.empty());
        m_scaledContoursCache[cacheKey] = m_contours;

        return m_contours;
    }

    void Path::formContours() {
        glm::vec3 currentPos = glm::vec3(0.f);
        std::vector<PathPoint> points = {PathPoint{currentPos, 1.f, 0}};
        for (auto &c : m_cmds) {
            using Kind = Renderer::Path::PathCommand::Kind;

            const float weight = c.weight > 0.f ? c.weight : m_strokeWidth;

            switch (c.kind) {
            case Kind::Move: {
                const auto &pos = c.move.p;
                currentPos = {c.move.p, c.z};
                auto prevPoint = points.back();
                if (points.size() == 1) {
                    points[0] = PathPoint{{pos, c.z}, weight, c.id};
                    continue;
                }

                m_contours.emplace_back(std::move(points));
                points.clear();
                points.emplace_back(PathPoint{{pos, c.z}, weight, c.id});
            } break;
            case Kind::Line: {
                points.emplace_back(PathPoint{{c.line.p, c.z}, weight, c.id});
                currentPos = {c.line.p, c.z};
            } break;
            case Kind::Quad: {
                auto positions = Renderer2D::Vulkan::PathRenderer::generateQuadBezierPoints(currentPos, c.quad.c, {c.quad.p, c.z});
                points.reserve(points.size() + positions.size());
                for (const auto &pos : positions) {
                    points.emplace_back(PathPoint{pos, weight, c.id});
                }
                currentPos = {c.quad.p, c.z};
            } break;
            case Kind::Cubic: {
                auto positions = Renderer2D::Vulkan::PathRenderer::generateCubicBezierPoints(currentPos,
                                                                                             c.cubic.c1, c.cubic.c2,
                                                                                             {c.cubic.p, c.z});
                points.reserve(points.size() + positions.size());
                for (const auto &pos : positions) {
                    points.emplace_back(PathPoint{pos, weight, c.id});
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

    void Path::normalize(const glm::vec2 &size) {
        scale({1.f / size.x, 1.f / size.y}, true);
    }

    bool Path::scale(const glm::vec2 &factor, bool overrideOriginal) {
        if (factor == m_currentScale) {
            return false;
        }

        m_currentScale = factor;

        const auto cacheKey = std::format("{}_{}", m_currentScale.x, m_currentScale.y);

        if (!overrideOriginal && m_scaledCmdsCache.contains(cacheKey)) {
            m_cmds = m_scaledCmdsCache.at(cacheKey);
            m_currentScale = factor;
            m_contours.clear();
            if (m_scaledContoursCache.contains(cacheKey)) {
                m_contours = m_scaledContoursCache.at(cacheKey);
                m_lowestPos = m_ogLowestPos * factor;
            }
            return m_contours.empty();
        }

        if (m_ogCmds.empty()) {
            m_ogCmds = m_cmds;
            m_ogLowestPos = m_lowestPos;
        } else {
            m_cmds = m_ogCmds;
            m_lowestPos = m_ogLowestPos;
        }

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

        m_lowestPos *= factor;

        m_scaledCmdsCache[cacheKey] = m_cmds;

        if (overrideOriginal) {
            m_ogCmds = m_cmds;
            m_ogLowestPos = m_lowestPos;
        }

        return true;
    }

    PathProperties &Path::getPropsRef() {
        return m_props;
    }

    const PathProperties &Path::getProps() const {
        return m_props;
    }

    void Path::setProps(const PathProperties &props) {
        m_props = props;
    }

    float Path::getStrokeWidth() const {
        return m_strokeWidth;
    }

    void Path::setStrokeWidth(float width) {
        if (m_strokeWidth == width)
            return;
        m_strokeWidth = width;
        m_contours.clear();
        m_scaledContoursCache.clear();
    }

    void Path::setBounds(const glm::vec2 &bounds) {
        m_bounds = bounds;
    }

    const glm::vec2 &Path::getBounds() const {
        return m_bounds;
    }

    void Path::setLowestPos(const glm::vec2 &pos) {
        m_lowestPos = pos;
    }

    const glm::vec2 &Path::getLowestPos() const {
        return m_lowestPos;
    }

    Path Path::copy() const {
        Path newPath;
        newPath.m_cmds = m_cmds;
        newPath.m_props = m_props;
        newPath.m_strokeWidth = m_strokeWidth;
        newPath.m_bounds = m_bounds;
        newPath.m_lowestPos = m_lowestPos;
        return newPath;
    }

    float Path::parseNumber(const char *&ptr, const char *end) {
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

    // --- Helper: Skip delimiters ---
    void Path::skipSeparators(const char *&ptr, const char *end) {
        while (ptr < end && (*ptr == ' ' || *ptr == ',' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
            ptr++;
        }
    }

    // --- Helper: Reflection ---
    glm::vec2 Path::reflectPoint(glm::vec2 p, glm::vec2 pivot) {
        return 2.0f * pivot - p;
    }

    // --- Helper: Arc to Cubics ---
    void Path::arcToCubics(Path &path, glm::vec2 cur, glm::vec2 rxry, float phi_deg, bool large_arc, bool sweep, glm::vec2 end) {
        // Ported from Python implementation
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
        int segs = std::max(1, (int)std::ceil(std::abs(delta) / (glm::pi<float>() / 2.0f)));
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
                return glm::vec2(
                    (-rx * cos_phi * st) - (ry * sin_phi * ct),
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

    Path Path::fromSvgString(const std::string &svgData) {
        Path path;
        const char *ptr = svgData.data();
        const char *end = ptr + svgData.size();

        glm::vec2 cur(0.0f);
        glm::vec2 subPathStart(0.0f);

        // Control points for smooth curves (S/s, T/t)
        glm::vec2 prevCubicCtrl(0.0f);
        glm::vec2 prevQuadCtrl(0.0f);

        // Track previous command type to determine control point inference
        char lastCmd = 0;

        // Command tracking
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
            char type = (char)(absolute ? cmd : (cmd - 'a' + 'A')); // Normalize to Uppercase

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
                if (lastCmd == 'C' || lastCmd == 'c' || lastCmd == 'S' || lastCmd == 's') {
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
                if (lastCmd == 'Q' || lastCmd == 'q' || lastCmd == 'T' || lastCmd == 't') {
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

                arcToCubics(path, cur, glm::vec2(rx, ry), rot, laf != 0.0f, sf != 0.0f, e);

                // After an arc, the spec isn't strictly clear on "previous control point"
                // for subsequent smooth curves, usually assumed to be the endpoint itself
                // unless we calculate the derivative of the last cubic segment.
                // For simplicity/performance, we treat it as no control point (linear join).
                // If high precision S/T after A is needed, calculate derivative of last cubic.
                cur = e;
                break;
            }
            case 'Z': { // Close
                if (path.getCmds().empty())
                    break;

                // Set the is_closed property
                path.getPropsRef().isClosed = true;

                // Physically close the loop for rendering consistency logic
                // (Only if not already at start point)
                if (glm::distance(cur, subPathStart) > 1e-5f) {
                    path.lineTo(subPathStart);
                }
                cur = subPathStart;
                break;
            }
            default:
                // Unknown command, skip or throw
                break;
            }

            lastCmd = cmd;

            // Reset control points if command wasn't a curve of that specific type
            if (type != 'C' && type != 'S')
                prevCubicCtrl = cur;
            if (type != 'Q' && type != 'T')
                prevQuadCtrl = cur;
        }

        return path;
    }
} // namespace Bess::Renderer
