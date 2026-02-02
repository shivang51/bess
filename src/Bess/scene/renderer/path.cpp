#include "scene/renderer/path.h"

#include <math.h>

#include <math.h>

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

    void Path::skipSpacesAndCommas(const std::string_view &sv, size_t &i) noexcept {
        const size_t n = sv.size();
        while (i < n) {
            char c = sv[i];
            if (c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f')
                ++i;
            else
                break;
        }
    }

    bool Path::isCommandChar(char c) noexcept {
        switch (c) {
        case 'M':
        case 'm':
        case 'Z':
        case 'z':
        case 'L':
        case 'l':
        case 'H':
        case 'h':
        case 'V':
        case 'v':
        case 'C':
        case 'c':
        case 'S':
        case 's':
        case 'Q':
        case 'q':
        case 'T':
        case 't':
        case 'A':
        case 'a':
            return true;
        default:
            return false;
        }
    }

    double Path::parseNumber(const std::string_view &sv, size_t &i, bool &ok) {
        ok = false;
        const size_t n = sv.size();
        if (i >= n)
            return 0.0;
        size_t start = i;

        if (sv[i] == '+' || sv[i] == '-')
            ++i;
        bool anyDigits = false;

        while (i < n && (sv[i] >= '0' && sv[i] <= '9')) {
            ++i;
            anyDigits = true;
        }

        if (i < n && sv[i] == '.') {
            ++i;
            while (i < n && (sv[i] >= '0' && sv[i] <= '9')) {
                ++i;
                anyDigits = true;
            }
        }

        if (i < n && (sv[i] == 'e' || sv[i] == 'E')) {
            size_t expPos = i;
            ++i;
            if (i < n && (sv[i] == '+' || sv[i] == '-'))
                ++i;
            bool expDigits = false;
            while (i < n && (sv[i] >= '0' && sv[i] <= '9')) {
                ++i;
                expDigits = true;
            }
            if (!expDigits) {

                i = expPos;
            }
        }

        if (!anyDigits) {

            i = start;
            return 0.0;
        }

        size_t len = i - start;

        len = std::min<size_t>(len, 128);
        char buf[129];
        for (size_t k = 0; k < len; ++k)
            buf[k] = sv[start + k];
        buf[len] = '\0';
        char *endptr = nullptr;
        const double val = std::strtod(buf, &endptr);

        ok = true;
        return val;
    }

    double Path::dist(double ax, double ay, double bx, double by) noexcept {
        return std::hypot(bx - ax, by - ay);
    }

    double Path::distancePointToLine(double px, double py, double ax, double ay, double bx, double by) noexcept {
        double vx = bx - ax;
        double vy = by - ay;
        double wx = px - ax;
        double wy = py - ay;
        double denom = (vx * vx) + (vy * vy);
        if (denom == 0.0)
            return std::hypot(px - ax, py - ay);
        double t = (vx * wx + vy * wy) / denom;
        double projx = ax + (t * vx);
        double projy = ay + (t * vy);
        return std::hypot(px - projx, py - projy);
    }

    bool Path::isDegenerateCubic(double sx, double sy,
                                 double c1x, double c1y,
                                 double c2x, double c2y,
                                 double ex, double ey,
                                 double rel_tol, double abs_tol) noexcept {
        double d1 = distancePointToLine(c1x, c1y, sx, sy, ex, ey);
        double d2 = distancePointToLine(c2x, c2y, sx, sy, ex, ey);
        double h1 = dist(c1x, c1y, sx, sy);
        double h2 = dist(c2x, c2y, ex, ey);
        double seg = dist(sx, sy, ex, ey);
        double rel = rel_tol * (seg > 0.0 ? seg : 1.0);
        double tol = std::max(abs_tol, rel);
        if (d1 <= tol && d2 <= tol && h1 <= tol && h2 <= tol)
            return true;
        if (std::abs(c1x - sx) <= abs_tol && std::abs(c1y - sy) <= abs_tol &&
            std::abs(c2x - ex) <= abs_tol && std::abs(c2y - ey) <= abs_tol)
            return true;
        return false;
    }

    void Path::arcToCubicsAppend(std::vector<PathCommand> &cmds,
                                 double x1, double y1,
                                 double rx, double ry, double phi_deg,
                                 bool large_arc, bool sweep,
                                 double x2, double y2) {

        if (rx == 0.0 || ry == 0.0) {
            PathCommand c{};
            c.kind = PathCommand::Kind::Cubic;
            c.cubic.c1 = glm::vec2((float)x1, (float)y1);
            c.cubic.c2 = glm::vec2((float)x2, (float)y2);
            c.cubic.p = glm::vec2((float)x2, (float)y2);
            cmds.push_back(c);
            return;
        }

        double phi = std::fmod(phi_deg, 360.0) * M_PI / 180.0;
        double cos_phi = std::cos(phi), sin_phi = std::sin(phi);

        double dx2 = (x1 - x2) / 2.0;
        double dy2 = (y1 - y2) / 2.0;
        double x1p = (cos_phi * dx2) + (sin_phi * dy2);
        double y1p = (-sin_phi * dx2) + (cos_phi * dy2);

        double rx_abs = std::abs(rx);
        double ry_abs = std::abs(ry);
        if (rx_abs == 0.0 || ry_abs == 0.0) {
            PathCommand c{};
            c.kind = PathCommand::Kind::Cubic;
            c.cubic.c1 = glm::vec2((float)x1, (float)y1);
            c.cubic.c2 = glm::vec2((float)x2, (float)y2);
            c.cubic.p = glm::vec2((float)x2, (float)y2);
            cmds.push_back(c);
            return;
        }

        double x1p2 = x1p * x1p;
        double y1p2 = y1p * y1p;
        double rx2 = rx_abs * rx_abs;
        double ry2 = ry_abs * ry_abs;
        double lam = (x1p2 / rx2) + (y1p2 / ry2);
        if (lam > 1.0) {
            double s = std::sqrt(lam);
            rx_abs *= s;
            ry_abs *= s;
            rx2 = rx_abs * rx_abs;
            ry2 = ry_abs * ry_abs;
        }

        double num = (rx2 * ry2) - (rx2 * y1p2) - (ry2 * x1p2);
        double denom = (rx2 * y1p2) + (ry2 * x1p2);
        double cc = 0.0;
        if (denom != 0.0 && num > 0.0) {
            cc = std::sqrt(num / denom);
        } else {
            cc = 0.0;
        }
        if (large_arc == sweep)
            cc = -cc;

        double cxp = cc * (rx_abs * y1p / ry_abs);
        double cyp = cc * (-ry_abs * x1p / rx_abs);

        double cx = (cos_phi * cxp) - (sin_phi * cyp) + ((x1 + x2) / 2.0);
        double cy = (sin_phi * cxp) + (cos_phi * cyp) + ((y1 + y2) / 2.0);

        auto angle = [](double ux, double uy, double vx, double vy) {
            double dot = (ux * vx) + uy * vy;
            double det = ux * vy - uy * vx;
            return std::atan2(det, dot);
        };

        double ux = (x1p - cxp) / rx_abs;
        double uy = (y1p - cyp) / ry_abs;
        double vx = (-x1p - cxp) / rx_abs;
        double vy = (-y1p - cyp) / ry_abs;

        double theta1 = std::atan2(uy, ux);
        double delta = angle(ux, uy, vx, vy);

        if (!sweep && delta > 0)
            delta -= 2.0 * M_PI;
        else if (sweep && delta < 0)
            delta += 2.0 * M_PI;

        int segs = std::max(1, (int)std::ceil(std::abs(delta) / (M_PI / 2.0)));
        double deltaPer = delta / segs;

        for (int i = 0; i < segs; ++i) {
            double t1 = theta1 + i * deltaPer;
            double t2 = t1 + deltaPer;

            auto point = [&](double t) {
                double ct = std::cos(t);
                double st = std::sin(t);
                double x = cx + rx_abs * cos_phi * ct - ry_abs * sin_phi * st;
                double y = cy + rx_abs * sin_phi * ct + ry_abs * cos_phi * st;
                return std::pair<double, double>{x, y};
            };
            auto deriv = [&](double t) {
                double ct = std::cos(t);
                double st = std::sin(t);
                double dxdt = -rx_abs * cos_phi * st - ry_abs * sin_phi * ct;
                double dydt = -rx_abs * sin_phi * st + ry_abs * cos_phi * ct;
                return std::pair<double, double>{dxdt, dydt};
            };

            auto p1 = point(t1);
            auto p2 = point(t2);
            auto d1 = deriv(t1);
            auto d2 = deriv(t2);

            double dt = t2 - t1;
            if (std::abs(dt) < 1e-12) {
                PathCommand c{};
                c.kind = PathCommand::Kind::Cubic;
                c.cubic.c1 = glm::vec2((float)p1.first, (float)p1.second);
                c.cubic.c2 = glm::vec2((float)p2.first, (float)p2.second);
                c.cubic.p = glm::vec2((float)p2.first, (float)p2.second);
                cmds.push_back(c);
                continue;
            }

            double k = (4.0 / 3.0) * std::tan(dt / 4.0);
            double c1x = p1.first + k * d1.first;
            double c1y = p1.second + k * d1.second;
            double c2x = p2.first - k * d2.first;
            double c2y = p2.second - k * d2.second;

            PathCommand c{};
            c.kind = PathCommand::Kind::Cubic;
            c.cubic.c1 = glm::vec2((float)c1x, (float)c1y);
            c.cubic.c2 = glm::vec2((float)c2x, (float)c2y);
            c.cubic.p = glm::vec2((float)p2.first, (float)p2.second);
            cmds.push_back(c);
        }
    }

    Path Path::fromSvgString(const std::string &svgData) {
        Path path;
        std::string_view sv(svgData);
        const size_t n = sv.size();
        size_t i = 0;

        std::vector<PathCommand> intermediate;
        intermediate.reserve(std::min<size_t>(256, n / 4));

        double cur_x = 0.0, cur_y = 0.0;
        double sub_x = 0.0, sub_y = 0.0;
        bool have_sub = false;

        char prev_cmd = 0;
        glm::dvec2 prev_cubic_ctrl(0.0, 0.0);
        bool have_prev_cubic = false;
        glm::dvec2 prev_quad_ctrl(0.0, 0.0);
        bool have_prev_quad = false;

        auto make_point = [&](double x, double y, bool relative) {
            if (relative)
                return std::pair<double, double>{cur_x + x, cur_y + y};
            else
                return std::pair<double, double>{x, y};
        };

        skipSpacesAndCommas(sv, i);
        while (i < n) {
            skipSpacesAndCommas(sv, i);
            if (i >= n)
                break;
            char tok = sv[i];

            char cmd = 0;
            if (isCommandChar(tok)) {
                cmd = sv[i++];
            } else {
                if (prev_cmd == 0)
                    throw std::runtime_error("Path data must begin with command");
                cmd = prev_cmd;
            }
            bool absolute = (cmd >= 'A' && cmd <= 'Z');
            char C = (char)std::toupper(cmd);

            auto peekIsNumber = [&](size_t idx) -> bool {
                size_t j = idx;
                skipSpacesAndCommas(sv, j);
                if (j >= n)
                    return false;
                char c = sv[j];

                return (c == '+' || c == '-' || c == '.' || (c >= '0' && c <= '9'));
            };

            if (C == 'Z') {
                PathCommand pc{};
                pc.kind = PathCommand::Kind::Line;

                pc.id = UINT64_MAX;
                intermediate.push_back(pc);

                have_prev_cubic = have_prev_quad = false;
                prev_cmd = cmd;
                continue;
            }

            while (true) {
                skipSpacesAndCommas(sv, i);
                if (i >= n)
                    break;
                if (isCommandChar(sv[i]))
                    break;

                bool ok = false;
                if (C == 'M') {
                    double x = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    skipSpacesAndCommas(sv, i);
                    double y = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed M command");
                    auto p = make_point(x, y, !absolute);
                    PathCommand pc{};
                    pc.kind = PathCommand::Kind::Move;
                    pc.move.p = glm::vec2((float)p.first, (float)p.second);
                    intermediate.push_back(pc);
                    cur_x = p.first;
                    cur_y = p.second;
                    sub_x = cur_x;
                    sub_y = cur_y;
                    have_sub = true;
                    have_prev_cubic = have_prev_quad = false;

                    prev_cmd = absolute ? 'L' : 'l';
                    C = 'L';
                    continue;
                } else if (C == 'L') {
                    double x = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    skipSpacesAndCommas(sv, i);
                    double y = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed L command");
                    auto p = make_point(x, y, !absolute);
                    PathCommand pc{};
                    pc.kind = PathCommand::Kind::Line;
                    pc.line.p = glm::vec2((float)p.first, (float)p.second);
                    intermediate.push_back(pc);
                    cur_x = p.first;
                    cur_y = p.second;
                    have_prev_cubic = have_prev_quad = false;
                    prev_cmd = cmd;
                    continue;
                } else if (C == 'H') {
                    double x = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    double px = absolute ? x : (cur_x + x);
                    PathCommand pc{};
                    pc.kind = PathCommand::Kind::Line;
                    pc.line.p = glm::vec2((float)px, (float)cur_y);
                    intermediate.push_back(pc);
                    cur_x = px;
                    have_prev_cubic = have_prev_quad = false;
                    prev_cmd = cmd;
                    continue;
                } else if (C == 'V') {
                    double y = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    double py = absolute ? y : (cur_y + y);
                    PathCommand pc{};
                    pc.kind = PathCommand::Kind::Line;
                    pc.line.p = glm::vec2((float)cur_x, (float)py);
                    intermediate.push_back(pc);
                    cur_y = py;
                    have_prev_cubic = have_prev_quad = false;
                    prev_cmd = cmd;
                    continue;
                } else if (C == 'C') {
                    double x1 = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    skipSpacesAndCommas(sv, i);
                    double y1 = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed C");
                    skipSpacesAndCommas(sv, i);
                    double x2 = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed C");
                    skipSpacesAndCommas(sv, i);
                    double y2 = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed C");
                    skipSpacesAndCommas(sv, i);
                    double x = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed C");
                    skipSpacesAndCommas(sv, i);
                    double y = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed C");

                    auto p1 = make_point(x1, y1, !absolute);
                    auto p2 = make_point(x2, y2, !absolute);
                    auto p = make_point(x, y, !absolute);

                    PathCommand pc{};
                    pc.kind = PathCommand::Kind::Cubic;
                    pc.cubic.c1 = glm::vec2((float)p1.first, (float)p1.second);
                    pc.cubic.c2 = glm::vec2((float)p2.first, (float)p2.second);
                    pc.cubic.p = glm::vec2((float)p.first, (float)p.second);
                    intermediate.push_back(pc);
                    prev_cubic_ctrl = glm::dvec2(p2.first, p2.second);
                    have_prev_cubic = true;
                    have_prev_quad = false;
                    cur_x = p.first;
                    cur_y = p.second;
                    prev_cmd = cmd;
                    continue;
                } else if (C == 'S') {
                    double x2 = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    skipSpacesAndCommas(sv, i);
                    double y2 = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed S");
                    skipSpacesAndCommas(sv, i);
                    double x = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed S");
                    skipSpacesAndCommas(sv, i);
                    double y = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed S");

                    double c1x = NAN, c1y = NAN;
                    if (have_prev_cubic && (prev_cmd == 'C' || prev_cmd == 'c' || prev_cmd == 'S' || prev_cmd == 's')) {
                        c1x = (2.0 * cur_x) - prev_cubic_ctrl.x;
                        c1y = (2.0 * cur_y) - prev_cubic_ctrl.y;
                    } else {
                        c1x = cur_x;
                        c1y = cur_y;
                    }

                    auto p2 = make_point(x2, y2, !absolute);
                    auto p = make_point(x, y, !absolute);

                    PathCommand pc{};
                    pc.kind = PathCommand::Kind::Cubic;
                    pc.cubic.c1 = glm::vec2((float)c1x, (float)c1y);
                    pc.cubic.c2 = glm::vec2((float)p2.first, (float)p2.second);
                    pc.cubic.p = glm::vec2((float)p.first, (float)p.second);
                    intermediate.push_back(pc);
                    prev_cubic_ctrl = glm::dvec2(p2.first, p2.second);
                    have_prev_cubic = true;
                    have_prev_quad = false;
                    cur_x = p.first;
                    cur_y = p.second;
                    prev_cmd = cmd;
                    continue;
                } else if (C == 'Q') {
                    double cx = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    skipSpacesAndCommas(sv, i);
                    double cy = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed Q");
                    skipSpacesAndCommas(sv, i);
                    double x = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed Q");
                    skipSpacesAndCommas(sv, i);
                    double y = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed Q");

                    auto cp = make_point(cx, cy, !absolute);
                    auto p = make_point(x, y, !absolute);
                    PathCommand pc{};
                    pc.kind = PathCommand::Kind::Quad;
                    pc.quad.c = glm::vec2((float)cp.first, (float)cp.second);
                    pc.quad.p = glm::vec2((float)p.first, (float)p.second);
                    intermediate.push_back(pc);
                    prev_quad_ctrl = glm::dvec2(cp.first, cp.second);
                    have_prev_quad = true;
                    have_prev_cubic = false;
                    cur_x = p.first;
                    cur_y = p.second;
                    prev_cmd = cmd;
                    continue;
                } else if (C == 'T') {
                    double x = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    skipSpacesAndCommas(sv, i);
                    double y = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed T");

                    double cpx = NAN, cpy = NAN;
                    if (have_prev_quad && (prev_cmd == 'Q' || prev_cmd == 'q' || prev_cmd == 'T' || prev_cmd == 't')) {
                        cpx = (2.0 * cur_x) - prev_quad_ctrl.x;
                        cpy = (2.0 * cur_y) - prev_quad_ctrl.y;
                    } else {
                        cpx = cur_x;
                        cpy = cur_y;
                    }
                    auto p = make_point(x, y, !absolute);
                    PathCommand pc{};
                    pc.kind = PathCommand::Kind::Quad;
                    pc.quad.c = glm::vec2((float)cpx, (float)cpy);
                    pc.quad.p = glm::vec2((float)p.first, (float)p.second);
                    intermediate.push_back(pc);
                    prev_quad_ctrl = glm::dvec2(cpx, cpy);
                    have_prev_quad = true;
                    have_prev_cubic = false;
                    cur_x = p.first;
                    cur_y = p.second;
                    prev_cmd = cmd;
                    continue;
                } else if (C == 'A') {
                    double rx = parseNumber(sv, i, ok);
                    if (!ok)
                        break;
                    skipSpacesAndCommas(sv, i);
                    double ry = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed A");
                    skipSpacesAndCommas(sv, i);
                    double xrot = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed A");
                    skipSpacesAndCommas(sv, i);
                    double laf = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed A");
                    skipSpacesAndCommas(sv, i);
                    double sf = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed A");
                    skipSpacesAndCommas(sv, i);
                    double x = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed A");
                    skipSpacesAndCommas(sv, i);
                    double y = parseNumber(sv, i, ok);
                    if (!ok)
                        throw std::runtime_error("Malformed A");

                    auto p = make_point(x, y, !absolute);

                    arcToCubicsAppend(intermediate, cur_x, cur_y, rx, ry, xrot, (laf != 0.0), (sf != 0.0), p.first, p.second);

                    if (!intermediate.empty()) {

                        const PathCommand &last = intermediate.back();
                        if (last.kind == PathCommand::Kind::Cubic) {
                            cur_x = last.cubic.p.x;
                            cur_y = last.cubic.p.y;
                            prev_cubic_ctrl = glm::dvec2(last.cubic.c2.x, last.cubic.c2.y);
                            have_prev_cubic = true;
                            have_prev_quad = false;
                        }
                    }
                    prev_cmd = cmd;
                    continue;
                } else {

                    throw std::runtime_error(std::string("Unsupported command ") + C);
                }
            }

            prev_cmd = cmd;
        }

        std::vector<PathCommand> simplified;
        simplified.reserve(intermediate.size());
        glm::vec2 lastPt(0.0f);
        bool haveLast = false;
        glm::vec2 subStart(0.0f);
        bool haveSubStart = false;

        for (const auto &e : intermediate) {
            if (e.id == UINT64_MAX) {

                if (haveSubStart) {

                    if (!haveLast || std::abs(lastPt.x - subStart.x) > 1e-12f || std::abs(lastPt.y - subStart.y) > 1e-12f) {
                        PathCommand pc{};
                        pc.kind = PathCommand::Kind::Line;
                        pc.line.p = subStart;
                        simplified.push_back(pc);
                    }
                }
                haveLast = true;
                lastPt = subStart;
                continue;
            }

            switch (e.kind) {
            case PathCommand::Kind::Move: {
                glm::vec2 p = e.move.p;
                if (!haveLast || (std::abs(p.x - lastPt.x) > 1e-12f || std::abs(p.y - lastPt.y) > 1e-12f)) {
                    simplified.push_back(e);
                    lastPt = p;
                    haveLast = true;
                    subStart = p;
                    haveSubStart = true;
                } else {

                    subStart = p;
                    haveSubStart = true;
                }
                break;
            }
            case PathCommand::Kind::Line: {
                glm::vec2 p = e.line.p;
                if (!haveLast || std::abs(p.x - lastPt.x) > 1e-12f || std::abs(p.y - lastPt.y) > 1e-12f) {
                    simplified.push_back(e);
                    lastPt = p;
                    haveLast = true;
                }
                break;
            }
            case PathCommand::Kind::Cubic: {
                glm::vec2 c1 = e.cubic.c1;
                glm::vec2 c2 = e.cubic.c2;
                glm::vec2 p = e.cubic.p;
                glm::dvec2 s = haveLast ? glm::dvec2(lastPt.x, lastPt.y) : glm::dvec2(0.0, 0.0);
                if (isDegenerateCubic(s.x, s.y, c1.x, c1.y, c2.x, c2.y, p.x, p.y, 1e-6, 1e-3)) {

                    if (std::abs(p.x - s.x) < 1e-12 && std::abs(p.y - s.y) < 1e-12) {
                        lastPt = p;
                        haveLast = true;

                    } else {
                        PathCommand pc{};
                        pc.kind = PathCommand::Kind::Line;
                        pc.line.p = p;
                        simplified.push_back(pc);
                        lastPt = p;
                        haveLast = true;
                    }
                } else {
                    simplified.push_back(e);
                    lastPt = p;
                    haveLast = true;
                }
                break;
            }
            case PathCommand::Kind::Quad: {
                glm::vec2 c = e.quad.c;
                glm::vec2 p = e.quad.p;
                glm::dvec2 s = haveLast ? glm::dvec2(lastPt.x, lastPt.y) : glm::dvec2(0.0, 0.0);
                double dctrl = distancePointToLine(c.x, c.y, s.x, s.y, p.x, p.y);
                if (dctrl <= std::max(1e-3, 1e-6 * dist(s.x, s.y, p.x, p.y))) {
                    if (std::abs(p.x - s.x) >= 1e-12 || std::abs(p.y - s.y) >= 1e-12) {
                        PathCommand pc{};
                        pc.kind = PathCommand::Kind::Line;
                        pc.line.p = p;
                        simplified.push_back(pc);
                        lastPt = p;
                        haveLast = true;
                    } else {
                        lastPt = p;
                        haveLast = true;
                    }
                } else {
                    simplified.push_back(e);
                    lastPt = p;
                    haveLast = true;
                }
                break;
            }
            }
        }

        path.setCommands({});
        std::vector<PathCommand> finalCmds;
        finalCmds.reserve(simplified.size());
        for (const auto &e : simplified) {
            finalCmds.push_back(e);
            switch (e.kind) {
            case PathCommand::Kind::Move:
                path.moveTo(e.move.p);
                break;
            case PathCommand::Kind::Line:
                path.lineTo(e.line.p);
                break;
            case PathCommand::Kind::Quad:
                path.quadTo(e.quad.c, e.quad.p);
                break;
            case PathCommand::Kind::Cubic:
                path.cubicTo(e.cubic.c1, e.cubic.c2, e.cubic.p);
                break;
            }
        }
        path.setCommands(finalCmds);

        return path;
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
} // namespace Bess::Renderer
