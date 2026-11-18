// license:BSD-3-Clause
// Standalone geometry checks; compile with a C++17 compiler, without MAME.
#include "namcos21_hlr.h"
#include <cassert>
#include <iostream>

int main()
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
	std::cout << "Starblade HLR geometry checks passed\n";
}
