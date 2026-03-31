# Merge Plan: ECA Conversion Improvements into main-python

## Overview

This document outlines the plan to merge conversion improvements from ECA-Neuron's `push-local` branch into our `main-python` branch. The goal is to adopt their improved conversion pipeline while maintaining backward compatibility with our Python bindings API.

**Source:** `https://github.com/ECA-Neuron/mesh2splat.git` branch `push-local` (commit `f53622f`)  
**Target:** `main-python` branch  
**Date:** March 2026

---

## Summary of Changes

### What We're Adopting from ECA

| Feature | Description |
|---------|-------------|
| **Two-pass conversion** | Count fragments first, then allocate exact buffer size and write |
| **DcMode enum** | Configurable color encoding: SH0 (current), DirectLinear, DirectSrgb |
| **OpacityMode enum** | Configurable opacity encoding: Current, Raw, Logit (inverse sigmoid) |
| **scale → linearScale rename** | Clearer naming for linear-space scale values |
| **GaussianSplat module** | Structured data with BoundingBox, GaussianSplattingMode, pack/unpack helpers |
| **Debug counters** | 5-counter SSBO tracking conversion stats |
| **Stochastic sampling** | Hash-based random sampling for large meshes |
| **Safe scale export** | `safeLog()` to avoid `-inf` in PLY export |

### Key Decisions

| Decision | Choice |
|----------|--------|
| Adopt `GaussianSplat.h` structured data | **Yes** |
| Include debug counter logging | **Yes** (conditional via flag) |
| Two-pass conversion | **Default**, with single-pass as optional fallback |
| Python field naming | Keep `scale_x/y/z`, rename only internal `GaussianSSBO::scale` → `linearScale` |
| Expose DcMode/OpacityMode to Python | **Yes**, as new enums |

---

## Python API Compatibility

**No breaking changes to existing Python API:**

| Python Field | Status |
|--------------|--------|
| `Gaussian.scale_x` | ✅ Unchanged |
| `Gaussian.scale_y` | ✅ Unchanged |
| `Gaussian.scale_z` | ✅ Unchanged |
| `ConversionOptions.scale_multiplier` | ✅ Unchanged |

**New Python API additions:**

```python
# New enums
class DcMode(Enum):
    Current = 0      # SH0 encoding (default, matches original behavior)
    DirectLinear = 1 # Linear RGB directly
    DirectSrgb = 2   # sRGB values directly

class OpacityMode(Enum):
    Current = 0  # Format-specific default
    Raw = 1      # Raw opacity (0-1)
    Logit = 2    # Inverse sigmoid (standard for PLY)

# New ConversionOptions fields
options.dc_mode = DcMode.Current          # default
options.opacity_mode = OpacityMode.Logit  # default
```

---

## Phase 1: Core C++ Changes (src/)

### 1.1 Add GaussianSplat Module

**New Files:**
- `src/parsers/GaussianSplat.h`
- `src/parsers/GaussianSplat.cpp`

**Contents (from ECA):**
- `BoundingBox` struct with `grow()`, `getTransformedAabb()` helpers
- `GaussianSplattingMode` enum: `Unknown`, `GeometryOnly`, `Classic`, `PBR`
- `packScale()` / `unpackScale()` - log/exp scale conversion
- `sigmoid()` / `inverseSigmoid()` helpers
- `SortKey64` for spatial sorting (future streaming support)

### 1.2 Update Data Structures

**File:** `src/utils/utils.hpp`

Changes:
- Add `#define GLM_FORCE_XYZW_ONLY 1` before GLM includes
- Rename `GaussianDataSSBO::scale` → `GaussianDataSSBO::linearScale`
- Add new fields to `TextureInfo`:
  ```cpp
  int textureIndex = -1;
  int imageIndex = -1;
  int samplerIndex = -1;
  int wrapS = 0;
  int wrapT = 0;
  int minFilter = 0;
  int magFilter = 0;
  std::string mimeType;
  ```
- Add new fields to `Mesh`:
  ```cpp
  std::string sourceName;
  int primitiveIndex = -1;
  int materialIndex = -1;
  struct UVAccessorInfo { ... } uvAccessor;
  ```

**File:** `src/utils/utils.cpp`

Changes:
- Update `isValid()` to check `g.linearScale` instead of `g.scale`
- Fix `.rgb/.r/.g/.b` → `.xyz/.x/.y/.z` accessors throughout

