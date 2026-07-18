#pragma once

#include <cstdint>

namespace noise {

// Seed the noise generator. Call once at game start.
void seed(uint32_t s);

// 1D noise: returns value in range [-1, 1]
float noise1d(float x);

// 2D noise: returns value in range [-1, 1]
float noise2d(float x, float y);

// 2D fractal noise (octaves): returns value in range [-1, 1]
float fractal2d(float x, float y, int octaves = 4, float persistence = 0.5f);

// Helper: map noise value [-1, 1] to [0, 1]
float normalize(float v);

// Helper: map noise value to integer range [min, max]
int map_to_int(float v, int min_val, int max_val);

}
