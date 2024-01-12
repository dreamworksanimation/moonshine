# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

moonray_ispc_dso(ConstantColorMap
    DEPENDENCIES
        Moonray::rendering_shading
        Moonray::shading_ispc)
