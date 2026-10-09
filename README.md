# cropsim

A deterministic, headless C++17 library for generating and rendering immutable 2D crop worlds.

## Development

```sh
nix develop
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
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

World snapshots use a versioned, little-endian binary representation. The built-in headless
renderer orthographically projects all leaves and produces their binary occupancy union as
in-memory 8-bit grayscale images and portable PGM files.

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
