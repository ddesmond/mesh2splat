# Mesh2Splat Python Bindings - Implementation Plan

## Status: COMPLETE ✓

All phases implemented and tested successfully on macOS.

---

## Overview

Python bindings for the Mesh2Splat library, enabling headless mesh-to-gaussian conversion from Python.

### Features

| Feature | Details | Status |
|---------|---------|--------|
| **Binding Tech** | pybind11 | ✓ |
| **Backends** | CPU (portable) + GPU (OpenGL) | ✓ |
| **Backend Selection** | Auto-detect (GPU first, fallback to CPU) | ✓ |
| **GPU Context** | EGL (Linux), CGL (macOS) | ✓ |
| **Platforms** | Linux, macOS | ✓ macOS tested |
| **Python Env** | `.venv/` with `requirements.txt` | ✓ |
| **Build** | CMake-integrated | ✓ |
| **API Style** | Object-oriented | ✓ |

### Test Results (macOS, Apple Silicon)

| Backend | Gaussians | Time | Notes |
|---------|-----------|------|-------|
| CPU | 631,102 | ~118ms | UV-space rasterization |
| GPU | 167,348 | ~65ms | Orthogonal projection |

Test model: `small_lpg_tank_4k.gltf` (15,042 triangles) at resolution 512.

---

## Project Structure

```
mesh2splat/
├── CMakeLists.txt                         # MODIFIED: Added BUILD_PYTHON_BINDINGS option
├── .gitignore                             # MODIFIED: Added Python ignores, models/
├── requirements.txt                       # Python dependencies
├── .venv/                                 # Virtual environment (gitignored)
│
├── python/                                # Python bindings
│   ├── CMakeLists.txt                     # Build configuration
│   ├── mesh2splat/
│   │   ├── __init__.py                    # Package init with fallback
│   │   └── _mesh2splat.cpython-*.so       # Built native module
│   ├── example.py                         # Usage example
│   │
│   └── src/mesh2splat/
│       ├── Bindings.cpp                   # pybind11 module entry
│       ├── Converter.hpp/cpp              # Interface + factory
│       │
│       ├── core/                          # Core types (GL-free)
│       │   ├── Types.hpp/cpp              # Gaussian, Scene, Mesh, etc.
│       │   ├── GltfLoader.hpp/cpp         # GLTF loading (tiny_gltf)
│       │   ├── PlyIO.hpp/cpp              # PLY I/O (happly)
│       │   └── TextureSampler.hpp/cpp     # Bilinear texture sampling
│       │
│       ├── cpu/                           # CPU backend
│       │   ├── CPUConverter.hpp/cpp       # CPU conversion pipeline
│       │   └── Rasterizer.hpp/cpp         # Triangle rasterization
│       │
│       └── gpu/                           # GPU backend
│           ├── GPUConverter.hpp/cpp       # GPU conversion pipeline
│           ├── HeadlessContext.hpp/cpp    # EGL (Linux) / CGL (macOS)
│           └── ShaderManager.hpp/cpp      # Embedded GLSL 410 shaders
│
├── models/                                # Test models (gitignored)
│
└── thirdParty/
    ├── pybind11/                          # Git submodule
    └── ... (glm, tiny_gltf, happly, stb)
```

---

## Implementation Phases

### Phase 0: Environment Setup ✓

- `requirements.txt` - pybind11, numpy, pytest
- `.gitignore` - Python ignores, models/, .venv/

### Phase 1: CMake Configuration ✓

- `CMakeLists.txt` - Added `BUILD_PYTHON_BINDINGS` option
- `python/CMakeLists.txt` - Full build with platform detection
- `MESH2SPLAT_ENABLE_GPU` option (default ON)

### Phase 2: Core Types & Utilities ✓

