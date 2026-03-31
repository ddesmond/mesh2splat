# Mesh2Splat Python Bindings - Implementation Plan

## Status: COMPLETE

All phases implemented and tested. Pip package and Docker builds available.

---

## Overview

Python bindings for the Mesh2Splat library, enabling headless mesh-to-gaussian conversion from Python.

### Features

| Feature | Details | Status |
|---------|---------|--------|
| **Binding Tech** | pybind11 | Done |
| **Backends** | CPU (portable) + GPU (OpenGL) | Done |
| **Backend Selection** | Auto-detect (GPU first, fallback to CPU) | Done |
| **GPU Context** | EGL (Linux), CGL (macOS) | Done |
| **Platforms** | Linux, macOS | Done |
| **Python Versions** | 3.10, 3.11, 3.12 | Done |
| **Pip Package** | scikit-build-core | Done |
| **Docker Builds** | manylinux, Debian, Ubuntu | Done |

### Test Results (macOS, Apple Silicon)

| Backend | Mode | Gaussians | Time |
|---------|------|-----------|------|
| CPU | UV | 118,464 | ~118ms |
| GPU | UV | 118,874 | ~65ms |
| CPU | Projection | 631,102 | ~120ms |
| GPU | Projection | 167,348 | ~65ms |

Test model: `small_lpg_tank_4k.gltf` (15,042 triangles) at resolution 512.

---

## Quick Start

### Install from Wheel

```bash
pip install dist/mesh2splat-*.whl
```

### Build Wheels

```bash
# macOS
make wheels-macos

# Linux (Docker)
make wheels-linux

# All platforms
make wheels-all
```

### Usage

```python
import mesh2splat

# Simple conversion
mesh2splat.convert("model.gltf", "output.ply")

# With options
converter = mesh2splat.Converter()
options = mesh2splat.ConversionOptions()
options.resolution = 1024
options.rasterization_mode = mesh2splat.RasterizationMode.UV

result = converter.convert_file("model.gltf", options)
print(f"Generated {result.total_gaussians} gaussians")

mesh2splat.PlyIO.save("output.ply", result.gaussians)
```

---

## Project Structure

```
mesh2splat/
├── CMakeLists.txt              # Root CMake (BUILD_PYTHON_BINDINGS option)
├── pyproject.toml              # Pip package config (scikit-build-core)
├── Makefile                    # Build targets for wheels
├── requirements.txt            # Python dev dependencies
├── PLAN.md                     # This file
├── GUIDE.md                    # Docker build guide
├── BUILD_PYTHON.md             # Python wheel build guide
│
├── docker/                     # Docker build files
│   ├── build.sh                # Build orchestration script
│   ├── Dockerfile.manylinux2014
│   ├── Dockerfile.manylinux_2_28
│   ├── Dockerfile.debian12
│   ├── Dockerfile.ubuntu2204
│   └── Dockerfile.ubuntu2404
│
├── python/                     # Python bindings source
│   ├── CMakeLists.txt
│   ├── mesh2splat/
│   │   └── __init__.py
│   ├── example.py
│   └── src/mesh2splat/
│       ├── Bindings.cpp        # pybind11 module
│       ├── Converter.hpp/cpp   # Main interface
│       ├── core/               # GL-free types
│       │   ├── Types.hpp/cpp
│       │   ├── GltfLoader.hpp/cpp
│       │   ├── PlyIO.hpp/cpp
│       │   └── TextureSampler.hpp/cpp
│       ├── cpu/                # CPU backend
│       │   ├── CPUConverter.hpp/cpp
│       │   └── Rasterizer.hpp/cpp
│       └── gpu/                # GPU backend
│           ├── GPUConverter.hpp/cpp
│           ├── HeadlessContext.hpp/cpp
│           └── ShaderManager.hpp/cpp
│
├── dist/                       # Built wheels (gitignored)
│   ├── macos/
│   └── linux/
│
└── thirdParty/
    └── pybind11/               # Git submodule
```

---

## Build System

### Makefile Targets

| Target | Description |
|--------|-------------|
| `make wheels-macos` | Build macOS wheels (3.10, 3.11, 3.12) |
| `make wheels-macos-3.10` | Build macOS wheel for Python 3.10 |
| `make wheels-linux` | Build all Linux wheels via Docker |
| `make wheels-linux-manylinux2014` | Build manylinux2014 wheels |
| `make wheels-all` | Build all platforms |
| `make clean` | Remove build artifacts |
| `make test` | Test installed wheel |

### Docker Images

| Image | Base | glibc | Python Versions |
|-------|------|-------|-----------------|
| manylinux2014 | CentOS 7 | 2.17+ | 3.10, 3.11, 3.12 |
| manylinux_2_28 | AlmaLinux 8 | 2.28+ | 3.10, 3.11, 3.12 |
| debian12 | Debian Bookworm | 2.36 | 3.11 |
| ubuntu2204 | Ubuntu 22.04 | 2.35 | 3.10 |
| ubuntu2404 | Ubuntu 24.04 | 2.39 | 3.12 |

---

## Python API

### Enums

