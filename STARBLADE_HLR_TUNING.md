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