### 1.3 Add DcMode/OpacityMode Enums

**File:** `src/parsers/parsers.hpp`

Add:
```cpp
enum class DcMode : uint32_t {
    Current = 0,      // SH0 encoding
    DirectLinear = 1,
    DirectSrgb = 2
};

enum class OpacityMode : uint32_t {
    Current = 0,
    Raw = 1,
    Logit = 2
};
```

Update function signatures:
```cpp
void writePbrPLY(..., DcMode dcMode, OpacityMode opacityMode);
void writeBinaryPlyStandardFormat(..., DcMode dcMode, OpacityMode opacityMode);
void saveSplatVector(..., DcMode dcMode, OpacityMode opacityMode);  // renamed from savePlyVector
```

**File:** `src/parsers/parsers.cpp`

Add helper functions:
```cpp
static glm::vec3 computeDcFromColor(const glm::vec3& colorLinear, DcMode dcMode) {
    switch (dcMode) {
        case DcMode::DirectLinear: return colorLinear;
        case DcMode::DirectSrgb:   return utils::linear_to_srgb_float(colorLinear);
        case DcMode::Current:
        default:                   return utils::getShFromColor(colorLinear);
    }
}

static float encodeOpacity(float opacityLinear, OpacityMode mode, bool defaultLogit) {
    bool useLogit = defaultLogit;
    if (mode == OpacityMode::Raw) useLogit = false;
    else if (mode == OpacityMode::Logit) useLogit = true;
    
    if (!useLogit) return opacityLinear;
    
    // Inverse sigmoid
    const float eps = 1e-6f;
    float a = std::clamp(opacityLinear, eps, 1.0f - eps);
    return std::log(a / (1.0f - a));
}
```

Update PLY export functions to:
- Use `computeDcFromColor()` for color encoding
- Use `encodeOpacity()` for opacity encoding
- Use `safeLog()` for scale: `std::log(std::max(scale, 1e-12f))`
- Reference `gaussian.linearScale` instead of `gaussian.scale`

### 1.4 Update RenderContext

**File:** `src/renderer/renderPasses/RenderContext.hpp`

Add new fields:
```cpp
// Conversion settings
uint32_t maxSplats = 4000000;
bool useTwoPassConversion = true;  // Default to two-pass, false for single-pass fallback

// Debug flags
bool debugUv = false;
bool debugColor = false;
bool debugTextureStats = false;
bool debugColorStats = false;
bool debugConversionLogging = true;  // Controls std::cout logging

// Color/opacity encoding
int dcMode = 0;      // 0=SH0, 1=DirectLinear, 2=DirectSrgb
int opacityMode = 2; // 0=Current, 1=Raw, 2=Logit

// New buffers
GLuint conversionDebugCounters = 0;
GLuint debugPrimIdBuffer = 0;
```

Remove unused split-screen fields:
```cpp
// DELETE: meshGBufferFBO, meshGPosition, meshGNormal, meshGAlbedo, 
//         meshGDepth, meshGMetallicRoughness, meshGDepthRBO,
//         splitScreenEnabled, splitScreenPosition
```

### 1.5 Update ConversionPass

**File:** `src/renderer/renderPasses/ConversionPass.cpp`

Implement two-pass conversion with single-pass fallback:

```cpp
void ConversionPass::execute(RenderContext& renderContext) {
    // ... setup ...
    
    auto runPass = [&](uint32_t res, bool countOnly, float sampleProb, 
                       uint32_t seed, uint32_t maxSplats) -> uint32_t {
        // Reset atomic counter
        // Set uniforms: u_countOnly, u_sampleProb, u_hashSeed, u_maxSplats
        // Setup framebuffer
        // Draw all meshes
        // Read back counter
        // Cleanup framebuffer
        return count;
    };
    
    if (renderContext.useTwoPassConversion) {
        // Pass 1: Count only
        uint32_t candidateCount = runPass(requestedRes, true, 1.0f, seed, 1u);
        
        // Allocate exact buffer size
        GLsizeiptr bufferSize = candidateCount * sizeof(utils::GaussianDataSSBO);
        // ... resize gaussianBuffer ...
        
        // Pass 2: Write gaussians
        uint32_t written = runPass(requestedRes, false, 1.0f, seed + 1, candidateCount);
        renderContext.numberOfGaussians = written;
    } else {
        // Single-pass fallback (original behavior)
        uint32_t maxGaussians = estimateMaxGaussians(renderContext);
        // ... allocate buffer ...
        uint32_t written = runPass(requestedRes, false, 1.0f, seed, maxGaussians);
        renderContext.numberOfGaussians = written;
    }
    
    // Conditional logging
    if (renderContext.debugConversionLogging) {
        std::cout << "[mesh2splat] ConversionPass: candidates=" << candidateCount
                  << " written=" << written << std::endl;
    }
}
```

