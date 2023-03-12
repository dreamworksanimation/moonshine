# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(component material_glitter)

set(installIncludeDir ${PACKAGE_NAME}/material/glitter)
set(exportGroup ${PROJECT_NAME}Targets)

add_library(${component} SHARED "")
add_library(${PROJECT_NAME}::${component} ALIAS ${component})

# ----------------------------------------
# compile some ispc sources to object files
set(objLib ${component}_objlib)

add_library(${objLib} OBJECT)

target_sources(${objLib}
    PRIVATE
        ispc/Glitter.ispc
)

file(RELATIVE_PATH relBinDir ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(${objLib} PROPERTIES
    ISPC_HEADER_SUFFIX _ispc_stubs.h
    ISPC_HEADER_DIRECTORY /${relBinDir}
    ISPC_INSTRUCTION_SETS avx1-i32x8
)

target_link_libraries(${objLib}
    PRIVATE
        ${PROJECT_NAME}::common_interpolation
        ${PROJECT_NAME}::common_noise
        ${PROJECT_NAME}::map_projection
        Moonray::bvh_shading_ispc
        Moonray::map_primvar
        Moonray::shading_ispc
        SceneRdl2::common_math
        SceneRdl2::common_platform
        SceneRdl2::render_util
        SceneRdl2::scene_rdl2
)

# Set standard compile/link options
Moonshine_ispc_compile_options(${objLib})

# ----------------------------------------

target_sources(${component}
    PRIVATE
        Glitter.cc
        # pull in our ispc object files
        $<TARGET_OBJECTS:${objLib}>
)

set_property(TARGET ${component}
    PROPERTY PUBLIC_HEADER
        Glitter.h
        ${CMAKE_CURRENT_BINARY_DIR}/Glitter_ispc_stubs.h
)

set_property(TARGET ${component}
    PROPERTY PRIVATE_HEADER
        ispc/Glitter.isph
)

target_include_directories(${component}
    PUBLIC
        # Find some ispc_stubs.h files
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${component}
    PRIVATE
        ${PROJECT_NAME}::common_interpolation
        Moonray::map_primvar
    PUBLIC
        ${PROJECT_NAME}::common_noise
        Moonray::rendering_bvh_shading
        Moonray::rendering_shading
        SceneRdl2::common_math
        SceneRdl2::scene_rdl2
        SceneRdl2::render_util
        SceneRdl2::render_logging
)

# If at Dreamworks add a SConscript stub file so others can use this library.
SConscript_Stub(${component})

# Set standard compile/link options
Moonshine_cxx_compile_definitions(${component})
Moonshine_cxx_compile_features(${component})
Moonshine_cxx_compile_options(${component})
Moonshine_link_options(${component})

# -------------------------------------
# Install the target and the export set
# -------------------------------------
include(GNUInstallDirs)

# install the target
install(TARGETS ${component}
    COMPONENT ${component}
    EXPORT ${exportGroup}
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        NAMELINK_SKIP
    RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${installIncludeDir}
    PRIVATE_HEADER
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${installIncludeDir}/ispc
)

# # install the export set
# install(
#     EXPORT ${exportGroup}
#     NAMESPACE ${PROJECT_NAME}::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
# )