- `Types.hpp/cpp` - Gaussian, GaussianSSBO, Scene, Mesh, Face, Material, BBox, ConversionOptions, ConversionResult
- `GltfLoader.hpp/cpp` - GLTF/GLB loading with transform hierarchy
- `PlyIO.hpp/cpp` - Standard, PBR, and Compressed PLY formats
- `TextureSampler.hpp/cpp` - Bilinear sampling with sRGB conversion

### Phase 3: CPU Backend ✓

- `Rasterizer.hpp/cpp` - UV-space triangle rasterization with barycentric interpolation
- `CPUConverter.hpp/cpp` - Full pipeline with Jacobian-based scale computation

### Phase 4: GPU Backend ✓

- `HeadlessContext.hpp/cpp` - Platform-specific (EGL/CGL) headless OpenGL 4.1
- `ShaderManager.hpp/cpp` - Embedded GLSL 410 shaders (VS/GS/FS)
- `GPUConverter.hpp/cpp` - MRT rendering to 6 float textures

**Bug Fixes Applied:**
- Fixed `flat` qualifier mismatch between GS outputs and FS inputs

### Phase 5: Converter Interface ✓

- `Converter.hpp/cpp` - Factory with auto-detection, error propagation

### Phase 6: Python Bindings ✓

- `Bindings.cpp` - Full pybind11 module with all types exposed
- `__init__.py` - Package init with import error handling

**Types Exposed:**
- Enums: `Backend`, `PlyFormat`
- Classes: `Gaussian`, `ConversionOptions`, `ConversionResult`, `Converter`, `PlyIO`, `GltfLoader`, `Scene`, `Mesh`, `Material`, `Face`, `BBox`
- Functions: `convert()`, `get_version()`, `get_build_info()`, `gaussians_to_numpy()`

### Phase 7: Testing ✓

- `example.py` - Complete usage example
- Tested with `small_lpg_tank_4k.gltf`

---

## Python API

```python
import mesh2splat

# Check build info
print(mesh2splat.get_build_info())
# Output: mesh2splat v1.0.0 (GPU+CPU) [macOS]

# Check available backends
print(mesh2splat.Converter.get_available_backends())
# Output: [Backend.CPU, Backend.GPU]

# Create converter (auto-selects GPU if available)
converter = mesh2splat.Converter()  # or Backend.CPU / Backend.GPU
print(f"Using: {converter.get_active_backend()}")

# Option 1: Direct file conversion
result = converter.convert_file("model.gltf", options)

# Option 2: Load then convert
loader = mesh2splat.GltfLoader()
scene = loader.load("model.gltf")
result = converter.convert(scene, options)

# Check results
print(f"Success: {result.success}")
print(f"Gaussians: {result.total_gaussians}")
print(f"Time: {result.conversion_time_ms:.1f} ms")

# Export to PLY
mesh2splat.PlyIO.save("output.ply", result.gaussians, mesh2splat.PlyFormat.Standard)

# Get as numpy arrays
arrays = result.to_numpy()
positions = arrays["positions"]  # (N, 3)
colors = arrays["colors"]        # (N, 3) - SH0 coefficients
scales = arrays["scales"]        # (N, 3)
rotations = arrays["rotations"]  # (N, 4) - wxyz quaternion
```

### ConversionOptions

```python
options = mesh2splat.ConversionOptions()
options.resolution = 512           # UV rasterization resolution
options.backend = Backend.Auto     # Auto, CPU, or GPU
options.ply_format = PlyFormat.Standard
options.scale_multiplier = 1.0
options.srgb_conversion = True
options.verbose = False
```

---

## Gaussian Data Layout

The `Gaussian` struct stores SH0 color coefficients (not raw RGB):

| Field | Type | Description |
|-------|------|-------------|
| x, y, z | float | Position |
| r, g, b | float | SH0 color coefficients |
| opacity | float | Opacity (0-1) |
| scale_x, scale_y, scale_z | float | Gaussian scales |
| rot_w, rot_x, rot_y, rot_z | float | Rotation quaternion |
| nx, ny, nz | float | Surface normal |
| metallic, roughness, ao | float | PBR properties |

