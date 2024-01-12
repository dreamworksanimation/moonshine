// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

// Skewing factors for 3D simplex grid:
// F3 = 1/3
static const float sF3 = 0.333333333;
// G3 = 1/6
static const float sG3 = 0.166666667;

// Skewing factors for 4D simplex grid:
// F4 = (Math.sqrt(5.0)-1.0)/4.0;
static const float sF4  = 0.309016994375;
// G4 = (5.0-Math.sqrt(5.0))/20.0;
static const float sG4  = 0.138196601125;

//  Constant value for orthogonal gradient vectors
//  GV = sqrt(2)/sqrt(3) = 0.816496580
static const float sGV = 0.81649658f;
static const float sNGV = -0.81649658f;

// Code based on public domain work by Stefan Gustavson
// https://github.com/stegu/perlin-noise/blob/master/LICENSE.md
//
// From Stefan Gustavson,
// "For 3D, we define two orthogonal vectors in the desired rotation plane.
// These vectors are based on the midpoints of the 12 edges of a cube,
// they all rotate in their own plane and are never coincident or collinear.
// A larger array of random vectors would also do the job, but these 12
// (including 4 repeats to make the array length a power of two) work better.
// They are not random, they are carefully chosen to represent a small
// isotropic set of directions for any rotation angle."
static const float sSimplexGradients3DU[16][3] = {
    {1.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 1.0f},
    {-1.0f, 0.0f, 1.0f},
    {0.0f, -1.0f, 1.0f},
    {1.0f, 0.0f, -1.0f},
    {0.0f, 1.0f, -1.0f},
    {-1.0f, 0.0f, -1.0f},
    {0.0f, -1.0f, -1.0f},
    {sGV, sGV, sGV},
    {sNGV, sGV, sNGV},
    {sNGV, sNGV, sGV},
    {sGV, sNGV, sNGV},
    {sNGV, sGV, sGV},
    {sGV, sNGV, sGV},
    {sGV, sNGV, sNGV},
    {sNGV, sGV, sNGV}
};

static const float sSimplexGradients3DV[16][3] = {
    {sNGV, sGV, sGV},
    {sNGV, sNGV, sGV},
    {sGV, sNGV, sGV},
    {sGV, sGV, sGV},
    {sNGV, sNGV, sNGV},
    {sGV, sNGV, sNGV},
    {sGV, sGV, sNGV},
    {sNGV, sGV, sNGV},
    {1.0f, -1.0f, 0.0f},
    {1.0f, 1.0f, 0.0f},
    {-1.0f, 1.0f, 0.0f},
    {-1.0f, -1.0f, 0.0f},
    {1.0f, 0.0f, 1.0f},
    {-1.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, -1.0f},
    {0.0f, -1.0f, -1.0f}
};

static const float sSimplexGradients4DU[32][4] = {
    {0.0f, 1.0f, 1.0f, 1.0f},
    {0.0f, 1.0f, 1.0f, -1.0f},
    {0.0f, 1.0f, -1.0f, 1.0f},
    {0.0f, 1.0f, -1.0f, -1.0f},
    {0.0f, -1.0f, 1.0f, 1.0f}, 
    {0.0f, -1.0f, 1.0f, -1.0f}, 
    {0.0f, -1.0f, -1.0f, 1.0f}, 
    {0.0f, -1.0f, -1.0f, -1.0f},
    {1.0f, 0.0f, 1.0f, 1.0f}, 
    {1.0f, 0.0f, 1.0f, -1.0f}, 
    {1.0f, 0.0f, -1.0f, 1.0f}, 
    {1.0f, 0.0f, -1.0f, -1.0f},
    {-1.0f, 0.0f, 1.0f, 1.0f}, 
    {-1.0f, 0.0f, 1.0f, -1.0f}, 
    {-1.0f, 0.0f, -1.0f, 1.0f}, 
    {-1.0f, 0.0f, -1.0f, -1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f}, 
    {1.0f, 1.0f, 0.0f, -1.0f}, 
    {1.0f, -1.0f, 0.0f, 1.0f}, 
    {1.0f, -1.0f, 0.0f, -1.0f},
    {-1.0f, 1.0f, 0.0f, 1.0f}, 
    {-1.0f, 1.0f, 0.0f, -1.0f}, 
    {-1.0f, -1.0f, 0.0f, 1.0f}, 
    {-1.0f, -1.0f, 0.0f, -1.0f},
    {1.0f, 1.0f, 1.0f, 0.0f}, 
    {1.0f, 1.0f, -1.0f, 0.0f}, 
    {1.0f, -1.0f, 1.0f, 0.0f}, 
    {1.0f, -1.0f, -1.0f, 0.0f},
    {-1.0f, 1.0f, 1.0f, 0.0f}, 
    {-1.0f, 1.0f, -1.0f, 0.0f}, 
    {-1.0f, -1.0f, 1.0f, 0.0f},
    {-1.0f, -1.0f, -1.0f, 0.0f}
};

static const float sSimplexGradients4DV[32][4] = {
    {sNGV, sNGV, sGV, 1.0f},
    {sNGV, sNGV, sGV, -1.0f},
    {sGV, sNGV, sNGV, 1.0},
    {sGV, sNGV, sNGV, -1.0f},
    {sGV, sGV, sGV, 1.0f},
    {sGV, sGV, sGV, -1.0f},
    {sNGV, sGV, sNGV, 1.0f},
    {sNGV, sGV, sNGV, -1.0f},
    {sNGV, sGV, sGV, 1.0f},
    {sNGV, sGV, sGV, -1.0f},
    {sNGV, sNGV, sNGV, 1.0f},
    {sNGV, sNGV, sNGV, -1.0f},
    {sGV, sNGV, sGV, 1.0f},
    {sGV, sNGV, sGV, -1.0f},
    {sGV, sGV, sNGV, 1.0f},
    {sGV, sGV, sNGV, -1.0f},
    {sNGV, sGV, sNGV, 1.0f},
    {sNGV, sGV, sNGV, -1.0f},
    {sGV, sGV, sGV, 1.0f},
    {sGV, sGV, sGV, -1.0f},
    {sNGV, sNGV, sGV, 1.0f},
    {sNGV, sNGV, sGV, -1.0f},
    {sGV, sNGV, sNGV, 1.0f},
    {sGV, sNGV, sNGV, -1.0f},
    {1.0f, 0.0f, -1.0f, 0.0f},
    {1.0f, 0.0f, 1.0f, 0.0f},
    {-1.0f, 0.0f, 1.0f, 0.0f},
    {1.0f, 0.0f, 1.0f, 0.0f},
    {-1.0f, 0.0f, -1.0f, 0.0f},
    {1.0f, 0.0f, -1.0f, 0.0f},
    {-1.0f, 0.0f, -1.0f, 0.0f},
    {-1.0f, 0.0f, 1.0f, 0.0f}
};


