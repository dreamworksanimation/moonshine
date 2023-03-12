# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(RampDisplayFilter
    DEPENDENCIES
        Moonray::rendering_displayfilter
        Moonray::rendering_shading
        Moonray::shading_ispc
        SceneRdl2::common_math
        SceneRdl2::scene_rdl2)
