# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(ProjectTriplanarNormalMap_v2
    DEPENDENCIES
        Moonray::common_mcrt_macros
        Moonray::map_primvar
        Moonray::rendering_shading
        Moonray::shading_ispc
        Moonshine::map_projection
        SceneRdl2::common_math
        SceneRdl2::render_util
        SceneRdl2::scene_rdl2)
