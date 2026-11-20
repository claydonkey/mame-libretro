// license:BSD-3-Clause
// Standalone diagnostic geometry checks; requires C++17.
#include "vectrx_segment_stats.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace vectrx_segment_stats;
    duplicate_filter filter;
    const std::vector<segment> input{{10,0,0,0,1}, {0,0,10,0,1}, {5,0,15,0,1}, {0,0,10,0,2}, {10,0,0,0,1}};
    std::vector<segment> kept;
    for (const auto &s : input) if (filter.accept(s)) kept.push_back(s);
    assert(kept.size() == 3);
    // First direction and relative order survive; partial overlaps and other
    // colours remain. A new frame can draw all of its first copies again.
    assert(key_for(kept[0]) == key_for(input[0]) && kept[0].x0 == 10 && kept[0].x1 == 0);
    assert(key_for(kept[1]) == key_for(input[2]) && key_for(kept[2]) == key_for(input[3]));
    filter.clear();
    assert(filter.accept(input[0]) && !filter.accept(input[1]));
    auto close = [](double a, double b) { assert(std::abs(a - b) < 0.000001); };
    overlap_filter overlaps;
    assert(overlaps.trim({3,0,7,0,1}).size() == 1);
    auto pieces = overlaps.trim({10,0,0,0,1});
    assert(pieces.size() == 2);
    assert(pieces[0].x0 == 10 && pieces[0].x1 == 7 && pieces[1].x0 == 3 && pieces[1].x1 == 0);
    assert(overlaps.trim({0,0,10,0,1}).empty());
    assert(overlaps.trim({0,0,10,0,2}).size() == 1);
    assert(overlaps.trim({0,1,10,1,1}).size() == 1);
    assert(overlaps.trim({5,-5,5,5,1}).size() == 1); // Crossing is not overlap.
    assert(overlaps.trim({2,0,2,0,1}).size() == 1); // Preserve degenerate points.
    overlaps.clear();
    assert(overlaps.trim({0,0,10,0,1}).size() == 1);
    overlaps.clear();
    overlaps.trim({2,0,4,0,1}); overlaps.trim({6,0,8,0,1});
    pieces = overlaps.trim({0,0,10,0,1});
    assert(pieces.size() == 3 && pieces[0].x1 == 2 && pieces[1].x0 == 4 && pieces[1].x1 == 6 && pieces[2].x0 == 8);
    overlaps.clear();
    overlaps.trim({0,0,3,4,1});
    pieces = overlaps.trim({-3,-4,6,8,1});
    assert(pieces.size() == 2 && pieces[0].x0 == -3 && pieces[0].y1 == 0 && pieces[1].x0 == 3 && pieces[1].y1 == 8);
    overlaps.clear();
    overlaps.trim({0,0,0,5,1});
    pieces = overlaps.trim({0,10,0,-5,1});
    assert(pieces.size() == 2 && pieces[0].y0 == 10 && pieces[0].y1 == 5 && pieces[1].y0 == 0 && pieces[1].y1 == -5);
    // Many intersecting intervals, directions, offsets and colours: output
    // union must match input, with no positive-length repeated coverage.
    std::vector<segment> all, output;
    overlaps.clear();
    for (int i = 0; i < 400; ++i) {
        int a = (i * 17) % 41 - 20, b = (i * 29 + 7) % 41 - 20;
        const int offset = i % 3;
        segment s{a, -2*a + offset, b, -2*b + offset, uint32_t(i % 2)};
        all.push_back(s);
        auto remainder = overlaps.trim(s);
        output.insert(output.end(), remainder.begin(), remainder.end());
        close(measure(output).covered_length, measure(all).covered_length);
        close(measure(output).visible_length, measure(output).covered_length);
    }
    auto r = measure({});
    assert(!r.segments); close(r.visible_length, 0); close(r.covered_length, 0);
    r = measure({{0,0,10,0,1}, {10,0,0,0,1}, {5,0,15,0,1}});
    assert(r.segments == 3 && r.duplicates == 1 && r.connected == 1);
    close(r.visible_length, 30); close(r.covered_length, 15); close(r.blank_travel, 5);
    r = measure({{0,0,0,10,1}, {0,5,0,15,1}, {0,20,0,25,1}});
    close(r.visible_length, 25); close(r.covered_length, 20);
    r = measure({{0,0,3,4,1}, {3,4,0,0,1}});
    assert(r.duplicates == 1); close(r.visible_length, 10); close(r.covered_length, 5);
    r = measure({{0,0,10,0,1}, {0,0,10,0,2}, {5,-5,5,5,1}, {1,1,1,1,1}});
    assert(!r.duplicates && r.zero_length == 1); close(r.visible_length, 30); close(r.covered_length, 30);
    r = measure({{4094,4094,0,0,1}, {2047,2047,0,0,1}});
    close(r.covered_length, std::hypot(4094.0, 4094.0));
    std::cout << "Segment filters passed: first-copy order, reversed duplicates, exact overlap trimming, split direction, union preservation, colour isolation, crossings and degenerate edges\n";
}
