// license:BSD-3-Clause
// Screen-space segment measurements and optional exact-duplicate filtering.
#ifndef MAME_VIDEO_VECTRX_SEGMENT_STATS_H
#define MAME_VIDEO_VECTRX_SEGMENT_STATS_H
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace vectrx_segment_stats {
struct segment { int x0, y0, x1, y1; uint32_t color; };
using edge_key = std::tuple<int, int, int, int, uint32_t>;

inline edge_key key_for(const segment &s)
{
    int x0 = s.x0, y0 = s.y0, x1 = s.x1, y1 = s.y1;
    if (std::tie(x1, y1) < std::tie(x0, y0)) { std::swap(x0, x1); std::swap(y0, y1); }
    return {x0, y0, x1, y1, s.color};
}

// Call in submission order. Only subsequent copies are rejected, so the
// caller retains the first segment's direction and every surviving position.
class duplicate_filter {
public:
    void clear() { m_seen.clear(); }
    bool accept(const segment &s) { return m_seen.insert(key_for(s)).second; }
private:
    std::set<edge_key> m_seen;
};

// Subtract previously drawn intervals on exactly the same integer line and
// colour. Surviving pieces keep the incoming direction and traversal order.
// Use pre-wire integer endpoints: reconstructed endpoints must be exact, not
// inversions of lossy wire quantization. No tolerance or near-line snapping.
class overlap_filter {
public:
    void clear() { m_covered.clear(); }
    std::vector<segment> trim(const segment &s)
    {
        int dx = s.x1 - s.x0, dy = s.y1 - s.y0;
        const int divisor = std::gcd(dx, dy);
        if (!divisor) return {s}; // Preserve intentional point/dwell geometry.
        int ux = dx / divisor, uy = dy / divisor;
        if (ux < 0 || (ux == 0 && uy < 0)) { ux = -ux; uy = -uy; }
        const int64_t norm = int64_t(ux) * ux + int64_t(uy) * uy;
        const int64_t from = int64_t(ux) * s.x0 + int64_t(uy) * s.y0;
        const int64_t to = int64_t(ux) * s.x1 + int64_t(uy) * s.y1;
        const int64_t lo = std::min(from, to), hi = std::max(from, to);
        auto &covered = m_covered[{ux, uy, int64_t(ux) * s.y0 - int64_t(uy) * s.x0, s.color}];
        std::vector<segment> pieces;
        auto append = [&](int64_t a, int64_t b) {
            auto point = [&](int64_t t) {
                const int64_t steps = (t - from) / norm;
                return std::pair<int, int>{int(s.x0 + steps * ux), int(s.y0 + steps * uy)};
            };
            if (from > to) std::swap(a, b);
            const auto first = point(a), last = point(b);
            pieces.push_back({first.first, first.second, last.first, last.second, s.color});
        };
        int64_t cursor = lo;
        for (const auto &run : covered) {
            if (run.second <= cursor) continue;
            if (run.first >= hi) break;
            if (run.first > cursor) append(cursor, std::min(hi, run.first));
            cursor = std::max(cursor, run.second);
            if (cursor >= hi) break;
        }
        if (cursor < hi) append(cursor, hi);
        if (from > to) std::reverse(pieces.begin(), pieces.end());

        // Maintain sorted, disjoint coverage, coalescing touching intervals.
        std::vector<std::pair<int64_t, int64_t>> merged;
        int64_t start = lo, end = hi;
        bool inserted = false;
        for (const auto &run : covered) {
            if (run.second < start) merged.push_back(run);
            else if (run.first > end) {
                if (!inserted) { merged.emplace_back(start, end); inserted = true; }
                merged.push_back(run);
            } else { start = std::min(start, run.first); end = std::max(end, run.second); }
        }
        if (!inserted) merged.emplace_back(start, end);
        covered = std::move(merged);
        return pieces;
    }
private:
    std::map<std::tuple<int, int, int64_t, uint32_t>, std::vector<std::pair<int64_t, int64_t>>> m_covered;
};
struct result {
    unsigned segments = 0, duplicates = 0, zero_length = 0, connected = 0;
    double visible_length = 0, covered_length = 0, blank_travel = 0;
};

// Coordinates are final integer coordinates. Coverage is exact collinear union
// per output colour; crossings and merely nearby edges are not overlaps.
inline result measure(const std::vector<segment> &segments)
{
    result r;
    using line = std::tuple<int, int, int64_t, uint32_t>;
    std::map<line, std::vector<std::pair<int64_t, int64_t>>> intervals;
    duplicate_filter seen;
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto &s = segments[i];
        ++r.segments;
        if (i) {
            const auto &previous = segments[i - 1];
            r.blank_travel += std::hypot(double(s.x0 - previous.x1), double(s.y0 - previous.y1));
            if (s.x0 == previous.x1 && s.y0 == previous.y1) ++r.connected;
        }
        int x0 = s.x0, y0 = s.y0, x1 = s.x1, y1 = s.y1;
        if (std::tie(x1, y1) < std::tie(x0, y0)) { std::swap(x0, x1); std::swap(y0, y1); }
        if (!seen.accept(s)) ++r.duplicates;
        const int dx = x1 - x0, dy = y1 - y0;
        const int divisor = std::gcd(dx, dy);
        if (!divisor) { ++r.zero_length; continue; }
        r.visible_length += std::hypot(double(dx), double(dy));
        const int ux = dx / divisor, uy = dy / divisor;
        const int64_t offset = int64_t(ux) * y0 - int64_t(uy) * x0;
        const int64_t start = int64_t(ux) * x0 + int64_t(uy) * y0;
        const int64_t end = int64_t(ux) * x1 + int64_t(uy) * y1;
        intervals[{ux, uy, offset, s.color}].emplace_back(start, end);
    }
    for (auto &entry : intervals) {
        auto &runs = entry.second;
        std::sort(runs.begin(), runs.end());
        int64_t start = runs.front().first, end = runs.front().second, total = 0;
        for (size_t i = 1; i < runs.size(); ++i) {
            if (runs[i].first <= end) end = std::max(end, runs[i].second);
            else { total += end - start; start = runs[i].first; end = runs[i].second; }
        }
        total += end - start;
        r.covered_length += double(total) / std::hypot(double(std::get<0>(entry.first)), double(std::get<1>(entry.first)));
    }
    return r;
}
} // namespace vectrx_segment_stats
#endif
