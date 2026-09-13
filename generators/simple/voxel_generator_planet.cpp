#include "voxel_generator_planet.h"
#include "../../util/containers/span.h"
#include "../../util/godot/classes/image.h"
#include "../../util/math/sdf_sphere_heightmap.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

#ifdef ZN_GODOT
#include "../../util/godot/core/class_db.h"
#endif

namespace zylann::voxel {

VoxelGeneratorPlanet::VoxelGeneratorPlanet() {
	_height_source = SOURCE_NOISE;
	Ref<ZN_FastNoiseLite> noise;
	noise.instantiate();
	set_height_noise(noise);
}

VoxelGeneratorPlanet::~VoxelGeneratorPlanet() {}

void VoxelGeneratorPlanet::set_height_source(HeightSource source) {
	if (_height_source == source) {
		return;
	}
	_height_source = source;
	RWLockWrite wlock(_parameters_lock);
	_parameters.height_source = source;
}

VoxelGeneratorPlanet::HeightSource VoxelGeneratorPlanet::get_height_source() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.height_source;
}

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

void VoxelGeneratorPlanet::set_height_noise(Ref<ZN_FastNoiseLite> noise) {
	if (_height_noise == noise) {
		return;
	}
	_height_noise = noise;
	Ref<ZN_FastNoiseLite> copy;
	if (noise.is_valid()) {
		// The noise resource is not thread-safe, so we keep a private copy for use in worker threads.
		copy = noise->duplicate();
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.height_noise = copy;
}

Ref<ZN_FastNoiseLite> VoxelGeneratorPlanet::get_height_noise() const {
	return _height_noise;
}

void VoxelGeneratorPlanet::set_height_noise_amplitude(float amplitude) {
	if (amplitude < 0.f) {
		amplitude = 0.f;
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.height_noise_amplitude = amplitude;
}

float VoxelGeneratorPlanet::get_height_noise_amplitude() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.height_noise_amplitude;
}

void VoxelGeneratorPlanet::set_image_data5(Ref<Image> im) {
	if (im == _image_data5) {
		return;
	}
	if (im.is_valid()) {
		ERR_FAIL_COND(im->is_compressed());
	}
	_image_data5 = im;
	Ref<Image> copy;
	float norm_x = 1.f;
	float norm_y = 1.f;
	if (im.is_valid()) {
		copy = im->duplicate();
		norm_x = float(im->get_width());
		norm_y = float(im->get_height());
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.image_data5 = copy;
	_parameters.norm_x_data5 = norm_x;
	_parameters.norm_y_data5 = norm_y;
}

Ref<Image> VoxelGeneratorPlanet::get_image_data5() const {
	return _image_data5;
}

void VoxelGeneratorPlanet::set_image_data6(Ref<Image> im) {
	if (im == _image_data6) {
		return;
	}
	if (im.is_valid()) {
		ERR_FAIL_COND(im->is_compressed());
	}
	_image_data6 = im;
	Ref<Image> copy;
	float norm_x = 1.f;
	float norm_y = 1.f;
	if (im.is_valid()) {
		copy = im->duplicate();
		norm_x = float(im->get_width());
		norm_y = float(im->get_height());
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.image_data6 = copy;
	_parameters.norm_x_data6 = norm_x;
	_parameters.norm_y_data6 = norm_y;
}

Ref<Image> VoxelGeneratorPlanet::get_image_data6() const {
	return _image_data6;
}

void VoxelGeneratorPlanet::set_image_data7(Ref<Image> im) {
	if (im == _image_data7) {
		return;
	}
	if (im.is_valid()) {
		ERR_FAIL_COND(im->is_compressed());
	}
	_image_data7 = im;
	Ref<Image> copy;
	float norm_x = 1.f;
	float norm_y = 1.f;
	if (im.is_valid()) {
		copy = im->duplicate();
		norm_x = float(im->get_width());
		norm_y = float(im->get_height());
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.image_data7 = copy;
	_parameters.norm_x_data7 = norm_x;
	_parameters.norm_y_data7 = norm_y;
}

Ref<Image> VoxelGeneratorPlanet::get_image_data7() const {
	return _image_data7;
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
		const Parameters &params
) const {
	if (!params.detail_noise_enabled || params.detail_noise.is_null() || params.image_data5.is_null()) {
		return sdf;
	}
	const float amplitude_base = params.detail_noise_amplitude;
	if (amplitude_base <= 0.f) {
		return sdf;
	}
	const ZN_FastNoiseLite &noise = **params.detail_noise;
	// The G channel (CHANNEL_DATA5) of image_data5 attenuates the noise amplitude:
	// 0.0 = flat (no noise), 1.0 = full configured amplitude.
	float u = 0.f;
	float v = 0.f;
	compute_planet_uv(pos_x, pos_y, pos_z, u, v);
	const float meta_g = sample_image_channel_linear(
			**params.image_data5,
			u * params.norm_x_data5,
			v * params.norm_y_data5,
			1 // G channel
	);
	const float attenuation = math::clamp(meta_g, 0.f, 1.f);
	const float amplitude = amplitude_base * attenuation;
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

	if (params.height_source == SOURCE_IMAGE && params.image.is_null()) {
		return result;
	}

	const Image *image = (params.height_source == SOURCE_IMAGE && params.image.is_valid()) ? &**params.image : nullptr;

	const Vector3i bs = out_buffer.get_size();
	const int stride = 1 << input.lod;

	const Image *im_d5 = params.image_data5.is_valid() ? &**params.image_data5 : nullptr;
	const Image *im_d6 = params.image_data6.is_valid() ? &**params.image_data6 : nullptr;
	const Image *im_d7 = params.image_data7.is_valid() ? &**params.image_data7 : nullptr;
	const bool has_any_data_image = (im_d5 != nullptr || im_d6 != nullptr || im_d7 != nullptr);
	const ZN_FastNoiseLite *height_noise =
			(params.height_source == SOURCE_NOISE && params.height_noise.is_valid()) ? &**params.height_noise : nullptr;
	const float height_noise_amp = params.height_noise_amplitude * params.factor;

	int gz = input.origin_in_voxels.z;
	for (int z = 0; z < bs.z; ++z, gz += stride) {
		int gx = input.origin_in_voxels.x;
		for (int x = 0; x < bs.x; ++x, gx += stride) {
			int gy = input.origin_in_voxels.y;
			for (int y = 0; y < bs.y; ++y, gy += stride) {
				float sdf;
				if (params.height_source == SOURCE_IMAGE) {
					sdf = sdf_sphere_heightmap(
							gx,
							gy,
							gz,
							params.radius,
							params.factor,
							*image,
							params.min_height,
							params.max_height,
							params.norm_x,
							params.norm_y
					);
				} else {
					const float d = Math::Fast_Sqrt(float(gx * gx + gy * gy + gz * gz)) + 0.0001f;
					sdf = d - params.radius;
					if (height_noise != nullptr && height_noise_amp != 0.f) {
						sdf -= height_noise_amp * height_noise->get_noise_3d(gx, gy, gz);
					}
				}
				sdf = _apply_noise(sdf, gx, gy, gz, params);
				out_buffer.set_voxel_f(sdf, x, y, z, VoxelBuffer::CHANNEL_SDF);

				if (has_any_data_image) {
					float u = 0.f;
					float v = 0.f;
					compute_planet_uv(gx, gy, gz, u, v);

					const float meta_g = im_d5 != nullptr
							? sample_image_channel_linear(*im_d5, u * params.norm_x_data5, v * params.norm_y_data5, 1)
							: 0.f;
					const float meta_b = im_d6 != nullptr
							? sample_image_channel_linear(*im_d6, u * params.norm_x_data6, v * params.norm_y_data6, 2)
							: 0.f;
					const float meta_a = im_d7 != nullptr
							? sample_image_channel_linear(*im_d7, u * params.norm_x_data7, v * params.norm_y_data7, 3)
							: 0.f;

					out_buffer.set_voxel_f(meta_g, x, y, z, VoxelBuffer::CHANNEL_DATA5);
					out_buffer.set_voxel_f(meta_b, x, y, z, VoxelBuffer::CHANNEL_DATA6);
					out_buffer.set_voxel_f(meta_a, x, y, z, VoxelBuffer::CHANNEL_DATA7);
				} else {
					out_buffer.set_voxel_f(0.f, x, y, z, VoxelBuffer::CHANNEL_DATA5);
					out_buffer.set_voxel_f(0.f, x, y, z, VoxelBuffer::CHANNEL_DATA6);
					out_buffer.set_voxel_f(0.f, x, y, z, VoxelBuffer::CHANNEL_DATA7);
				}
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

	if (channel == VoxelBuffer::CHANNEL_SDF) {
		float sdf;
		if (params.height_source == SOURCE_IMAGE) {
			if (params.image.is_null()) {
				return v;
			}
			const Image &image = **params.image;
			sdf = sdf_sphere_heightmap(
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
		} else {
			const float fx = float(pos.x);
			const float fy = float(pos.y);
			const float fz = float(pos.z);
			const float d = Math::Fast_Sqrt(fx * fx + fy * fy + fz * fz) + 0.0001f;
			sdf = d - params.radius;
			if (params.height_noise.is_valid()) {
				const float height_noise_amp = params.height_noise_amplitude * params.factor;
				if (height_noise_amp != 0.f) {
					sdf -= height_noise_amp * params.height_noise->get_noise_3d(fx, fy, fz);
				}
			}
		}
		v.f = _apply_noise(sdf, float(pos.x), float(pos.y), float(pos.z), params);
	} else if (channel == VoxelBuffer::CHANNEL_DATA5) {
		if (params.image_data5.is_valid()) {
			float u = 0.f;
			float v_coord = 0.f;
			compute_planet_uv(float(pos.x), float(pos.y), float(pos.z), u, v_coord);
			v.f = sample_image_channel_linear(
					**params.image_data5,
					u * params.norm_x_data5,
					v_coord * params.norm_y_data5,
					1
			);
		} else {
			v.f = 0.f;
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA6) {
		if (params.image_data6.is_valid()) {
			float u = 0.f;
			float v_coord = 0.f;
			compute_planet_uv(float(pos.x), float(pos.y), float(pos.z), u, v_coord);
			v.f = sample_image_channel_linear(
					**params.image_data6,
					u * params.norm_x_data6,
					v_coord * params.norm_y_data6,
					2
			);
		} else {
			v.f = 0.f;
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA7) {
		if (params.image_data7.is_valid()) {
			float u = 0.f;
			float v_coord = 0.f;
			compute_planet_uv(float(pos.x), float(pos.y), float(pos.z), u, v_coord);
			v.f = sample_image_channel_linear(
					**params.image_data7,
					u * params.norm_x_data7,
					v_coord * params.norm_y_data7,
					3
			);
		} else {
			v.f = 0.f;
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

	if (channel == VoxelBuffer::CHANNEL_SDF) {
		if (params.height_source == SOURCE_IMAGE) {
			if (params.image.is_null()) {
				for (size_t i = 0; i < count; ++i) {
					out_values[i] = constants::SDF_FAR_OUTSIDE;
				}
				return;
			}
			const Image &image = **params.image;
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
				out_values[i] = _apply_noise(sdf, positions_x[i], positions_y[i], positions_z[i], params);
			}
		} else {
			const ZN_FastNoiseLite *height_noise =
					params.height_noise.is_valid() ? &**params.height_noise : nullptr;
			const float height_noise_amp = params.height_noise_amplitude * params.factor;
			for (size_t i = 0; i < count; ++i) {
				const float px = positions_x[i];
				const float py = positions_y[i];
				const float pz = positions_z[i];
				const float d = Math::Fast_Sqrt(px * px + py * py + pz * pz) + 0.0001f;
				float sdf = d - params.radius;
				if (height_noise != nullptr && height_noise_amp != 0.f) {
					sdf -= height_noise_amp * height_noise->get_noise_3d(px, py, pz);
				}
				out_values[i] = _apply_noise(sdf, px, py, pz, params);
			}
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA5) {
		if (params.image_data5.is_valid()) {
			const Image &image = **params.image_data5;
			for (size_t i = 0; i < count; ++i) {
				float u = 0.f;
				float v = 0.f;
				compute_planet_uv(positions_x[i], positions_y[i], positions_z[i], u, v);
				out_values[i] = sample_image_channel_linear(image, u * params.norm_x_data5, v * params.norm_y_data5, 1);
			}
		} else {
			for (size_t i = 0; i < count; ++i) {
				out_values[i] = 0.f;
			}
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA6) {
		if (params.image_data6.is_valid()) {
			const Image &image = **params.image_data6;
			for (size_t i = 0; i < count; ++i) {
				float u = 0.f;
				float v = 0.f;
				compute_planet_uv(positions_x[i], positions_y[i], positions_z[i], u, v);
				out_values[i] = sample_image_channel_linear(image, u * params.norm_x_data6, v * params.norm_y_data6, 2);
			}
		} else {
			for (size_t i = 0; i < count; ++i) {
				out_values[i] = 0.f;
			}
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA7) {
		if (params.image_data7.is_valid()) {
			const Image &image = **params.image_data7;
			for (size_t i = 0; i < count; ++i) {
				float u = 0.f;
				float v = 0.f;
				compute_planet_uv(positions_x[i], positions_y[i], positions_z[i], u, v);
				out_values[i] = sample_image_channel_linear(image, u * params.norm_x_data7, v * params.norm_y_data7, 3);
			}
		} else {
			for (size_t i = 0; i < count; ++i) {
				out_values[i] = 0.f;
			}
		}
	} else {
		for (size_t i = 0; i < count; ++i) {
			out_values[i] = 0.f;
		}
	}
}

void VoxelGeneratorPlanet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_height_source", "source"), &VoxelGeneratorPlanet::set_height_source);
	ClassDB::bind_method(D_METHOD("get_height_source"), &VoxelGeneratorPlanet::get_height_source);

	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelGeneratorPlanet::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelGeneratorPlanet::get_image);

	ClassDB::bind_method(D_METHOD("set_height_noise", "noise"), &VoxelGeneratorPlanet::set_height_noise);
	ClassDB::bind_method(D_METHOD("get_height_noise"), &VoxelGeneratorPlanet::get_height_noise);
	ClassDB::bind_method(
			D_METHOD("set_height_noise_amplitude", "amplitude"), &VoxelGeneratorPlanet::set_height_noise_amplitude
	);
	ClassDB::bind_method(D_METHOD("get_height_noise_amplitude"), &VoxelGeneratorPlanet::get_height_noise_amplitude);

	ClassDB::bind_method(D_METHOD("set_image_data5", "image"), &VoxelGeneratorPlanet::set_image_data5);
	ClassDB::bind_method(D_METHOD("get_image_data5"), &VoxelGeneratorPlanet::get_image_data5);

	ClassDB::bind_method(D_METHOD("set_image_data6", "image"), &VoxelGeneratorPlanet::set_image_data6);
	ClassDB::bind_method(D_METHOD("get_image_data6"), &VoxelGeneratorPlanet::get_image_data6);

	ClassDB::bind_method(D_METHOD("set_image_data7", "image"), &VoxelGeneratorPlanet::set_image_data7);
	ClassDB::bind_method(D_METHOD("get_image_data7"), &VoxelGeneratorPlanet::get_image_data7);

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
			PropertyInfo(Variant::INT, "height_source", PROPERTY_HINT_ENUM, "Image,Noise"),
			"set_height_source",
			"get_height_source"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, Image::get_class_static()),
			"set_image",
			"get_image"
	);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT,
					"height_noise",
					PROPERTY_HINT_RESOURCE_TYPE,
					ZN_FastNoiseLite::get_class_static(),
					PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT
			),
			"set_height_noise",
			"get_height_noise"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "height_noise_amplitude"),
			"set_height_noise_amplitude",
			"get_height_noise_amplitude"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "image_data5", PROPERTY_HINT_RESOURCE_TYPE, Image::get_class_static()),
			"set_image_data5",
			"get_image_data5"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "image_data6", PROPERTY_HINT_RESOURCE_TYPE, Image::get_class_static()),
			"set_image_data6",
			"get_image_data6"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "image_data7", PROPERTY_HINT_RESOURCE_TYPE, Image::get_class_static()),
			"set_image_data7",
			"get_image_data7"
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

	BIND_ENUM_CONSTANT(SOURCE_IMAGE);
	BIND_ENUM_CONSTANT(SOURCE_NOISE);
}

} // namespace zylann::voxel
