#include "voxel_generator_planet.h"
#include "../../util/containers/span.h"
#include "../../util/godot/classes/image.h"
#include "../../util/math/sdf_sphere_heightmap.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

#ifdef ZN_GODOT
#include "../../util/godot/core/class_db.h"
#endif

namespace zylann::voxel {

VoxelGeneratorPlanet::VoxelGeneratorPlanet() {}

VoxelGeneratorPlanet::~VoxelGeneratorPlanet() {}

void VoxelGeneratorPlanet::set_image(Ref<Image> im) {
	if (im == _image) {
		return;
	}
	if (im.is_valid()) {
		ERR_FAIL_COND(im->is_compressed());
	}
	_image = im;
	Ref<Image> copy;
	float min_h = 0.f;
	float max_h = 1.f;
	float norm_x = 1.f;
	float norm_y = 1.f;
	if (im.is_valid()) {
		copy = im->duplicate();
		norm_x = float(im->get_width());
		norm_y = float(im->get_height());
		// Scan once to find the height range, used to skip sampling when far from the surface.
		min_h = 1e30f;
		max_h = -1e30f;
		const int w = im->get_width();
		const int h = im->get_height();
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				const float v = im->get_pixel(x, y).r;
				if (v < min_h) {
					min_h = v;
				}
				if (v > max_h) {
					max_h = v;
				}
			}
		}
		if (min_h > max_h) {
			min_h = 0.f;
			max_h = 1.f;
		}
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.image = copy;
	_parameters.min_height = min_h;
	_parameters.max_height = max_h;
	_parameters.norm_x = norm_x;
	_parameters.norm_y = norm_y;
}

Ref<Image> VoxelGeneratorPlanet::get_image() const {
	return _image;
}

void VoxelGeneratorPlanet::set_radius(float radius) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.radius = radius;
}

float VoxelGeneratorPlanet::get_radius() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.radius;
}

void VoxelGeneratorPlanet::set_factor(float factor) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.factor = factor;
}

float VoxelGeneratorPlanet::get_factor() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.factor;
}

void VoxelGeneratorPlanet::set_detail_noise(Ref<ZN_FastNoiseLite> noise) {
	if (_detail_noise == noise) {
		return;
	}
	_detail_noise = noise;
	Ref<ZN_FastNoiseLite> copy;
	if (noise.is_valid()) {
		// The noise resource is not thread-safe, so we keep a private copy for use in worker threads.
		copy = noise->duplicate();
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.detail_noise = copy;
}

Ref<ZN_FastNoiseLite> VoxelGeneratorPlanet::get_detail_noise() const {
	return _detail_noise;
}

void VoxelGeneratorPlanet::set_detail_noise_enabled(bool enabled) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.detail_noise_enabled = enabled;
}

bool VoxelGeneratorPlanet::is_detail_noise_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.detail_noise_enabled;
}

void VoxelGeneratorPlanet::set_detail_noise_amplitude(float amplitude) {
	if (amplitude < 0.f) {
		amplitude = 0.f;
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.detail_noise_amplitude = amplitude;
}

float VoxelGeneratorPlanet::get_detail_noise_amplitude() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.detail_noise_amplitude;
}

float VoxelGeneratorPlanet::_apply_noise(
		float sdf,
		float pos_x,
		float pos_y,
		float pos_z,
		const Parameters &params,
		const Image &image
) const {
	if (!params.detail_noise_enabled || params.detail_noise.is_null()) {
		return sdf;
	}
	const ZN_FastNoiseLite &noise = **params.detail_noise;
	// The G channel (CHANNEL_DATA5) of the heightmap attenuates the noise amplitude:
	// 0.0 = flat (no noise), 1.0 = full configured amplitude.
	float meta_g = 0.f;
	float meta_b = 0.f;
	float meta_a = 0.f;
	sample_planet_metadata(image, pos_x, pos_y, pos_z, params.norm_x, params.norm_y, meta_g, meta_b, meta_a);
	const float attenuation = math::clamp(meta_g, 0.f, 1.f);
	const float amplitude = params.detail_noise_amplitude * attenuation;
	if (amplitude <= 0.f) {
		return sdf;
	}
	// Sample the noise in world space. The noise resource's `period` already defines its frequency.
	const float n = noise.get_noise_3d(pos_x, pos_y, pos_z);
	return sdf - amplitude * n;
}

