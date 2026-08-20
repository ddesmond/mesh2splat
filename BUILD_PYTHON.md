# Building Python Wheels for mesh2splat

This guide explains how to build Python wheel packages for mesh2splat.

## Quick Start

```bash
# Build macOS wheels (all Python versions)
make wheels-macos

# Build Linux wheels (all distros via Docker)
make wheels-linux

# Build a Windows GPU wheel from PowerShell
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip build
.\.venv\Scripts\python.exe -m build --wheel -o dist\windows

# Build everything
make wheels-all
```

## Requirements

### macOS

- Python 3.10, 3.11, 3.12 installed
- Xcode Command Line Tools (`xcode-select --install`)
- CMake 3.15+ (`brew install cmake`)

### Linux (via Docker)

- Docker installed and running
- No other dependencies needed (everything runs in containers)

### Windows

- Python 3.10, 3.11, or 3.12 installed
- CMake 3.15+
- Visual Studio 2022 with the "Desktop development with C++" workload
- OpenGL-compatible GPU and drivers

Windows Python wheels build with GPU support enabled by default. The GPU backend uses WGL for the headless OpenGL context and the bundled static GLEW library for OpenGL function loading.

## macOS Builds

### Build All Python Versions

```bash
make wheels-macos
```

Output: `dist/macos/*.whl`

### Build Specific Python Version

```bash
make wheels-macos-3.10
make wheels-macos-3.11
make wheels-macos-3.12
```

### Custom Python Paths

If your Python installations are not in the default locations:

```bash
# Using pyenv
PYTHON310=$(pyenv prefix 3.10.13)/bin/python make wheels-macos-3.10

# Using Homebrew
PYTHON310=/opt/homebrew/bin/python3.10 make wheels-macos-3.10

# Using system Python
PYTHON312=/usr/local/bin/python3.12 make wheels-macos-3.12
```

### Manual Build (without Make)

```bash
# Install build dependencies
python3.10 -m pip install build scikit-build-core pybind11

# Build wheel
python3.10 -m build --wheel -o dist/macos/
```

## Linux Builds (Docker)

See [GUIDE.md](GUIDE.md) for detailed Docker build instructions.

### Build All Linux Distros

```bash
make wheels-linux
```

Output: `dist/linux/<distro>/*.whl`

### Build Specific Distro

```bash
make wheels-linux-manylinux2014    # CentOS 7, glibc 2.17+ (broadest compat)
make wheels-linux-manylinux_2_28   # AlmaLinux 8, glibc 2.28+
make wheels-linux-debian12         # Debian 12 Bookworm
make wheels-linux-ubuntu2204       # Ubuntu 22.04 LTS
make wheels-linux-ubuntu2404       # Ubuntu 24.04 LTS
```

## Windows Builds

Run these commands from PowerShell in the repository root:

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip build
.\.venv\Scripts\python.exe -m build --wheel -o dist\windows
```

Output: `dist\windows\mesh2splat-0.1.0-cp312-cp312-win_amd64.whl`

To build without GPU support:

```powershell
.\.venv\Scripts\python.exe -m build --wheel -o dist\windows-cpu --config-setting=cmake.define.MESH2SPLAT_ENABLE_GPU=OFF
```

## Output Structure

After building, wheels are organized as:

```
dist/
├── macos/
│   ├── mesh2splat-0.1.0-cp310-cp310-macosx_12_0_arm64.whl
│   ├── mesh2splat-0.1.0-cp311-cp311-macosx_12_0_arm64.whl
│   └── mesh2splat-0.1.0-cp312-cp312-macosx_12_0_arm64.whl
└── linux/
    ├── manylinux2014/
    │   ├── mesh2splat-0.1.0-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
    │   ├── mesh2splat-0.1.0-cp311-cp311-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
    │   └── mesh2splat-0.1.0-cp312-cp312-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
    ├── manylinux_2_28/
    │   └── ...
    ├── debian12/
    │   └── mesh2splat-0.1.0-cp311-cp311-linux_x86_64.whl
    ├── ubuntu2204/
    │   └── mesh2splat-0.1.0-cp310-cp310-linux_x86_64.whl
    └── ubuntu2404/
        └── mesh2splat-0.1.0-cp312-cp312-linux_x86_64.whl
