# Pull Request: Python Bindings + Major Features + Bug Fixes

## Summary

This PR adds comprehensive Python bindings for headless mesh-to-Gaussian conversion, multiple major GUI features, and fixes 68+ bugs identified through code review.

---

## New Features

### Python Bindings (`mesh2splat` package)

A complete Python package for headless mesh-to-Gaussian splatting conversion:

- **GPU-accelerated conversion** using EGL headless OpenGL context (Linux)
- **CPU fallback** for systems without GPU support
- **GLTF/GLB loader** with full PBR material support (baseColor, metallic, roughness, normal maps, emissive)
- **PLY I/O** for reading/writing Gaussian splat files (standard + compressed formats)
- **Flip Y option** for SuperSplat compatibility (180° X-axis rotation)
- **NumPy integration** for efficient data interchange

```python
import mesh2splat

# GPU conversion
gaussians = mesh2splat.convert("model.glb", resolution=2048, samples_per_face=16)
mesh2splat.save_ply("output.ply", gaussians, flip_y=True)

# Or use the Converter class for fine-grained control
converter = mesh2splat.Converter(use_gpu=True)
converter.load_gltf("model.glb")
converter.convert(resolution=4096)
converter.save("output.ply")
```

### Build & Distribution

- **pip installable**: `pip install .` or build wheels
- **Docker builds** for manylinux2014, manylinux_2_28, Debian 12, Ubuntu 22.04/24.04
- **scikit-build-core** integration with CMake
- **Makefile** for common build tasks
- **GitHub Actions ready** configuration

### GUI Application Features

- **GLTF/GLB Import**: Full support for loading GLTF 2.0 models with embedded textures
- **Save All Formats**: Export to PLY (standard/compressed) and splat formats simultaneously
- **Flip Y Export**: 180° X-axis rotation for SuperSplat/web viewer compatibility
- **Orthogonal Projection**: Toggle between perspective and orthographic camera modes
- **F-Key Framing**: Press F to frame/focus on the loaded model
- **Linux Build Fixes**: EGL headless context, OpenGL function loader, cross-platform paths

### Conversion Improvements

- **Two-Pass Conversion System**: Improved gaussian generation quality
- **DcMode/OpacityMode Enums**: Configurable diffuse color and opacity handling
- **Resolution Scaling**: Proper handling of high-resolution conversion targets

---

## Bug Fixes

### Critical (7 issues)

1. **Dangling pointer in batch processing** - `BatchItem*` pointer invalidated when vector mutates; refactored to index-based access
2. **Unaligned memory access** - `reinterpret_cast` on unaligned GLTF buffer data; replaced with `memcpy`
3. **Integer overflow** - `int pixelCount = w * h` overflows at 8K resolution; changed to `size_t`
4. **Face normal corruption (CPU)** - Edge swap modified vectors before cross product; reordered computation
5. **Face normal corruption (GPU)** - Same bug in shader code generation; fixed
6. **Out-of-bounds index access** - GLTF face indices not bounds-checked; added validation
7. **GPU resource leak** - Exceptions left FBOs/textures leaked; added RAII cleanup

### High (20 issues)

8. **Incorrect emissiveFactor lookup** - Searched wrong map in GLTF material
9. **Uninitialized GL handles** - Multiple render passes had uninitialized GLuint members
10. **glUnmapBuffer on unmapped buffer** - Called on never-mapped SSBO
11. **Shader leak on compile failure** - Missing `glDeleteShader` on error path
12. **Deep copy of millions of gaussians** - `savePlyVector` took vector by value; changed to move semantics
13. **Uninitialized Gaussian3D members** - Default constructor left fields undefined
14. **Missing file I/O error checking** - PLY writer didn't check file open success
15. **Uninitialized memory access** - `reserve()` then index access instead of `push_back()`
16-27. Various memory management and error handling improvements

### Medium (14 issues)