int VoxelGeneratorPlanet::get_used_channels_mask() const {
	return (1 << VoxelBuffer::CHANNEL_SDF) | (1 << VoxelBuffer::CHANNEL_DATA5) | (1 << VoxelBuffer::CHANNEL_DATA6) |
		   (1 << VoxelBuffer::CHANNEL_DATA7);
}

VoxelGenerator::Result VoxelGeneratorPlanet::generate_block(VoxelGenerator::VoxelQueryData input) {
	VoxelBuffer &out_buffer = input.voxel_buffer;

	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	Result result;

	ERR_FAIL_COND_V(params.image.is_null(), result);
	const Image &image = **params.image;

	const Vector3i bs = out_buffer.get_size();
	const int stride = 1 << input.lod;

	int gz = input.origin_in_voxels.z;
	for (int z = 0; z < bs.z; ++z, gz += stride) {
		int gx = input.origin_in_voxels.x;
		for (int x = 0; x < bs.x; ++x, gx += stride) {
			int gy = input.origin_in_voxels.y;
			for (int y = 0; y < bs.y; ++y, gy += stride) {
				float sdf = sdf_sphere_heightmap(
						gx,
						gy,
						gz,
						params.radius,
						params.factor,
						image,
						params.min_height,
						params.max_height,
						params.norm_x,
						params.norm_y
				);
				sdf = _apply_noise(sdf, gx, gy, gz, params, image);
				out_buffer.set_voxel_f(sdf, x, y, z, VoxelBuffer::CHANNEL_SDF);

				// Carry per-voxel metadata from the unused G, B and A channels of the heightmap.
				float meta_g = 0.f;
				float meta_b = 0.f;
				float meta_a = 0.f;
				sample_planet_metadata(
						image,
						gx,
						gy,
						gz,
						params.norm_x,
						params.norm_y,
						meta_g,
						meta_b,
						meta_a
				);
				out_buffer.set_voxel_f(meta_g, x, y, z, VoxelBuffer::CHANNEL_DATA5);
				out_buffer.set_voxel_f(meta_b, x, y, z, VoxelBuffer::CHANNEL_DATA6);
				out_buffer.set_voxel_f(meta_a, x, y, z, VoxelBuffer::CHANNEL_DATA7);
			}
		} // for x
	} // for z

	out_buffer.compress_uniform_channels();
	return result;
}

VoxelSingleValue VoxelGeneratorPlanet::generate_single(Vector3i pos, unsigned int channel) {
	VoxelSingleValue v;
	v.i = 0;
	ZN_ASSERT_RETURN_V(channel < VoxelBuffer::MAX_CHANNELS, v);

	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	if (params.image.is_null()) {
		return v;
	}
	const Image &image = **params.image;

	if (channel == VoxelBuffer::CHANNEL_SDF) {
		float sdf = sdf_sphere_heightmap(
				float(pos.x),
				float(pos.y),
				float(pos.z),
				params.radius,
				params.factor,
				image,
				params.min_height,
				params.max_height,
				params.norm_x,
				params.norm_y
		);
		v.f = _apply_noise(sdf, float(pos.x), float(pos.y), float(pos.z), params, image);
	} else if (channel == VoxelBuffer::CHANNEL_DATA5 || channel == VoxelBuffer::CHANNEL_DATA6 ||
			   channel == VoxelBuffer::CHANNEL_DATA7) {
		float meta_g = 0.f;
		float meta_b = 0.f;
		float meta_a = 0.f;
		sample_planet_metadata(
				image,
				float(pos.x),
				float(pos.y),
				float(pos.z),
				params.norm_x,
				params.norm_y,
				meta_g,
				meta_b,
				meta_a
		);
		if (channel == VoxelBuffer::CHANNEL_DATA5) {
			v.f = meta_g;
		} else if (channel == VoxelBuffer::CHANNEL_DATA6) {
			v.f = meta_b;
		} else {
			v.f = meta_a;
		}
	} else {
		v.i = 0;
	}
	return v;
}

