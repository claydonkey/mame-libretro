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
# Starblade hardware line filters

`starblade_hlr_planet_outline` defaults to **1**. On means remove extra
planetary surface/atmosphere contours and retain the external silhouette.
Set it to **0**, or pass `-nostarblade_hlr_planet_outline`, to restore those
contours. It applies to Starblade HLR output and its HLR preview, not the
normal filled raster picture.

The experimental selector uses the un-depth-cued planet material pens
0x3ee3–0x3ef0 observed in the saved 40-second scene, restricted to quads whose
vertices are in the distant half of the valid depth range. A projected union
mask suppresses internal material boundaries; the normal depth test still
occludes the remaining outline behind foreground objects. Nearby palette
reuse is excluded. This is a game-specific material/depth selector, not a
general object-identity guarantee across every scene; visual comparison is
still required, especially as the planet moves or materials are reused.

Other independent switches:

- `starblade_hlr_deduplicate 1`: remove identical final-coordinate lines.
- `starblade_hlr_merge_overlaps 1`: trim exact collinear repeated coverage.

Those two switches default to 0; the comparison launch enables both explicitly.
The planet outline defaults to 1 as requested. No firmware changes are required.

`-verbose` reports the active planet-outline state and sampled geometry/output
counts. `-starblade_hlr_dump <path.csv>` optionally saves one raw quad snapshot
after 40 emulated seconds. It is disabled by default and is useful for checking
material identification. The standalone `namcos21_hlr_test.cpp` accepts that CSV
as an optional argument for a captured-scene check, in addition to its synthetic
silhouette, occlusion and nearby-material-reuse checks.
