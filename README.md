# cropsim

A deterministic, headless C++17 library for generating and rendering immutable 2D crop worlds.

## Development

```sh
nix develop
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Or build and run the C++ tests plus the spatial benchmark through the flake:

```sh
nix run .#cpp-test
```

The YAML format accepts explicit crops and deterministic grid generators. A `radius` is
shorthand for one circular leaf:

```yaml
seed: 568616
crops:
  - position: [0.25, 0.75]
    radius: 0.1
generators:
  - gentype: grid
    origin: [1.0, 2.0]
    rows: 2
    columns: 3
    spacing: [0.5, 0.25]
    radius: 0.08
    jitter: 0.01
```

Multi-leaf crops use deterministically ordered oriented ellipses. Leaf positions are relative
to the crop position, radii are the two local ellipse axes, and rotations are in radians:

```yaml
crops:
  - position: [0.25, 0.75]
    leaves:
      - {type: ellipse, position: [0.15, 0.0], radii: [0.15, 0.05], rotation: 1.3}
      - {type: ellipse, position: [0.0, 0.15], radii: [0.15, 0.05], rotation: 0.2}
generators:
  - gentype: grid
    origin: [1.0, 2.0]
    rows: 2
    columns: 3
    spacing: [0.5, 0.25]
    leaves:
      - {position: [0.1, 0.0], radii: [0.12, 0.04], rotation: 0.5}
      - {position: [-0.1, 0.0], radii: [0.12, 0.04], rotation: -0.5}
```

Placement and crop morphology can also be composed. The `grid` generator owns positions and
jitter while its nested crop generator independently creates geometry from a per-crop stream:

```yaml
seed: 568616
generators:
  - gentype: grid
    origin: [0.0, 0.0]
    rows: 2
    columns: 5
    spacing: [0.2, 0.2]
    jitter: 0.01
    crop:
      gentype: generic
      scale: {mean: 0.05, min: 0.04, max: 0.06}
      leaf_num: {mean: 4, min: 1, max: 10, stddev: 1.5}
      leaf_length: {mean: 1.0, min: 0.8, max: 1.2}
      leaf_width: {mean: 0.4, min: 0.2, max: 0.6}
      leaf_offset: {mean: 0.0, min: -0.05, max: 0.05}
      leaf_orientation: {mean: 0.0, min: -0.5, max: 0.5}
```

Each distribution, including `scale`, may instead be a scalar for an exact value. Scale is
sampled once per crop, so all leaves share that crop's size. Missing members use the defaults
shown above, and a missing `stddev` is `(max - min) / 6`. Legacy grid-level `radius` and `leaves`
remain shorthand for `crop: {gentype: fixed, ...}`. Nested and legacy geometry cannot be mixed.
Generic leaves use their length as the local x/major radius, rotated along their radial angle;
their width is the perpendicular local y/minor radius.

## Hierarchical field generation

Field sets, fields, and rows are transient generators: they deterministically place crops but
are not stored in the immutable world or its snapshots. Coordinates are metres and angles are
radians. A field set accepts one simple outer polygon, creates an exact number of jittered-lattice
Voronoi fields, leaves fixed-width road gaps between them, fills each field with parallel rows,
and fills each row with crops:

```yaml
seed: 568616
generators:
  - gentype: field_set
    bounds: [[0, 0], [100, 0], [100, 80], [0, 80]]
    count: 12
    road_width: 3.0
    seed_jitter: 0.8
    minimum_field_area: 0.0
    minimum_field_width: 0.0
    field:
      gentype: parallel_rows
      orientation: auto
      orientation_offset: 0.0
      row_spacing: 0.75
      row_jitter: 0.0
      headland: 0.5
      row:
        gentype: linear
        crop_spacing: 0.25
        along_jitter: 0.0
        cross_jitter: 0.0
        crop:
          gentype: generic
