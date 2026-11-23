// license:BSD-3-Clause
// Standalone geometry checks; compile with a C++17 compiler, without MAME.
#include "namcos21_hlr.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char *argv[])
{
	using namespace namcos21_hlr;
	std::vector<uint16_t> z(100, 0x7fff);
	quad q{{vertex{1,1,100},vertex{8,1,100},vertex{8,8,100},vertex{1,8,100}}, 123};
	auto lines = extract({q}, z.data(),10,10);
	assert(lines.size() == 4); // no triangulation diagonal
	assert(extract({q,q},z.data(),10,10).size() == 4); // duplicate edges
	std::fill(z.begin(),z.end(),50);
	assert(extract({q},z.data(),10,10).empty()); // fully occluded
	std::fill(z.begin(),z.end(),100);
	assert(extract({q},z.data(),10,10).size() == 4); // own surface
	std::fill(z.begin(),z.end(),0x7fff);
	for (int y=0;y<10;++y) for(int x=4;x<=5;++x) z[y*10+x]=50;
	lines = extract({q},z.data(),10,10);
	assert(lines.size() == 6); // top and bottom split by a nearer occluder
	for (const auto &s:lines)
		assert(!(s.a.x < 4 && s.b.x > 5));
	vertex a{-10000,5,0}, b{10000,5,200};
	assert(clip(a,b,10,10));
	assert(std::abs(a.x)<0.0001 && std::abs(b.x-9)<0.0001);
	assert(a.z > 99 && b.z < 101); // clipping preserves interpolated depth
	a={-5,-5,100};b={-1,-1,100};
	assert(!clip(a,b,10,10));
	assert(extract({q},nullptr,10,10).empty());
	std::vector<quad> many;
	for(int i=0;i<2000;++i) { q.v[0].z=q.v[1].z=q.v[2].z=q.v[3].z=i+1; many.push_back(q); }
	std::fill(z.begin(),z.end(),0x7fff);
	assert(extract(many,z.data(),10,10).size() == max_segments);
	// Planet bands keep their external silhouette, suppress their shared
	// contour even across colours/depths, and leave foreground edges alone.
	quad band1{{vertex{1,2,18000},vertex{8,2,18000},vertex{8,5,18000},vertex{1,5,18000}},0x3ee3};
	quad band2{{vertex{1,5,18001},vertex{8,5,18001},vertex{8,9,18001},vertex{1,9,18001}},0x3ee4};
	std::fill(z.begin(),z.end(),0x7fff);
	const auto full = extract({band1,band2},z.data(),10,10);
	const auto outline = extract({band1,band2},z.data(),10,10,true);
	bool full_seam = false, outer_top = false;
	for (const auto &s : full) if (s.a.y == 5 && s.b.y == 5) full_seam = true;
	for (const auto &s : outline) {
		assert(!(s.a.y == 5 && s.b.y == 5));
		if (s.a.y == 2 && s.b.y == 2) outer_top = true;
	}
	assert(full_seam && outer_top && outline.size() < full.size());
	quad ship{{vertex{3,6,50},vertex{6,6,50},vertex{6,8,50},vertex{3,8,50}},123};
	auto mixed = extract({band1,band2,ship},z.data(),10,10,true);
	unsigned ship_edges = 0;
	for (const auto &s : mixed) if (s.pen == ship.pen) ++ship_edges;
	assert(ship_edges == 4);
	ship.pen = 0x3ee3;
	assert(extract({ship},z.data(),10,10,true).size() == 4); // nearby palette reuse is not a planet
	for (int y=0;y<10;++y) for (int x=4;x<=5;++x) z[y*10+x]=50;
	for (const auto &s : extract({band1,band2},z.data(),10,10,true))
		assert(!(s.a.x < 4 && s.b.x > 5)); // foreground still occludes horizon
	assert(extract({band1},nullptr,10,10,true).empty());
	if (argc == 2)
	{
		std::ifstream file(argv[1]); assert(file);
		std::string row; std::getline(file,row);
		std::vector<quad> captured;
		while (std::getline(file,row)) {
			std::replace(row.begin(),row.end(),',',' ');
			std::istringstream fields(row); quad face{}; fields >> face.pen;
			for (auto &v : face.v) fields >> v.x >> v.y >> v.z;
			assert(fields); captured.push_back(face);
		}
		std::vector<uint16_t> sky(496*480,0x7fff);
		const auto before = extract(captured,sky.data(),496,480);
		const auto after = extract(captured,sky.data(),496,480,true);
		assert(!captured.empty() && after.size() < before.size());
		// Neutral depth isolates the outline filter from the separate tested
		// occlusion path. Non-planet materials must retain identical segments.
		using saved_edge = std::tuple<double,double,double,double,double,double,uint16_t>;
		auto foreground = [](const std::vector<segment> &lines) {
			std::vector<saved_edge> edges;
			for (const auto &s : lines) if (!planet_material(s.pen))
				edges.emplace_back(s.a.x,s.a.y,s.a.z,s.b.x,s.b.y,s.b.z,s.pen);
			return edges;
		};
		assert(foreground(before) == foreground(after));
		std::cout << "Captured scene: " << captured.size() << " quads, " << before.size() << " -> " << after.size() << " segments; other materials unchanged\n";
	}
	std::cout << "Starblade HLR geometry checks passed\n";
}