28. **Deprecated `std::experimental::filesystem`** - Updated to C++17 `std::filesystem`
29. **ODR violation risk** - `static` functions in headers changed to `inline`
30. **O(N) vector erase for rolling buffer** - Changed to `std::deque` for O(1) `pop_front()`
31. **Hardcoded Windows path separator** - Changed to `std::filesystem::path` for cross-platform
32. **`#pragma once` in .cpp file** - Removed ineffective pragma
33-41. Const-correctness, unnecessary copies, logic improvements

### Low (12 issues)

- String parameters by value instead of const reference
- Redundant vector reallocations
- Magic numbers without named constants
- Missing `noexcept` specifications

### Style (15+ issues)

- Missing `override` keywords on virtual methods across all render passes
- Inconsistent naming and formatting

### GUI-Specific Fixes

- **Black screen bug** - FBO setup order issue + shader uniform typo (`u_isLightingEnalbed` → `u_isLightingEnabled`)
- **Example.py false error** - `PlyIO.save` returns None; fixed error handling

---

## Files Changed

### New Files (45+)

```
python/                          # Python bindings package
├── CMakeLists.txt
├── mesh2splat/__init__.py
├── example.py
└── src/mesh2splat/
    ├── Bindings.cpp
    ├── Converter.cpp/hpp
    ├── core/
    │   ├── GltfLoader.cpp/hpp
    │   ├── PlyIO.cpp/hpp
    │   ├── TextureSampler.cpp/hpp
    │   └── Types.cpp/hpp
    ├── cpu/
    │   ├── CPUConverter.cpp/hpp
    │   └── Rasterizer.cpp/hpp
    └── gpu/
        ├── GLLoader.cpp/hpp
        ├── GPUConverter.cpp/hpp
        ├── HeadlessContext.cpp/hpp
        └── ShaderManager.cpp/hpp

docker/                          # Multi-distro Docker builds
├── build.sh
├── Dockerfile.manylinux2014
├── Dockerfile.manylinux_2_28
├── Dockerfile.debian12
├── Dockerfile.ubuntu2204
└── Dockerfile.ubuntu2404

pyproject.toml                   # Python package config
Makefile                         # Build automation
BUILD_PYTHON.md                  # Build documentation
CODE_REVIEW.md                   # Review findings
.dockerignore
requirements.txt
```

### Modified Files (40+)

- `CMakeLists.txt` - Python bindings integration, Linux fixes
- `src/renderer/guiRendererConcreteMediator.cpp/hpp` - Batch processing fix
- `src/imGuiUi/ImGuiUI.cpp/hpp` - New features, deque optimization
- `src/utils/utils.hpp/cpp` - Filesystem updates, function inlining
- `src/utils/glUtils.cpp/hpp` - Shader deletion fix, unmapBuffer fix
- `src/parsers/parsers.cpp/hpp` - Move semantics, GLTF support
- `src/renderer/renderPasses/*.hpp` - GL handle init, override keywords
- `src/shaders/conversion/*.glsl` - Two-pass improvements
- `src/shaders/rendering/gaussianSplattingDeferredPS.glsl` - Typo fix

---

## Build Instructions

### Python Package

```bash
# Development install
pip install -e .

# Build wheel
pip wheel . -w dist/

# Docker builds (all distros)
cd docker && ./build.sh all
```

### C++ GUI

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Testing

- Tested GUI with multiple GLTF/GLB models
- Tested Python bindings with `duck.glb` and `small_lpg_tank_4k.gltf`
- Built wheels for Python 3.10, 3.11, 3.12 on multiple Linux distros
- CP312 wheels pass auditwheel (manylinux2014, manylinux_2_28 compliant)

---

## Breaking Changes

None - all changes are additive or fix existing bugs.

---

## Related Issues

- Fixes potential crashes from uninitialized GL handles
- Fixes memory corruption from dangling pointers in batch processing
- Fixes incorrect normals in converted gaussians
- Enables Linux builds that previously failed
