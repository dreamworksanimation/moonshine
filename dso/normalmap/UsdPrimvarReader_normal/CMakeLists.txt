# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(UsdPrimvarReader_normal
    DEPENDENCIES
        Moonray::map_primvar
        Moonray::rendering_shading
        Moonray::shading_ispc
        SceneRdl2::render_util)
