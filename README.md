# gaucloth

A real-time C++/OpenGL renderer for physics-based Gaussian cloth.

https://github.com/user-attachments/assets/32b52397-f553-435b-8805-e2a5d49c0884

## Overview

`gaucloth` is a master's capstone project exploring **real-time simulation and
rendering of physics-based Gaussian cloth**. It combines an XPBD cloth simulator
with mesh-embedded 3D Gaussian splats for appearance, rendered interactively in
C++ and OpenGL.

The work is motivated by *PGC: Physics-Based Gaussian Cloth from a Single Pose*
(Guo et al., CVPR 2025), which reconstructs simulation-ready garments — a cloth
mesh with embedded Gaussian splats — and animates them on novel poses. PGC
produces high-quality results but runs at ~1.6 FPS and sets its physics
parameters by hand. This project investigates two gaps the paper leaves open:

1. **Real-time rendering** — implementing the simulate-deform-render loop at
   interactive frame rates, and characterizing what limits performance.
2. **Parameter estimation** *(planned)* — recovering cloth material parameters
   (stiffness, density) from a single static draped pose rather than tuning them
   manually.

This project does **not** reproduce PGC's multi-camera reconstruction pipeline.
It starts from an existing or synthetic Gaussian-textured mesh and focuses on the
real-time simulation, rendering, and parameter-estimation downstream.

## Status

Early development. Currently:

- [x] Window + OpenGL 4.3 core context (GLFW + GLAD)
- [x] Cloth grid geometry and orbit camera
- [ ] XPBD stretch + bending constraints
- [ ] Collision and user interaction
- [ ] Mesh-embedded Gaussian splat rendering
- [ ] PBR shading and frequency-decomposition compositing
- [ ] Performance characterization
- [ ] (Stretch) Material parameter estimation from static pose

## Building

**Requirements:** CMake ≥ 3.16, a C++17 compiler, and git.
GLFW is fetched and built automatically by CMake; GLAD is vendored in the repo.
OpenGL is provided by your system's GPU drivers.

```sh
cmake -B build
cmake --build build
./build/gaucloth
```

The first configure step downloads and builds GLFW from source and may take a
minute; subsequent builds are cached and fast.

**Linux note:** building GLFW from source requires standard X11 and/or Wayland
development headers, which are present on most desktop Linux installations.

## Project structure

```
gaucloth/
├── CMakeLists.txt        # portable build (FetchContent GLFW + vendored GLAD)
├── src/
│   ├── main.cpp          # window, rendering, camera, and the render loop
│   └── cloth.h           # XPBD cloth simulation (physics only — no OpenGL)
└── external/
    └── glad/             # vendored OpenGL loader (generated, committed)
```

## License