└── windows/
    └── mesh2splat-0.1.0-cp312-cp312-win_amd64.whl
```

## Installing Built Wheels

```bash
# Install specific wheel
pip install dist/macos/mesh2splat-0.1.0-cp310-cp310-macosx_12_0_arm64.whl

# Install with upgrade
pip install --upgrade dist/macos/mesh2splat-*.whl

# Verify installation
python -c "import mesh2splat; print(mesh2splat.get_build_info())"
```

On Windows:

```powershell
.\.venv\Scripts\python.exe -m pip install --force-reinstall dist\windows\mesh2splat-0.1.0-cp312-cp312-win_amd64.whl
.\.venv\Scripts\python.exe -c "import mesh2splat; print(mesh2splat.get_build_info()); print(mesh2splat.Converter.get_available_backends())"
```

## Testing

```bash
# Test installed wheel
make test

# Manual test
python -c "
import mesh2splat
print(f'Version: {mesh2splat.__version__}')
print(f'Build: {mesh2splat.get_build_info()}')
print(f'Backends: {mesh2splat.Converter.get_available_backends()}')
"
```

### Windows GPU Smoke Test

After installing the Windows wheel, this should report `GPU+CPU`, list both CPU and GPU backends, and initialize the default converter on GPU:

```powershell
.\.venv\Scripts\python.exe -c "import mesh2splat; print(mesh2splat.get_build_info()); print(mesh2splat.Converter.get_available_backends()); c=mesh2splat.Converter(); print(c, c.is_ready(), c.get_active_backend(), c.get_error_message())"
```

For an end-to-end conversion test, download a small glTF model and convert it:

```powershell
.\.venv\Scripts\python.exe -c "import mesh2splat; opts=mesh2splat.ConversionOptions(); opts.resolution=64; opts.backend=mesh2splat.Backend.GPU; result=mesh2splat.Converter(mesh2splat.Backend.GPU).convert_file('model.gltf', opts); print(result.success, result.used_backend, result.total_triangles, result.total_gaussians, result.error_message); mesh2splat.PlyIO.save('model_gpu_test.ply', result.gaussians) if result.success else exit(1)"
```

## Cleaning Build Artifacts

```bash
make clean
```

This removes:
- `build/` - CMake build directory
- `build-*/` - local platform-specific CMake build directories
- `dist/` - Built wheels
- `*.egg-info/` - Package metadata
- `python/mesh2splat/*.so` - Compiled extensions
- `wheelhouse/` - cibuildwheel output
- `test-assets/` - downloaded local test assets

## Troubleshooting

### "Python not found"

Ensure Python is installed and accessible:

```bash
# Check Python versions
which python3.10 python3.11 python3.12

# Or use pyenv
pyenv versions
```

### "CMake version too old"

Update CMake:

```bash
# macOS
brew upgrade cmake

# Linux
pip install --upgrade cmake
```

### "OpenGL headers not found" (Linux)

Install Mesa development packages:

```bash
# Debian/Ubuntu
sudo apt-get install libgl1-mesa-dev libegl1-mesa-dev

# RHEL/CentOS
sudo yum install mesa-libGL-devel mesa-libEGL-devel
```

### "GPU backend not available" (Windows)

Check that the wheel was built with GPU support:

```powershell
.\.venv\Scripts\python.exe -c "import mesh2splat; print(mesh2splat.get_build_info())"
```

The output should include `GPU+CPU [Windows]`. If it says `CPU`, rebuild without the CPU-only override and ensure Visual Studio, CMake, and GPU drivers are installed.

### Docker build fails

Ensure Docker is running and has enough resources:

```bash
# Check Docker status
docker info

# Clean Docker cache if needed
docker system prune -f
```

## PyPI Upload (Optional)

To upload wheels to PyPI:

```bash
# Install twine
pip install twine

# Upload to TestPyPI first
twine upload --repository testpypi dist/**/*.whl

# Upload to PyPI
twine upload dist/**/*.whl
```
