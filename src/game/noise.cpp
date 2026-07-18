#include "game/noise.h"
#include <cmath>
#include <algorithm>

namespace noise {

static uint32_t seed_val = 0;

// Permutation table (derived from seed)
static int perm[512];

static void build_permutation() {
    int p[256];
    for (int i = 0; i < 256; ++i) p[i] = i;

    // Fisher-Yates shuffle using seed
    uint32_t s = seed_val;
    for (int i = 255; i > 0; --i) {
        s = s * 1664525u + 1013904223u;
        int j = static_cast<int>(s >> 16) % (i + 1);
        std::swap(p[i], p[j]);
    }

    for (int i = 0; i < 512; ++i) perm[i] = p[i & 255];
}

void seed(uint32_t s) {
    seed_val = s;
    build_permutation();
}

// Gradients for 2D simplex noise
static const int grad2[12][2] = {
    {1,1},{-1,1},{1,-1},{-1,-1},
    {1,0},{-1,0},{0,1},{0,-1},
    {1,1},{-1,1},{1,-1},{-1,-1}
};

static float dot2(const int g[2], float x, float y) {
    return g[0] * x + g[1] * y;
}

static float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

float noise1d(float x) {
    int ix = static_cast<int>(std::floor(x)) & 255;
    float xf = x - std::floor(x);
    float u = fade(xf);

    int a = perm[ix];
    int b = perm[ix + 1];

    return lerp(u, static_cast<float>(a) / 255.0f * 2.0f - 1.0f,
                   static_cast<float>(b) / 255.0f * 2.0f - 1.0f);
}

float noise2d(float x, float y) {
    // Simplex-style 2D noise using permutation table
    int ix = static_cast<int>(std::floor(x)) & 255;
    int iy = static_cast<int>(std::floor(y)) & 255;

    float xf = x - std::floor(x);
    float yf = y - std::floor(y);

    float u = fade(xf);
    float v = fade(yf);

    int aa = perm[perm[ix] + iy];
    int ab = perm[perm[ix] + iy + 1];
    int ba = perm[perm[ix + 1] + iy];
    int bb = perm[perm[ix + 1] + iy + 1];

    float x1 = lerp(u,
        dot2(grad2[aa % 12], xf, yf),
        dot2(grad2[ba % 12], xf - 1, yf));
    float x2 = lerp(u,
        dot2(grad2[ab % 12], xf, yf - 1),
        dot2(grad2[bb % 12], xf - 1, yf - 1));

    return lerp(v, x1, x2);
}

float fractal2d(float x, float y, int octaves, float persistence) {
    float total = 0;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float max_val = 0;

    for (int i = 0; i < octaves; ++i) {
        total += noise2d(x * frequency, y * frequency) * amplitude;
        max_val += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / max_val;
}

float normalize(float v) {
    return (v + 1.0f) / 2.0f;
}

int map_to_int(float v, int min_val, int max_val) {
    float n = normalize(v);
    return min_val + static_cast<int>(n * (max_val - min_val + 1));
}

}
