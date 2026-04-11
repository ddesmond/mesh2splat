///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - pybind11 Bindings                   //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "core/Types.hpp"

namespace py = pybind11;

namespace mesh2splat {

std::string getVersion() {
    return "0.1.0";
}

std::string getBuildInfo() {
    std::string info = "mesh2splat ";
    info += getVersion();
#ifdef MESH2SPLAT_ENABLE_GPU
    info += " [GPU enabled]";
#else
    info += " [CPU only]";
#endif
    return info;
}

PYBIND11_MODULE(_mesh2splat, m) {
    m.doc() = "Mesh2Splat: Fast mesh to 3D Gaussian splat conversion";
    
    //--------------------------------------------------------------------------
    // Enums
    //--------------------------------------------------------------------------
    
    py::enum_<Backend>(m, "Backend", "Backend selection for conversion")
        .value("Auto", Backend::Auto, "Automatically select best available backend (GPU preferred)")
        .value("CPU", Backend::CPU, "CPU-only backend (portable, always available)")
        .value("GPU", Backend::GPU, "GPU backend using OpenGL (faster, requires headless context)")
        .export_values();
    
    py::enum_<PlyFormat>(m, "PlyFormat", "Output PLY format")
        .value("Standard", PlyFormat::Standard, "Standard 3DGS PLY format")
        .value("PBR", PlyFormat::PBR, "Extended format with metallic/roughness/AO")
        .value("Compressed", PlyFormat::Compressed, "Compressed format")
        .export_values();
    
    py::enum_<RasterizationMode>(m, "RasterizationMode", "Rasterization mode for conversion")
        .value("UV", RasterizationMode::UV, "Rasterize in original mesh UV space (texture-based)")
        .value("Projection", RasterizationMode::Projection, "Rasterize using orthogonal projection (triplanar)")
        .export_values();
    
    py::enum_<DcMode>(m, "DcMode", "DC (color) encoding mode")
        .value("Current", DcMode::Current, "SH0 encoding (default, matches original behavior)")
        .value("DirectLinear", DcMode::DirectLinear, "Linear RGB directly")
        .value("DirectSrgb", DcMode::DirectSrgb, "sRGB values directly")
        .export_values();
    
    py::enum_<OpacityMode>(m, "OpacityMode", "Opacity encoding mode")
        .value("Current", OpacityMode::Current, "Format-specific default")
        .value("Raw", OpacityMode::Raw, "Raw opacity (0-1)")
        .value("Logit", OpacityMode::Logit, "Inverse sigmoid (standard for PLY)")
        .export_values();
    
    //--------------------------------------------------------------------------
    // ConversionOptions
    //--------------------------------------------------------------------------
    
    py::class_<ConversionOptions>(m, "ConversionOptions", "Options for mesh to splat conversion")
        .def(py::init<>())
        .def_readwrite("resolution", &ConversionOptions::resolution,
            "Resolution of UV space rasterization (default: 512)")
        .def_readwrite("ply_format", &ConversionOptions::plyFormat,
            "Output PLY format (default: Standard)")
        .def_readwrite("scale_multiplier", &ConversionOptions::scaleMultiplier,
            "Scale multiplier for gaussian scales (default: 1.0)")
        .def_readwrite("srgb_conversion", &ConversionOptions::srgbConversion,
            "Whether to use sRGB color space conversion (default: True)")
        .def_readwrite("backend", &ConversionOptions::backend,
            "Backend selection (default: Auto)")
        .def_readwrite("rasterization_mode", &ConversionOptions::rasterizationMode,
            "Rasterization mode: UV (texture-based) or Projection (triplanar) (default: UV)")
        .def_readwrite("dc_mode", &ConversionOptions::dcMode,
            "DC (color) encoding mode (default: Current/SH0)")
        .def_readwrite("opacity_mode", &ConversionOptions::opacityMode,
            "Opacity encoding mode (default: Logit)")
        .def_readwrite("verbose", &ConversionOptions::verbose,
            "Enable verbose logging (default: False)")
        .def_readwrite("flip_y", &ConversionOptions::flipY,
            "Apply 180-degree X-axis rotation for SuperSplat/viewer compatibility (default: True)")
        .def("__repr__", [](const ConversionOptions& o) {
            std::string mode_str = (o.rasterizationMode == RasterizationMode::UV) ? "UV" : "Projection";
            return "<ConversionOptions resolution=" + std::to_string(o.resolution) + 
                   " mode=" + mode_str + ">";
        });
    
    //--------------------------------------------------------------------------
    // Gaussian
    //--------------------------------------------------------------------------
    
    py::class_<Gaussian>(m, "Gaussian", "A single 3D Gaussian splat")
        .def(py::init<>())
        .def_readwrite("x", &Gaussian::x, "X position")
        .def_readwrite("y", &Gaussian::y, "Y position")
        .def_readwrite("z", &Gaussian::z, "Z position")
        .def_readwrite("r", &Gaussian::r, "Red SH0 coefficient")
        .def_readwrite("g", &Gaussian::g, "Green SH0 coefficient")
        .def_readwrite("b", &Gaussian::b, "Blue SH0 coefficient")
        .def_readwrite("opacity", &Gaussian::opacity, "Opacity (0-1)")
        .def_readwrite("scale_x", &Gaussian::scale_x, "X scale")
        .def_readwrite("scale_y", &Gaussian::scale_y, "Y scale")
        .def_readwrite("scale_z", &Gaussian::scale_z, "Z scale")
        .def_readwrite("rot_x", &Gaussian::rot_x, "Rotation quaternion X")
        .def_readwrite("rot_y", &Gaussian::rot_y, "Rotation quaternion Y")
        .def_readwrite("rot_z", &Gaussian::rot_z, "Rotation quaternion Z")
        .def_readwrite("rot_w", &Gaussian::rot_w, "Rotation quaternion W")
        .def_readwrite("nx", &Gaussian::nx, "Normal X")
        .def_readwrite("ny", &Gaussian::ny, "Normal Y")
        .def_readwrite("nz", &Gaussian::nz, "Normal Z")
        .def_readwrite("metallic", &Gaussian::metallic, "Metallic (PBR)")
        .def_readwrite("roughness", &Gaussian::roughness, "Roughness (PBR)")
        .def_readwrite("ao", &Gaussian::ao, "Ambient occlusion (PBR)")
        .def("is_valid", &Gaussian::isValid, "Check if gaussian is valid")
        .def("__repr__", [](const Gaussian& g) {
            return "<Gaussian pos=(" + std::to_string(g.x) + "," + 
                   std::to_string(g.y) + "," + std::to_string(g.z) + ")>";
        });
    
    //--------------------------------------------------------------------------
    // ConversionResult
    //--------------------------------------------------------------------------
    
    py::class_<ConversionResult>(m, "ConversionResult", "Result of mesh to splat conversion")
        .def(py::init<>())
        .def_readonly("gaussians", &ConversionResult::gaussians, "List of gaussians")
        .def_readonly("total_triangles", &ConversionResult::totalTriangles, "Total input triangles")
        .def_readonly("total_gaussians", &ConversionResult::totalGaussians, "Total output gaussians")
        .def_readonly("conversion_time_ms", &ConversionResult::conversionTimeMs, "Conversion time in milliseconds")
        .def_readonly("used_backend", &ConversionResult::usedBackend, "Backend that was used")
        .def_readonly("messages", &ConversionResult::messages, "Info/warning messages")
        .def_readonly("success", &ConversionResult::success, "Whether conversion succeeded")
        .def_readonly("error_message", &ConversionResult::errorMessage, "Error message if failed")
        .def("__repr__", [](const ConversionResult& r) {
            if (r.success) {
                return "<ConversionResult success=True gaussians=" + 
                       std::to_string(r.totalGaussians) + " time=" + 
                       std::to_string(r.conversionTimeMs) + "ms>";
            } else {
                return "<ConversionResult success=False error=\"" + r.errorMessage + "\">";
            }
        });
    
    //--------------------------------------------------------------------------
    // BBox
    //--------------------------------------------------------------------------
    
    py::class_<BBox>(m, "BBox", "Axis-aligned bounding box")
        .def(py::init<>())
        .def_readonly("min", &BBox::min, "Minimum corner")
        .def_readonly("max", &BBox::max, "Maximum corner")
        .def("center", &BBox::center, "Get center point")
        .def("size", &BBox::size, "Get size");
    
    //--------------------------------------------------------------------------
    // Material
    //--------------------------------------------------------------------------
    
    py::class_<Material>(m, "Material", "PBR material properties")
        .def(py::init<>())
        .def_readonly("name", &Material::name)
        .def_readonly("metallic_factor", &Material::metallicFactor)
        .def_readonly("roughness_factor", &Material::roughnessFactor);
    
    //--------------------------------------------------------------------------
    // Face
    //--------------------------------------------------------------------------
    
    py::class_<Face>(m, "Face", "Triangle face with vertex attributes")
        .def(py::init<>());
    
    //--------------------------------------------------------------------------
    // Mesh
    //--------------------------------------------------------------------------
    
    py::class_<Mesh>(m, "Mesh", "Mesh primitive with material")
        .def(py::init<>())
        .def_readonly("name", &Mesh::name)
        .def_readonly("faces", &Mesh::faces)
        .def_readonly("material", &Mesh::material)
        .def_readonly("surface_area", &Mesh::surfaceArea)
        .def_readonly("bbox", &Mesh::bbox)
        .def("__repr__", [](const Mesh& m) {
            return "<Mesh name=\"" + m.name + "\" faces=" + std::to_string(m.faces.size()) + ">";
        });
    
    //--------------------------------------------------------------------------
    // Scene
    //--------------------------------------------------------------------------
    
    py::class_<Scene>(m, "Scene", "Complete scene with multiple meshes")
        .def(py::init<>())
        .def_readonly("meshes", &Scene::meshes)
        .def_readonly("bbox", &Scene::bbox)
        .def_readonly("source_path", &Scene::sourcePath)
        .def("__repr__", [](const Scene& s) {
            return "<Scene meshes=" + std::to_string(s.meshes.size()) + " source=\"" + s.sourcePath + "\">";
        });
    
    //--------------------------------------------------------------------------
    // Module-level functions
    //--------------------------------------------------------------------------
    
    m.def("get_version", &getVersion, "Get library version");
    m.def("get_build_info", &getBuildInfo, "Get build information");
    m.def("get_backend_name", &getBackendName, "Get backend name as string");
    m.def("get_available_backends", &getAvailableBackends, "Get list of available backends");
}

} // namespace mesh2splat