**PLY Format Notes:**
- Standard format: Colors stored as `f_dc_0/1/2` (SH0), opacity as logit, scales as log
- Compressed format: Colors as uint8 RGB, normals as octahedral-encoded uint8

---

## Build Commands

```bash
# Setup (one time)
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Build with GPU (default)
mkdir build && cd build
cmake .. -DBUILD_PYTHON_BINDINGS=ON -DCMAKE_BUILD_TYPE=Release
make -j8 _mesh2splat

# CPU-only build
cmake .. -DBUILD_PYTHON_BINDINGS=ON -DMESH2SPLAT_ENABLE_GPU=OFF

# Test
cd python
python -c "import mesh2splat; print(mesh2splat.get_build_info())"
python example.py
```

---

## Known Limitations

1. **macOS GPU**: Uses deprecated OpenGL via CGL. Works but generates deprecation warnings. Metal backend would be needed for long-term macOS GPU support.

2. **Gaussian Count Difference**: CPU and GPU backends produce different gaussian counts due to different rasterization approaches:
   - CPU: Rasterizes in original UV space
   - GPU: Uses orthogonal projection based on face orientation

3. **Linux**: EGL backend implemented but not yet tested.

---

## Files Implemented

| # | File | Lines | Status |
|---|------|-------|--------|
| 1 | `requirements.txt` | 12 | ✓ |
| 2 | `.gitignore` | (modified) | ✓ |
| 3 | `CMakeLists.txt` | (modified) | ✓ |
| 4 | `python/CMakeLists.txt` | ~100 | ✓ |
| 5 | `python/mesh2splat/__init__.py` | ~70 | ✓ |
| 6 | `python/example.py` | ~50 | ✓ |
| 7 | `python/src/mesh2splat/core/Types.hpp` | ~230 | ✓ |
| 8 | `python/src/mesh2splat/core/Types.cpp` | ~150 | ✓ |
| 9 | `python/src/mesh2splat/core/GltfLoader.hpp` | ~50 | ✓ |
| 10 | `python/src/mesh2splat/core/GltfLoader.cpp` | ~450 | ✓ |
| 11 | `python/src/mesh2splat/core/PlyIO.hpp` | ~50 | ✓ |
| 12 | `python/src/mesh2splat/core/PlyIO.cpp` | ~420 | ✓ |
| 13 | `python/src/mesh2splat/core/TextureSampler.hpp` | ~50 | ✓ |
| 14 | `python/src/mesh2splat/core/TextureSampler.cpp` | ~150 | ✓ |
| 15 | `python/src/mesh2splat/cpu/Rasterizer.hpp` | ~80 | ✓ |
| 16 | `python/src/mesh2splat/cpu/Rasterizer.cpp` | ~250 | ✓ |
| 17 | `python/src/mesh2splat/cpu/CPUConverter.hpp` | ~40 | ✓ |
| 18 | `python/src/mesh2splat/cpu/CPUConverter.cpp` | ~165 | ✓ |
| 19 | `python/src/mesh2splat/gpu/HeadlessContext.hpp` | ~50 | ✓ |
| 20 | `python/src/mesh2splat/gpu/HeadlessContext.cpp` | ~300 | ✓ |
| 21 | `python/src/mesh2splat/gpu/ShaderManager.hpp` | ~50 | ✓ |
| 22 | `python/src/mesh2splat/gpu/ShaderManager.cpp` | ~500 | ✓ |
| 23 | `python/src/mesh2splat/gpu/GPUConverter.hpp` | ~50 | ✓ |
| 24 | `python/src/mesh2splat/gpu/GPUConverter.cpp` | ~530 | ✓ |
| 25 | `python/src/mesh2splat/Converter.hpp` | ~60 | ✓ |
| 26 | `python/src/mesh2splat/Converter.cpp` | ~275 | ✓ |
| 27 | `python/src/mesh2splat/Bindings.cpp` | ~310 | ✓ |

**Total: ~4,400 lines of code**
