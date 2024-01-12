# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(NoiseMap_v2
    DEPENDENCIES
        Moonray::common_mcrt_macros
        Moonray::map_primvar
        Moonray::rendering_shading
        Moonray::shading_ispc
        Moonshine::common_interpolation
        Moonshine::common_noise
        SceneRdl2::common_math)
