///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - pybind11 Bindings                   //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include <pybind11/pybind11.h>
#include <string>

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
    
    m.def("get_version", &getVersion, "Get library version");
    m.def("get_build_info", &getBuildInfo, "Get build information");
}

} // namespace mesh2splat
