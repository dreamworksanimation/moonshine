# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(HairColorCorrectMaterial
    DEPENDENCIES
        Moonray::common_mcrt_macros
        Moonray::rendering_shading
        Moonray::shading_ispc
        Moonshine::material_dwabase)