### 1.6 Update Shaders

**File:** `src/shaders/conversion/converterVS.glsl`

Add at end of `main()`:
```glsl
// Required for proper primitive assembly
gl_Position = vec4(position, 1.0);
```

**File:** `src/shaders/conversion/converterFS.glsl`

Add new uniforms:
```glsl
uniform int u_countOnly;
uniform float u_sampleProb;
uniform uint u_hashSeed;
uniform uint u_maxSplats;
uniform int u_writePrimId;
uniform uint u_debugPrimId;
```

Add new SSBOs:
```glsl
layout(std430, binding = 7) buffer ConversionCounters {
    uint counters[];  // [candidates, accepted, attemptedWrites, written, oobRejected]
} conversionCounters;

layout(std430, binding = 8) buffer PrimIdBuffer {
    uint primIds[];
} primIdBuffer;
```

Add output:
```glsl
layout(location = 0) out vec4 dummyColor;
```

Add hash function:
```glsl
float randFromSeed(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return float(v.x) / 4294967295.0;
}
```

Update `main()`:
```glsl
void main() {
    dummyColor = vec4(0.0);
    
    atomicAdd(conversionCounters.counters[0], 1u);  // candidates
    
    // Stochastic sampling
    uvec3 seed = uvec3(uint(gl_FragCoord.x), uint(gl_FragCoord.y), u_hashSeed);
    if (randFromSeed(seed) > u_sampleProb) {
        discard;
    }
    
    atomicAdd(conversionCounters.counters[1], 1u);  // accepted
    
    if (u_countOnly != 0) {
        return;  // Count-only pass, don't write
    }
    
    atomicAdd(conversionCounters.counters[2], 1u);  // attemptedWrites
    
    uint index = atomicCounterIncrement(g_validCounter);
    if (index >= u_maxSplats) {
        atomicAdd(conversionCounters.counters[4], 1u);  // oobRejected
        discard;
    }
    
    // ... existing gaussian write logic ...
    
    atomicAdd(conversionCounters.counters[3], 1u);  // written
    dummyColor = vec4(1.0);
}
```

### 1.7 Update glUtils

**File:** `src/utils/glUtils.hpp`

Remove:
- `MeshRenderProgram` from `ShaderProgramTypes` enum
- `meshRenderVertexShaderLocation`, `meshRenderFragmentShaderLocation` from `ShaderLocations`
- `commonShaderLocation` from `ShaderLocations`
- `resolveIncludes()` function declaration
- `dependencies` field from `ShaderProgramInfo`

**File:** `src/utils/glUtils.cpp`

Remove:
- `resolveIncludes()` function implementation
- Mesh render shader initialization

---

## Phase 2: Python Bindings (python/)

### 2.1 Update Core Types

**File:** `python/src/mesh2splat/core/Types.hpp`

Add enums:
```cpp
enum class DcMode {
    Current = 0,
    DirectLinear = 1,
    DirectSrgb = 2
};

enum class OpacityMode {
    Current = 0,
    Raw = 1,
    Logit = 2
};
```

Rename field in GaussianSSBO:
```cpp
struct alignas(16) GaussianSSBO {
    glm::vec4 position;
    glm::vec4 color;
    glm::vec4 linearScale;  // renamed from 'scale'
    glm::vec4 normal;
    glm::vec4 rotation;
    glm::vec4 pbr;
};
```

Update ConversionOptions:
```cpp
struct ConversionOptions {
    // ... existing fields ...
    DcMode dcMode = DcMode::Current;
    OpacityMode opacityMode = OpacityMode::Logit;
};
```

### 2.2 Update Types Implementation

**File:** `python/src/mesh2splat/core/Types.cpp`

