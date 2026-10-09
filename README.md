# cropsim

A deterministic, headless C++17 library for generating and rendering immutable 2D crop worlds.

## Development

```sh
nix develop
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The initial YAML format accepts explicit crops and deterministic grid generators:

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

World snapshots use a versioned, little-endian binary representation. The built-in headless renderer produces in-memory 8-bit grayscale images and portable PGM files.