void VoxelGeneratorPlanet::generate_series(
		Span<const float> positions_x,
		Span<const float> positions_y,
		Span<const float> positions_z,
		unsigned int channel,
		Span<float> out_values,
		Vector3f min_pos,
		Vector3f max_pos
) {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	const size_t count = out_values.size();
	if (params.image.is_null()) {
		for (size_t i = 0; i < count; ++i) {
			out_values[i] = constants::SDF_FAR_OUTSIDE;
		}
		return;
	}
	const Image &image = **params.image;

	if (channel == VoxelBuffer::CHANNEL_SDF) {
		for (size_t i = 0; i < count; ++i) {
			float sdf = sdf_sphere_heightmap(
					positions_x[i],
					positions_y[i],
					positions_z[i],
					params.radius,
					params.factor,
					image,
					params.min_height,
					params.max_height,
					params.norm_x,
					params.norm_y
			);
			out_values[i] = _apply_noise(sdf, positions_x[i], positions_y[i], positions_z[i], params, image);
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA5 || channel == VoxelBuffer::CHANNEL_DATA6 ||
			   channel == VoxelBuffer::CHANNEL_DATA7) {
		for (size_t i = 0; i < count; ++i) {
			float meta_g = 0.f;
			float meta_b = 0.f;
			float meta_a = 0.f;
			sample_planet_metadata(
					image,
					positions_x[i],
					positions_y[i],
					positions_z[i],
					params.norm_x,
					params.norm_y,
					meta_g,
					meta_b,
					meta_a
			);
			if (channel == VoxelBuffer::CHANNEL_DATA5) {
				out_values[i] = meta_g;
			} else if (channel == VoxelBuffer::CHANNEL_DATA6) {
				out_values[i] = meta_b;
			} else {
				out_values[i] = meta_a;
			}
		}
	} else {
		for (size_t i = 0; i < count; ++i) {
			out_values[i] = 0.f;
		}
	}
}

void VoxelGeneratorPlanet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelGeneratorPlanet::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelGeneratorPlanet::get_image);

	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &VoxelGeneratorPlanet::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &VoxelGeneratorPlanet::get_radius);

	ClassDB::bind_method(D_METHOD("set_factor", "factor"), &VoxelGeneratorPlanet::set_factor);
	ClassDB::bind_method(D_METHOD("get_factor"), &VoxelGeneratorPlanet::get_factor);

	ClassDB::bind_method(D_METHOD("set_detail_noise", "noise"), &VoxelGeneratorPlanet::set_detail_noise);
	ClassDB::bind_method(D_METHOD("get_detail_noise"), &VoxelGeneratorPlanet::get_detail_noise);
	ClassDB::bind_method(D_METHOD("set_detail_noise_enabled", "enabled"), &VoxelGeneratorPlanet::set_detail_noise_enabled);
	ClassDB::bind_method(D_METHOD("is_detail_noise_enabled"), &VoxelGeneratorPlanet::is_detail_noise_enabled);
	ClassDB::bind_method(
			D_METHOD("set_detail_noise_amplitude", "amplitude"), &VoxelGeneratorPlanet::set_detail_noise_amplitude
	);
	ClassDB::bind_method(D_METHOD("get_detail_noise_amplitude"), &VoxelGeneratorPlanet::get_detail_noise_amplitude);

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, Image::get_class_static()),
			"set_image",
			"get_image"
	);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "factor"), "set_factor", "get_factor");
	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT,
					"detail_noise",
					PROPERTY_HINT_RESOURCE_TYPE,
					ZN_FastNoiseLite::get_class_static(),
					PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT
			),
			"set_detail_noise",
			"get_detail_noise"
	);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "detail_noise_enabled"), "set_detail_noise_enabled", "is_detail_noise_enabled");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "detail_noise_amplitude"),
			"set_detail_noise_amplitude",
			"get_detail_noise_amplitude"
	);
}

} // namespace zylann::voxel