```python
mesh2splat.Backend.Auto     # Auto-detect (default)
mesh2splat.Backend.CPU      # Force CPU
mesh2splat.Backend.GPU      # Force GPU

mesh2splat.RasterizationMode.UV         # UV-space rasterization
mesh2splat.RasterizationMode.Projection # Orthogonal projection

mesh2splat.PlyFormat.Standard    # Standard 3DGS PLY
mesh2splat.PlyFormat.PBR         # With metallic/roughness/ao
mesh2splat.PlyFormat.Compressed  # Compact format
```

### ConversionOptions

```python
options = mesh2splat.ConversionOptions()
options.resolution = 512                # UV rasterization resolution
options.backend = Backend.Auto          # Backend selection
options.rasterization_mode = RasterizationMode.UV  # Rasterization method
options.ply_format = PlyFormat.Standard # Output format
options.scale_multiplier = 1.0          # Gaussian scale factor
options.srgb_conversion = True          # Convert to linear color
options.verbose = False                 # Debug output
```

### Converter

```python
converter = mesh2splat.Converter()  # Auto-detect backend
converter = mesh2splat.Converter(mesh2splat.Backend.GPU)  # Force GPU

# Get backend info
print(converter.get_active_backend())
print(mesh2splat.Converter.get_available_backends())

# Convert file
result = converter.convert_file("model.gltf", options)

# Convert loaded scene
loader = mesh2splat.GltfLoader()
scene = loader.load("model.gltf")
result = converter.convert(scene, options)
```

### ConversionResult

```python
result.success            # bool
result.error_message      # str (if failed)
result.total_gaussians    # int
result.conversion_time_ms # float
result.gaussians          # list[Gaussian]

# Export to numpy
arrays = result.to_numpy()
positions = arrays["positions"]   # (N, 3) float32
colors = arrays["colors"]         # (N, 3) float32 - SH0
scales = arrays["scales"]         # (N, 3) float32
rotations = arrays["rotations"]   # (N, 4) float32 - wxyz
opacities = arrays["opacities"]   # (N,) float32
normals = arrays["normals"]       # (N, 3) float32
```

### PlyIO

```python
# Save
mesh2splat.PlyIO.save("output.ply", result.gaussians)
mesh2splat.PlyIO.save("output.ply", result.gaussians, mesh2splat.PlyFormat.PBR)

# Load
gaussians = mesh2splat.PlyIO.load("input.ply")
```

---

## Gaussian Data Layout

| Field | Type | Description |
|-------|------|-------------|
| x, y, z | float | Position |
| r, g, b | float | SH0 color coefficients |
| opacity | float | Opacity (0-1) |
| scale_x, scale_y, scale_z | float | Gaussian scales |
| rot_w, rot_x, rot_y, rot_z | float | Rotation quaternion (wxyz) |
| nx, ny, nz | float | Surface normal |
| metallic, roughness, ao | float | PBR properties |

### PLY Format Notes

- Colors stored as `f_dc_0/1/2` (SH0 coefficients)
- Opacity stored as logit (inverse sigmoid)
- Scales stored as log

---

## Implementation Phases

### Phase 0: Environment Setup
- `requirements.txt`, `.gitignore`, `models/` ignore

### Phase 1: CMake Configuration  
- `BUILD_PYTHON_BINDINGS`, `MESH2SPLAT_ENABLE_GPU` options
- `MESH2SPLAT_STANDALONE_PYTHON` for pip builds

### Phase 2: Core Types
- `Types.hpp/cpp` - Gaussian, Scene, Mesh, RasterizationMode
- `GltfLoader.hpp/cpp` - GLTF/GLB loading
- `PlyIO.hpp/cpp` - PLY I/O with proper SH0/logit encoding
- `TextureSampler.hpp/cpp` - Bilinear sampling

### Phase 3: CPU Backend
- `Rasterizer.hpp/cpp` - UV and Projection modes
- `CPUConverter.hpp/cpp` - Full pipeline

### Phase 4: GPU Backend
- `HeadlessContext.hpp/cpp` - EGL (Linux) / CGL (macOS)
- `ShaderManager.hpp/cpp` - GLSL 410 shaders with mode uniform
- `GPUConverter.hpp/cpp` - MRT rendering

### Phase 5: Converter Interface
- `Converter.hpp/cpp` - Factory with auto-detection

### Phase 6: Python Bindings
- `Bindings.cpp` - pybind11 module
- `__init__.py` - Package init

### Phase 7: Pip Package
- `pyproject.toml` - scikit-build-core config
- Updated CMakeLists.txt for standalone builds

### Phase 8: Docker Builds
- Dockerfiles for 5 Linux distributions
- `build.sh` orchestration script
- `Makefile` with all targets

---

## Known Limitations

1. **macOS GPU**: Uses deprecated OpenGL via CGL. Works but generates warnings.

2. **Rasterization Mode Differences**: UV mode produces consistent results between CPU/GPU (~0.3% difference). Projection mode differs more due to implementation details.

3. **Linux GPU**: EGL backend implemented but requires actual GPU hardware at runtime.

---

## Documentation

- **PLAN.md** - This implementation plan
- **GUIDE.md** - Docker build guide
- **BUILD_PYTHON.md** - Python wheel build guide
- **README.md** - Project overview (existing)

---

## Files Summary

| Category | Files | Lines |
|----------|-------|-------|
| Python bindings source | 27 | ~4,400 |
| Build configuration | 4 | ~450 |
| Docker files | 6 | ~300 |
| Documentation | 3 | ~700 |
| **Total** | **40** | **~5,850** |
