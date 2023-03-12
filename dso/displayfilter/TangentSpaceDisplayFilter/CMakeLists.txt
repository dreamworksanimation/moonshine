# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(TangentSpaceDisplayFilter
    DEPENDENCIES
        Moonray::rendering_displayfilter
        Moonray::shading_ispc
        SceneRdl2::common_math
        SceneRdl2::scene_rdl2)
