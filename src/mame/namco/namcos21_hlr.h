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

// Starblade's distant planet uses these un-depth-cued material pens. Captured
// quad data distinguishes its atmosphere/surface bands from foreground meshes.
// Test BEFORE palette depth cueing; never classify by final screen brightness.
inline bool planet_material(uint16_t pen) { return pen >= 0x3ee3 && pen <= 0x3ef0; }
inline bool planet_surface(const quad &q)
{
	// Captured planet vertices span 17698..18129; foreground reaches 13945.
	// Restrict the material match to the distant half of valid depth so a
	// nearby object reusing a palette entry retains its full geometry.
	return planet_material(q.pen) && std::all_of(q.v.begin(),q.v.end(), [](const vertex &v) { return v.z >= 0x4000 && v.z < 0x7fff; });
}

inline std::vector<uint8_t> planet_mask(const std::vector<quad> &quads, int width, int height)
{
	std::vector<uint8_t> mask(size_t(width) * height, 0);
	for (const auto &q : quads) if (planet_surface(q))
		for (const auto &indices : {std::array<unsigned, 3>{0,1,2}, std::array<unsigned, 3>{2,3,0}})
		{
			const std::array<vertex, 3> tri{q.v[indices[0]], q.v[indices[1]], q.v[indices[2]]};
			const int low = std::max(0, int(std::ceil(std::min({tri[0].y,tri[1].y,tri[2].y}))));
			const int high = std::min(height-1, int(std::floor(std::max({tri[0].y,tri[1].y,tri[2].y}))));
			for (int y = low; y <= high; ++y)
			{
				double left = width, right = -1;
				for (unsigned i = 0; i < 3; ++i)
				{
					const auto &a = tri[i], &b = tri[(i+1)%3];
					if (a.y == b.y || y < std::min(a.y,b.y) || y >= std::max(a.y,b.y)) continue;
					const double x = a.x + (b.x-a.x) * (y-a.y) / (b.y-a.y);
					left = std::min(left,x); right = std::max(right,x);
				}
				const int first = std::max(0, int(std::ceil(left)));
				const int last = std::min(width, int(std::ceil(right)));
				for (int x = first; x < last; ++x) mask[size_t(y)*width+x] = 1;
			}
		}
	return mask;
}

inline std::vector<segment> extract(const std::vector<quad> &quads, const uint16_t *depth, int width, int height, bool planet_outline = false)
{
	std::vector<segment> result;
	if (!depth || width <= 0 || height <= 0) return result;
	const auto mask = planet_outline ? planet_mask(quads,width,height) : std::vector<uint8_t>{};
	auto planet_inside = [&](double x, double y) {
		const int ix = int(std::floor(x)), iy = int(std::floor(y));
		// Cropping is not a planetary boundary; no screen-border outlines.
		return ix < 0 || iy < 0 || ix >= width || iy >= height || mask[size_t(iy)*width+ix] != 0;
	};
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
			const double distance = std::hypot(b.x-a.x,b.y-a.y);
			const double nx = -(b.y-a.y)/distance, ny = (b.x-a.x)/distance;
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
					if (visible && planet_outline && planet_surface(q))
						visible = planet_inside(p.x+nx,p.y+ny) != planet_inside(p.x-nx,p.y-ny);
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
