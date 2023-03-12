// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

struct ConstantDisplayFilter
{
    Color mColor;
};

export const uniform ConstantDisplayFilter * uniform
ConstantDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(ConstantDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform ConstantDisplayFilter * uniform self = ConstantDisplayFilter_get(me);
    *result = self->mColor;
}

DEFINE_DISPLAY_FILTER(ConstantDisplayFilter, filter)

//---------------------------------------------------------------------------

