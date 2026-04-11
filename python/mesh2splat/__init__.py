"""
Mesh2Splat: Fast mesh to 3D Gaussian splat conversion

This module provides Python bindings for converting 3D meshes (GLTF/GLB)
to 3D Gaussian splat format (PLY).
"""

from ._mesh2splat import (
    get_version,
    get_build_info,
    get_backend_name,
    get_available_backends,
    # Enums
    Backend,
    PlyFormat,
    RasterizationMode,
    DcMode,
    OpacityMode,
    # Core types
    Gaussian,
    ConversionOptions,
    ConversionResult,
    BBox,
    Material,
    Face,
    Mesh,
    Scene,
)

__version__ = get_version()
__all__ = [
    "get_version",
    "get_build_info",
    "get_backend_name",
    "get_available_backends",
    "Backend",
    "PlyFormat",
    "RasterizationMode",
    "DcMode",
    "OpacityMode",
    "Gaussian",
    "ConversionOptions",
    "ConversionResult",
    "BBox",
    "Material",
    "Face",
    "Mesh",
    "Scene",
]
