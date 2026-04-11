# Python bindings for Mesh2Splat
cmake_minimum_required(VERSION 3.15)

set(PYBIND11_DIR ${CMAKE_SOURCE_DIR}/thirdParty/pybind11)

# Find Python
find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)

# Source files for the binding library
set(MESH2SPLAT_PY_SOURCES
    src/mesh2splat/Bindings.cpp
)

# Create the Python module
pybind11_add_module(_mesh2splat MODULE ${MESH2SPLAT_PY_SOURCES})

# Include directories
target_include_directories(_mesh2splat PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdParty
    ${CMAKE_SOURCE_DIR}/thirdParty/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Note: Static linking (-static-libstdc++ -static-libgcc) is used in Docker builds
# for portable wheels, but disabled by default for local development

# For local development: output to python package directory
if(NOT MESH2SPLAT_STANDALONE_PYTHON)
    set_target_properties(_mesh2splat PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mesh2splat
    )
endif()

# For pip install: install the module to the correct location
# scikit-build-core will place these at the root of the wheel
install(TARGETS _mesh2splat LIBRARY DESTINATION .)

# Install the Python package files
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/mesh2splat/__init__.py
        DESTINATION .)
