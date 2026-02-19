#include <string.h>
#include "intro_animation.h"

static int anim_get_pixel(const uint8_t *fb, int width, int height, int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return 0;

    const int bytes_per_row = width / 8;
    const int byte_index = y * bytes_per_row + (x / 8);
    const uint8_t bit = (uint8_t)(1u << (7 - (x % 8)));
    return (fb[byte_index] & bit) ? 1 : 0;
}

static void anim_set_pixel(uint8_t *fb, int width, int height, int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    const int bytes_per_row = width / 8;
    const int byte_index = y * bytes_per_row + (x / 8);
    const uint8_t bit = (uint8_t)(1u << (7 - (x % 8)));
    fb[byte_index] |= bit;
}

void draw_intro_animation(uint8_t *fb, const uint8_t *splash, int width, int height, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const int fb_size = (width * height) / 8;
    memset(fb, 0, (size_t)fb_size);

    // Retro-style staggered tile reveal from left to right.
    const int tile_h = 8;
    const float reveal_head = t * (float)(width + 24);
    for (int y = 0; y < height; y++)
    {
        const int band = (y / tile_h) % 4;
        const float band_offset = (float)(band * 6);
        const float limit = reveal_head - band_offset;

        for (int x = 0; x < width; x++)
        {
            if ((float)x <= limit && anim_get_pixel(splash, width, height, x, y))
                anim_set_pixel(fb, width, height, x, y);
        }
    }

    // Small scanline shimmer during transition.
    const int pulse = ((int)(t * 48.0f)) % 2;
    if (pulse == 0)
    {
        for (int y = 1; y < height; y += 4)
        {
            for (int x = 0; x < width; x++)
                anim_set_pixel(fb, width, height, x, y);
        }
    }
}
