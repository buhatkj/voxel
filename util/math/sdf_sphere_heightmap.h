#ifndef VOXEL_SDF_SPHERE_HEIGHTMAP_H
#define VOXEL_SDF_SPHERE_HEIGHTMAP_H

#include "../godot/classes/image.h"
#include "constants.h"
#include "funcs.h"

namespace zylann::voxel {

inline float get_pixel_repeat(const Image &im, int x, int y, int w, int h) {
	return im.get_pixel(math::wrap(x, w), math::wrap(y, h)).r;
}

inline float get_pixel_repeat_linear(const Image &im, float x, float y, int im_w, int im_h) {
	const int x0 = int(Math::floor(x));
	const int y0 = int(Math::floor(y));

	const float xf = x - x0;
	const float yf = y - y0;

	const float h00 = get_pixel_repeat(im, x0, y0, im_w, im_h);
	const float h10 = get_pixel_repeat(im, x0 + 1, y0, im_w, im_h);
	const float h01 = get_pixel_repeat(im, x0, y0 + 1, im_w, im_h);
	const float h11 = get_pixel_repeat(im, x0 + 1, y0 + 1, im_w, im_h);

	// Bilinear filter
	const float h = Math::lerp(Math::lerp(h00, h10, xf), Math::lerp(h01, h11, xf), yf);

	return h;
}

inline float skew3(float x) {
	return (x * x * x + x) * 0.5f;
}

// This is mostly useful for generating planets from an existing heightmap
inline float sdf_sphere_heightmap(
		float x,
		float y,
		float z,
		float r,
		float m,
		const Image &im,
		float min_h,
		float max_h,
		float norm_x,
		float norm_y
) {
	// I think I can use the classic quake fast sqrt approximation here
	// this gets called a lot, and I wanna call it like EVEN MORE....
	const float d = Math::Fast_Sqrt(x * x + y * y + z * z) + 0.0001f;
	const float sd = d - r;
	// Optimize when far enough from heightmap.
	// This introduces a discontinuity but it should be ok for clamped storage
	const float margin = 1.2f * (max_h - min_h);
	if (sd > max_h + margin || sd < min_h - margin) {
		return sd;
	}
	const float nx = x / d;
	const float ny = y / d;
	const float nz = z / d;
	// TODO Could use fast atan2, it doesn't have to be precise
	// https://github.com/ducha-aiki/fast_atan2/blob/master/fast_atan.cpp
	const float uvx = -Math::atan2(nz, nx) * zylann::math::INV_TAU<float> + 0.5f;
	// This is an approximation of asin(ny)/(PI/2)
	// TODO It may be desirable to use the real function though,
	// in cases where we want to combine the same map in shaders
	const float ys = skew3(ny);
	const float uvy = -0.5f * ys + 0.5f;
	// TODO Could use bicubic interpolation when the image is sampled at lower resolution than voxels
	const float h = get_pixel_repeat_linear(im, uvx * norm_x, uvy * norm_y, im.get_width(), im.get_height());
	return sd - m * h;
}

// Samples the G, B and A channels of the heightmap image at the given position,
// using the same spherical UV mapping as `sdf_sphere_heightmap`.
// This is intended to carry per-voxel metadata (3 floats) derived from the
// otherwise-unused color channels of the source image.
inline void sample_planet_metadata(
		const Image &im,
		float x,
		float y,
		float z,
		float norm_x,
		float norm_y,
		float &out_g,
		float &out_b,
		float &out_a
) {
	const float d = Math::Fast_Sqrt(x * x + y * y + z * z) + 0.0001f;
	const float nx = x / d;
	const float ny = y / d;
	const float nz = z / d;
	const float uvx = (-Math::atan2(nz, nx) * zylann::math::INV_TAU<float> + 0.5f) * norm_x;
	const float ys = skew3(ny);
	const float uvy = (-0.5f * ys + 0.5f) * norm_y;

	const int im_w = im.get_width();
	const int im_h = im.get_height();
	const int x0 = int(Math::floor(uvx));
	const int y0 = int(Math::floor(uvy));
	const float xf = uvx - x0;
	const float yf = uvy - y0;

	// Read each corner pixel once and pull out the G, B and A components.
	const Color c00 = im.get_pixel(math::wrap(x0, im_w), math::wrap(y0, im_h));
	const Color c10 = im.get_pixel(math::wrap(x0 + 1, im_w), math::wrap(y0, im_h));
	const Color c01 = im.get_pixel(math::wrap(x0, im_w), math::wrap(y0 + 1, im_h));
	const Color c11 = im.get_pixel(math::wrap(x0 + 1, im_w), math::wrap(y0 + 1, im_h));

	out_g = Math::lerp(Math::lerp(c00.g, c10.g, xf), Math::lerp(c01.g, c11.g, xf), yf);
	out_b = Math::lerp(Math::lerp(c00.b, c10.b, xf), Math::lerp(c01.b, c11.b, xf), yf);
	out_a = Math::lerp(Math::lerp(c00.a, c10.a, xf), Math::lerp(c01.a, c11.a, xf), yf);
}

} // namespace zylann::voxel

#endif // VOXEL_SDF_SPHERE_HEIGHTMAP_H
