# Starblade hidden-line prototype

This extends the existing Vectrx2020 device/shared-library path. Native vector
games retain their existing point-stream handling. Only the Starblade machine
configuration enables polygon capture; the shared System 21 rasterizer defaults
to capture disabled.

Regenerate the vectrx2020 subtarget projects after changing the Lua source list,
then build normally. The target adds Starblade/ST1, System 21 support sources,
TMS32025 DSP emulation and C140 sound. Existing generated Visual Studio projects
need regeneration to acquire these sources and dependencies.

Examples (append your normal ROM path and Vectrx2020 connection options):

```
mamevectrx2020 starblad -starblade_hlr_preview -verbose
mamevectrx2020 starblad -starblade_hlr -verbose
mamevectrx2020 starblad -starblade_hlr -starblade_hlr_preview -verbose
```

Both options default off. Preview alone does not create or connect a Vectrx2020
output device. The original filled raster output remains available when preview
is off. Preview replaces the picture with polygon edge lines, excluding sprites.
Preview follows `-vector_rgb`: monochrome by default, original face colours when enabled.
The verbose log reports the visible segment count every 120 screen frames.

`-starblade_hlr_rate 30` caps hardware output at 30 frames per second to compare
network load with the default limit of 60. Emulation and preview keep their normal
rate. Missed output deadlines are discarded; the next submission uses current
geometry. Verbose output reports synchronous driver submission time and warns
when a submission blocks for at least 100 milliseconds.

Quad perimeters are collected before triangulation and double-buffered with
the original depth buffers. Viewport clipping interpolates endpoint depth before
sampling. Edges are tested at approximately pixel spacing against the completed
visible depth buffer, with a four-unit depth bias, then emitted as straight visible
runs. Exact duplicate edges are suppressed. Limits are 32768 captured quads and
4096 emitted segments per frame; dense scenes truncate in submission order.
Save-state loading discards transient edge lists until another rendered frame.

This is a first visual prototype: coplanar mesh seams remain; shared edges with
different face colours use the first encountered colour; sprite graphics and
sprite occlusion are excluded; depth cueing is not applied to vector colour.
Boundary rounding and the depth bias need gameplay inspection. The segment cap
is a software bound, not a measured hardware drawing budget.

The standalone `src/mame/namco/namcos21_hlr_test.cpp` checks visible, occluded,
split and duplicated edges, viewport clipping, depth interpolation and the cap.
Compile it separately with a C++17 compiler; it is not part of the emulator target.
