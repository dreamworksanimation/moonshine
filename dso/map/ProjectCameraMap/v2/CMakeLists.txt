# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(ProjectCameraMap_v2
    DEPENDENCIES
        Moonray::common_mcrt_macros
        Moonray::map_primvar
        Moonray::rendering_shading
        Moonray::shading_ispc
        Moonshine::map_projection)