Update `Gaussian::fromSSBO()`:
```cpp
// Scale - use linearScale
g.scale_x = ssbo.linearScale.x;
g.scale_y = ssbo.linearScale.y;
g.scale_z = ssbo.linearScale.z;
```

Update `Gaussian::toSSBO()`:
```cpp
ssbo.linearScale = glm::vec4(scale_x, scale_y, scale_z, 0.0f);
```

Add helper functions:
```cpp
glm::vec3 computeDcFromColor(const glm::vec3& colorLinear, DcMode dcMode);
float encodeOpacity(float opacity, OpacityMode mode);
```

### 2.3 Update PLY I/O

**File:** `python/src/mesh2splat/core/PlyIO.cpp`

Update function signatures to accept `DcMode` and `OpacityMode`:
```cpp
void writePlyStandard(const std::string& filename, 
                      const std::vector<GaussianSSBO>& gaussians,
                      float scaleMultiplier,
                      DcMode dcMode,
                      OpacityMode opacityMode);
```

Update all `gaussian.scale` → `gaussian.linearScale` references.

Use `safeLog()` for scale export:
```cpp
auto safeLog = [](float v) -> float {
    return std::log(std::max(v, 1e-12f));
};
```

### 2.4 Update Converters

**Files:**
- `python/src/mesh2splat/cpu/CPUConverter.cpp`
- `python/src/mesh2splat/cpu/Rasterizer.cpp`
- `python/src/mesh2splat/gpu/GPUConverter.cpp`
- `python/src/mesh2splat/gpu/ShaderManager.cpp`

Changes:
- Update all `ssbo.scale` → `ssbo.linearScale` references
- Pass `DcMode` and `OpacityMode` through to PLY export

### 2.5 Update Python Bindings

**File:** `python/src/mesh2splat/Bindings.cpp`

Add enum bindings:
```cpp
py::enum_<DcMode>(m, "DcMode", "Color encoding mode for PLY export")
    .value("Current", DcMode::Current, "SH0 encoding (default, standard 3DGS)")
    .value("DirectLinear", DcMode::DirectLinear, "Linear RGB values directly")
    .value("DirectSrgb", DcMode::DirectSrgb, "sRGB values directly")
    .export_values();

py::enum_<OpacityMode>(m, "OpacityMode", "Opacity encoding mode for PLY export")
    .value("Current", OpacityMode::Current, "Use format-specific default")
    .value("Raw", OpacityMode::Raw, "Raw opacity value (0-1)")
    .value("Logit", OpacityMode::Logit, "Inverse sigmoid (standard for PLY)")
    .export_values();
```

Add to ConversionOptions binding:
```cpp
.def_readwrite("dc_mode", &ConversionOptions::dcMode,
    "Color encoding mode (default: Current/SH0)")
.def_readwrite("opacity_mode", &ConversionOptions::opacityMode,
    "Opacity encoding mode (default: Logit)")
```

### 2.6 Update Python Package

**File:** `python/mesh2splat/__init__.py`

Add to imports:
```python
from ._mesh2splat import (
    # ... existing ...
    DcMode,
    OpacityMode,
)
```

Add to `__all__`:
```python
__all__ = [
    # ... existing ...
    "DcMode",
    "OpacityMode",
]
```

---

## Phase 3: Testing & Validation

### Build Tests

- [ ] Build main executable on macOS
- [ ] Build Python wheel with CPU backend
- [ ] Build Python wheel with GPU backend
- [ ] All builds complete without errors

### Functional Tests

- [ ] Run `python/example.py` without modifications (backward compat)
- [ ] Test `DcMode.Current` produces same output as before
- [ ] Test `DcMode.DirectLinear` produces linear RGB in PLY
- [ ] Test `DcMode.DirectSrgb` produces sRGB values in PLY
- [ ] Test `OpacityMode.Logit` applies inverse sigmoid
- [ ] Test `OpacityMode.Raw` writes raw opacity values

### Conversion Tests

- [ ] Two-pass conversion produces same results as single-pass
- [ ] CPU backend output matches GPU backend (within tolerance)
- [ ] Output PLY files load correctly in 3DGS viewers
- [ ] Debug logging outputs correct statistics

### Regression Tests

- [ ] Convert DamagedHelmet.glb → PLY (CPU)
- [ ] Convert DamagedHelmet.glb → PLY (GPU)
- [ ] Compare output file sizes and gaussian counts

