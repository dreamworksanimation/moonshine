# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(component material_dwabase)

set(installIncludeDir ${PACKAGE_NAME}/material/dwabase)
set(exportGroup ${PROJECT_NAME}Targets)

add_library(${component} SHARED "")
add_library(${PROJECT_NAME}::${component} ALIAS ${component})

# ----------------------------------------
# compile some ispc sources to object files
set(objLib ${component}_objlib)

add_library(${objLib} OBJECT)

target_sources(${objLib}
    PRIVATE
        ispc/Blending.ispc
        ispc/DwaBase.ispc
        ispc/DwaBaseLayerable.ispc
)

file(RELATIVE_PATH relBinDir ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(${objLib} PROPERTIES
    ISPC_HEADER_SUFFIX _ispc_stubs.h
    ISPC_HEADER_DIRECTORY /${relBinDir}
    ISPC_INSTRUCTION_SETS ${GLOBAL_ISPC_INSTRUCTION_SETS}
    LINKER_LANGUAGE CXX
)

target_link_libraries(${objLib}
    PRIVATE
        ${PROJECT_NAME}::material_glitter
        Moonray::rendering_shading
        Moonray::shading_ispc
        SceneRdl2::common_platform
)

# Set standard compile/link options
Moonshine_ispc_compile_options(${objLib})

get_target_property(objLibDeps ${objLib} DEPENDENCY)
if(NOT objLibDeps STREQUAL "")
    add_dependencies(${objLibDeps} 
        ${PROJECT_NAME}::material_glitter
        Moonray::rendering_shading
        Moonray::shading_ispc
        SceneRdl2::common_platform
    )
endif()

# ----------------------------------------

get_target_property(ISPC_TARGET_OBJECTS ${objLib} TARGET_OBJECTS)
target_sources(${component}
    PRIVATE
        Blending.cc
        DwaBase.cc
        DwaBaseLayerable.cc
        # pull in our ispc object files
        ${ISPC_TARGET_OBJECTS}
)

set_property(TARGET ${component}
    PROPERTY PUBLIC_HEADER
        Blending.h
        DwaBase.h
        DwaBaseLayerable.h
        ${CMAKE_CURRENT_BINARY_DIR}/Blending_ispc_stubs.h
        ${CMAKE_CURRENT_BINARY_DIR}/DwaBase_ispc_stubs.h
        ${CMAKE_CURRENT_BINARY_DIR}/DwaBaseLayerable_ispc_stubs.h
)

set_property(TARGET ${component}
    PROPERTY PRIVATE_HEADER
        ispc/Blending.isph
        ispc/DwaBase.isph
        ispc/DwaBaseLayerable.isph
)

target_include_directories(${component}
    PUBLIC
        # Find some ispc_stubs.h files
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${component}
    PUBLIC
        ${PROJECT_NAME}::material_glitter
        Moonray::common_mcrt_macros
        Moonray::rendering_shading
        Moonray::shading_ispc
        SceneRdl2::common_math
        SceneRdl2::common_platform
        SceneRdl2::scene_rdl2
)

add_dependencies(${component} ${objLib})

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
