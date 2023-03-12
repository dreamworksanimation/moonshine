# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(DirectionalMap
    DEPENDENCIES
        Moonray::common_mcrt_macros
        Moonray::rendering_shading
        Moonray::shading_ispc
        Moonshine::common_interpolation
        SceneRdl2::render_util)