```

`bounds` and a positive `count` are required, as are the nested `field`, `row`, and `crop`
generator mappings. The other values above show their defaults. `orientation` defaults to the
long axis of the field's minimum-area oriented bounding box and may instead be an explicit angle.
The outer polygon may be concave but cannot contain holes or self-intersections. Crop centers are
kept inside the headland-adjusted field; leaf geometry may extend beyond it.

The field hierarchy uses stable, domain-separated random streams. Changing crop geometry or the
number of crops in one row does not change the field and row layouts of its siblings. World crop
IDs remain sequential in canonical field, row, segment, and crop order.

World snapshots use a versioned, little-endian binary representation. The built-in headless
renderer orthographically projects all leaves and produces their binary occupancy union as
in-memory 8-bit grayscale images and portable PGM files.

## Spatial morphology modifiers

Field sets may declare reusable scalar fields. Coordinates `x` and `y` are world metres; `u` and
`v` normalize the outer field-set AABB to `[0, 1]`. Expressions may refer to other named fields,
regardless of declaration order. Fractal noise uses deterministic 2D lattice gradients with
quintic interpolation; Gaussian hotspots sum elliptical kernels and are not implicitly normalized.

```yaml
modifier_fields:
  patches: {gentype: gaussian_hotspots, count: 8,
            sigma: {mean: 12, min: 6, max: 20},
            amplitude: {mean: 0.8, min: 0.4, max: 1.2}}
  vigor: {gentype: expression, expression: "clamp(patches, -1, 1)"}
# ... field/row nesting ...
crop:
  gentype: generic
  scale:
    mean: 0.06
    min: 0.02
    max: 0.14
    modifiers: [{field: vigor, operation: add, strength: 0.04}]
  leaf_num:
    base: {mean: 5, min: 3, max: 7}
    modifiers: [{field: vigor, operation: add, strength: 2}]
    clamp: [1, 12]
```

Bindings run in YAML order after the base distribution is sampled. `relative` multiplies by `1 + strength * signal`, `add` adds
`strength * signal`, and `replace` assigns `offset + strength * signal`; the optional clamp runs
last. When a parameter distribution explicitly supplies both `min` and `max`, those bounds also
clamp the final modified value; an explicit `clamp` overrides them. Scale and leaf count are sampled once per crop, while leaf dimensions, offset, and
orientation are sampled once per leaf. Modifier definitions and evaluated values are transient;
snapshot v3 contains only the resulting crop geometry. Use `render_modifier_field` with an
optional value range to create a `GrayscaleImage`, then `write_pgm` or `format_terminal_image` for
file and CI previews.

## Optional GPU viewer

The deterministic CPU renderer remains the reference for tests and observations. A separate SDL3/SDL_GPU Vulkan viewer can be enabled for interactive inspection:

```sh
nix run .#cropsim-viewer -- world.yaml
```

Alternatively, build it directly with CMake:

```sh
cmake -S . -B build-viewer -G Ninja -DCROPSIM_BUILD_VIEWER=ON
cmake --build build-viewer
build-viewer/cropsim_viewer world.yaml
build-viewer/cropsim_viewer world.cropsim
```

The viewer detects YAML descriptions and binary snapshots from their contents. The explicit `--yaml` and `--snapshot` forms remain available when needed.

Drag with the left mouse button to pan, or pan with the arrow keys or Vim's `H`, `J`, `K`, and `L` keys. Use the wheel or `+` and `-` to zoom, press `F` to frame the world, and `Q` or Escape to exit. Pass `--check` to validate and load an input without opening a display.

The viewer starts in the canonical single-color occupancy mode. Press `C` to toggle deterministic
per-leaf diagnostic colors, or start in that mode with `--diagnostic-colors`. These colors are
derived in the viewer and are not stored in the world or snapshots.

`format_terminal_image` provides bounded ASCII previews for CI logs and ANSI truecolor half-block previews for interactive diagnostics. `cropsim::testing::compare_images` emits expected, actual, and difference previews and writes full-resolution PGM artifacts when an image comparison fails.

For a one-frame live graphics check, configure with `-DCROPSIM_ENABLE_VIEWER_SMOKE_TEST=ON` and run `ctest -L smoke`. This test is off by default because it requires a working display and Vulkan device.
