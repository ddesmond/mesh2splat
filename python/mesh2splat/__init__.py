"""
mesh2splat - Python bindings for Mesh2Splat

Convert 3D meshes to Gaussian splats for 3D Gaussian Splatting.

Example usage:
    import mesh2splat

    # Simple conversion
    mesh2splat.convert("model.gltf", "output.ply")

    # With options
    options = mesh2splat.ConversionOptions()
    options.resolution = 1024
    options.backend = mesh2splat.Backend.CPU

    converter = mesh2splat.Converter()
    result = converter.convert_file("model.gltf", options)

    if result.success:
        print(f"Generated {result.total_gaussians} gaussians")
        mesh2splat.PlyIO.save("output.ply", result.gaussians)

        # Access as numpy arrays
        arrays = mesh2splat.gaussians_to_numpy(result.gaussians)
        positions = arrays["positions"]  # (N, 3) float32
        colors = arrays["colors"]        # (N, 3) float32
"""

from ._mesh2splat import (
    # Enums
    Backend,
    PlyFormat,
    RasterizationMode,
    DcMode,
    OpacityMode,
    # Core types
    Gaussian,
    ConversionResult,
    ConversionOptions,
    BBox,
    Material,
    Face,
    Mesh,
    Scene,
    # Main classes
    Converter,
    PlyIO,
    GltfLoader,
    # Module functions
    convert,
    get_version,
    get_build_info,
    gaussians_to_numpy,
)

__version__ = get_version()

__all__ = [
    # Enums
    "Backend",
    "PlyFormat",
    "RasterizationMode",
    "DcMode",
    "OpacityMode",
    # Core types
    "Gaussian",
    "ConversionResult",
    "ConversionOptions",
    "BBox",
    "Material",
    "Face",
    "Mesh",
    "Scene",
    # Main classes
    "Converter",
    "PlyIO",
    "GltfLoader",
    # Functions
    "convert",
    "get_version",
    "get_build_info",
    "gaussians_to_numpy",
]