---

## Phase 4: Documentation & Commits

### Commit Sequence

| # | Commit Message | Files |
|---|----------------|-------|
| 1 | `Add GaussianSplat module with BoundingBox and mode enums` | `src/parsers/GaussianSplat.{h,cpp}` |
| 2 | `Rename scale to linearScale in GaussianDataSSBO` | `src/utils/utils.{hpp,cpp}` |
| 3 | `Add DcMode and OpacityMode enums for PLY export` | `src/parsers/parsers.{hpp,cpp}` |
| 4 | `Update RenderContext with conversion options and debug flags` | `src/renderer/renderPasses/RenderContext.hpp` |
| 5 | `Implement two-pass conversion with single-pass fallback` | `src/renderer/renderPasses/ConversionPass.cpp` |
| 6 | `Update shaders for count-only mode and sampling` | `src/shaders/conversion/*.glsl` |
| 7 | `Clean up glUtils - remove unused mesh render code` | `src/utils/glUtils.{hpp,cpp}` |
| 8 | `Update Python bindings with linearScale rename` | `python/src/mesh2splat/**` |
| 9 | `Add DcMode and OpacityMode to Python API` | `python/src/mesh2splat/Bindings.cpp`, `python/mesh2splat/__init__.py` |
| 10 | `Update documentation and examples` | `PLAN.md`, `python/example.py` |

### Documentation Updates

- [ ] Update `PLAN.md` with merge completion status
- [ ] Update `python/example.py` to demonstrate new options
- [ ] Add docstrings for new enums and options

---

## Files Summary

### Core C++ (src/)

| File | Action |
|------|--------|
| `src/parsers/GaussianSplat.h` | **ADD** |
| `src/parsers/GaussianSplat.cpp` | **ADD** |
| `src/parsers/parsers.hpp` | MODIFY |
| `src/parsers/parsers.cpp` | MODIFY |
| `src/utils/utils.hpp` | MODIFY |
| `src/utils/utils.cpp` | MODIFY |
| `src/utils/glUtils.hpp` | MODIFY |
| `src/utils/glUtils.cpp` | MODIFY |
| `src/renderer/renderPasses/RenderContext.hpp` | MODIFY |
| `src/renderer/renderPasses/ConversionPass.cpp` | MODIFY |
| `src/shaders/conversion/converterVS.glsl` | MODIFY |
| `src/shaders/conversion/converterFS.glsl` | MODIFY |

### Python Bindings (python/)

| File | Action |
|------|--------|
| `python/src/mesh2splat/core/Types.hpp` | MODIFY |
| `python/src/mesh2splat/core/Types.cpp` | MODIFY |
| `python/src/mesh2splat/core/PlyIO.cpp` | MODIFY |
| `python/src/mesh2splat/cpu/CPUConverter.cpp` | MODIFY |
| `python/src/mesh2splat/cpu/Rasterizer.cpp` | MODIFY |
| `python/src/mesh2splat/gpu/GPUConverter.cpp` | MODIFY |
| `python/src/mesh2splat/gpu/ShaderManager.cpp` | MODIFY |
| `python/src/mesh2splat/Converter.hpp` | MODIFY |
| `python/src/mesh2splat/Bindings.cpp` | MODIFY |
| `python/mesh2splat/__init__.py` | MODIFY |

**Total: 22 files** (2 new, 20 modified)

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| `linearScale` rename breaks internal code | Medium | Systematic search/replace; compiler catches misses |
| Two-pass changes conversion behavior | Low | Compare output against single-pass baseline |
| New shader uniforms cause GPU issues | Low | Test on macOS OpenGL 4.1 |
| Python backward compat broken | High | Keep `scale_x/y/z` names; only internal SSBO changes |

---

## Appendix: ECA Files Reference

Key files from ECA `push-local` to reference:
- `f53622f:src/parsers/GaussianSplat.h` - BoundingBox, mode enums
- `f53622f:src/parsers/parsers.cpp` - DcMode/OpacityMode implementation
- `f53622f:src/renderer/renderPasses/ConversionPass.cpp` - Two-pass logic
- `f53622f:src/shaders/conversion/converterFS.glsl` - Shader changes

Use `git show eca/push-local:<path>` to view these files.
