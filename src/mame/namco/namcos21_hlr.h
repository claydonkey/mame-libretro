// license:BSD-3-Clause
// Screen-space hidden-line extraction using the System 21 rasterizer's depth.
#ifndef MAME_NAMCO_NAMCOS21_HLR_H
#define MAME_NAMCO_NAMCOS21_HLR_H
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <tuple>
#include <vector>

namespace namcos21_hlr {
constexpr unsigned max_quads = 32768;
constexpr unsigned max_segments = 4096;
struct vertex { double x, y, z; };
struct quad { std::array<vertex, 4> v; uint16_t pen; };
struct segment { vertex a, b; uint16_t pen; };

inline vertex interpolate(const vertex &a, const vertex &b, double t)
{
	return { a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t };
}

// Clip before sampling: DSP coordinates can extend far beyond the viewport.
inline bool clip(vertex &a, vertex &b, int width, int height)
{
	double lo = 0, hi = 1;
	const vertex start = a, end = b;
	const double dx = b.x-a.x, dy = b.y-a.y;
	const double p[] = { -dx, dx, -dy, dy };
	const double q[] = { a.x, width-1-a.x, a.y, height-1-a.y };
	for (int i = 0; i < 4; ++i)
	{
		if (p[i] == 0) { if (q[i] < 0) return false; }
		else if (p[i] < 0) lo = std::max(lo, q[i]/p[i]);
		else hi = std::min(hi, q[i]/p[i]);
		if (lo > hi) return false;
	}
	a = interpolate(start, end, lo);
	b = interpolate(start, end, hi);
	return true;
}

inline std::vector<segment> extract(const std::vector<quad> &quads, const uint16_t *depth, int width, int height)
{
	std::vector<segment> result;
	if (!depth || width <= 0 || height <= 0) return result;
	// The same geometric edge may be submitted by both neighbouring faces.
	using key = std::tuple<double, double, double, double, double, double>;
	std::set<key> seen;
	for (const auto &q : quads)
	{
		for (unsigned edge = 0; edge < 4; ++edge)
		{
			vertex a = q.v[edge], b = q.v[(edge+1)%4];
			if (!clip(a, b, width, height)) continue;
			if (std::tie(a.x,a.y,a.z) > std::tie(b.x,b.y,b.z)) std::swap(a,b);
			if (!seen.emplace(a.x,a.y,a.z,b.x,b.y,b.z).second) continue;
			const double length = std::max(std::abs(b.x-a.x), std::abs(b.y-a.y));
			if (length < 1) continue;
			const int steps = int(std::ceil(length));
			int run = -1;
			for (int i = 0; i <= steps+1; ++i)
			{
				bool visible = false;
				if (i <= steps)
				{
					const vertex p = interpolate(a,b,double(i)/steps);
					const int x = std::clamp(int(std::lround(p.x)),0,width-1);
					const int y = std::clamp(int(std::lround(p.y)),0,height-1);
					const uint16_t z = depth[y*width+x];
					// Match the rasterizer's valid range. Bias compensates integer
					// scan conversion; background also covers unfilled outer edges.
					visible = p.z >= 0 && p.z < 0x7fff && (z == 0x7fff || p.z <= double(z)+4.0);
				}
				if (visible && run < 0) run = i;
				if (!visible && run >= 0)
				{
					if (i-1 > run)
					{
						result.push_back({interpolate(a,b,double(run)/steps), interpolate(a,b,double(i-1)/steps), q.pen});
						if (result.size() >= max_segments) return result;
					}
					run = -1;
				}
			}
		}
	}
	return result;
}
} // namespace namcos21_hlr
#endif
