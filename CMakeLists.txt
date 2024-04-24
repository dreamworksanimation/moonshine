# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required (VERSION 3.23.1)

include(OMR_PackageVersion)  # Sets versionString, projectString and PACKAGE_NAME
set(languages LANGUAGES CXX)
if(NOT CMAKE_XCODE_BUILD_SYSTEM)
    list(APPEND languages ISPC)
endif()
project(${projectString}
    VERSION ${versionString}
    ${languages})

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
list(APPEND CMAKE_MESSAGE_CONTEXT ${PROJECT_NAME})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(OMR_Platform)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

include(CheckLanguage)
check_language(ISPC)
if(CMAKE_ISPC_COMPILER)
    enable_language(ISPC)
else()
    message(STATUS "No ISPC support")
endif()

# ================================================
# Options
# ================================================
option(${PROJECT_NAME_UPPER}_BUILD_TESTING
    "Whether or not to build the unittests" YES)
option(${PROJECT_NAME_UPPER}_DWA_BUILD
    "Whether to enable DWA-specific features" NO)

# Ideally MOONRAY_DWA_BUILD should be set to YES externally (e.g. by rez-build), but this
# is a fallback to enable it if STUDIO=GLD...
if(GLD STREQUAL "$ENV{STUDIO}")
    set(${PROJECT_NAME_UPPER}_DWA_BUILD YES)
endif()

option(ABI_SET_VERSION "Enable the abi-version option" OFF)
if(ABI_SET_VERSION)
    set(ABI_VERSION "6" CACHE STRING "If ABI_SET_VERSION is on, which version to set")
endif()

# ================================================
# Find dependencies
# ================================================
find_package(TBB REQUIRED)
find_package(OpenImageIO REQUIRED)
if("${PROJECT_NAME}" STREQUAL "${CMAKE_PROJECT_NAME}")
    find_package(Moonray REQUIRED)
    find_package(SceneRdl2 REQUIRED)
endif()

# Set the RPATH for binaries in the install tree
set(CMAKE_INSTALL_RPATH ${GLOBAL_INSTALL_RPATH})
if(NOT IsDarwinPlatform)
    set(CMAKE_BUILD_RPATH ${COMPILER_LIBRARY_DIR})
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# ===============================================
# Set the install directory for rdl2 dsos
# ===============================================
set(RDL2DSO_INSTALL_DIR rdl2dso)
set(RDL2DSO_PROXY_INSTALL_DIR rdl2dso.proxy)

# if OPT_LEVEL is set use its value to override the CMAKE_BUILD_TYPE because the
# rez cmake plugin does not have an easy way to set the build_target.
set(opt_level $ENV{OPT_LEVEL})
if(opt_level STREQUAL opt-debug)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
elseif(opt_level STREQUAL debug)
    set(CMAKE_BUILD_TYPE Debug)
elseif(opt_level STREQUAL opt)
    set(CMAKE_BUILD_TYPE Release)
endif()

# default to Release if no build type specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Create include/${PACKAGE_NAME} link in the build directory for generated headers.
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/include)
file(CREATE_LINK ../lib ${PROJECT_BINARY_DIR}/include/${PACKAGE_NAME} SYMBOLIC)

include(${PROJECT_NAME}CompileDefinitions)
include(${PROJECT_NAME}CompileFeatures)
include(${PROJECT_NAME}CompileOptions)
include(${PROJECT_NAME}LinkOptions)
include(SConscriptStub)

# ================================================
# Add project files
# ================================================
add_subdirectory(lib)
add_subdirectory(dso)
# add_subdirectory(cmd)
if(GLD STREQUAL "$ENV{STUDIO}")
    set(SDKScript_template "##-*-python-*-\n\nImport('env')\n\nenv.GatherProxies()\n")
    file(WRITE ${CMAKE_BINARY_DIR}/SDKScript ${SDKScript_template})
    install(FILES ${CMAKE_BINARY_DIR}/SDKScript DESTINATION .)
endif()

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR ${PROJECT_NAME_UPPER}_BUILD_TESTING)
        AND BUILD_TESTING)
    find_package(CppUnit REQUIRED)
    add_subdirectory(tests)
endif()

# ================================================
# Install
# ================================================
include(GNUInstallDirs)
install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
        NAMESPACE ${PROJECT_NAME}::)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
)

write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
)

