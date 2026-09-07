#include "voxel_generator_planet.h"
#include "../../util/containers/span.h"
#include "../../util/godot/classes/image.h"
#include "../../util/math/sdf_sphere_heightmap.h"

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

int VoxelGeneratorPlanet::get_used_channels_mask() const {
	return (1 << VoxelBuffer::CHANNEL_SDF);
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
				const float sdf = sdf_sphere_heightmap(
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
				out_buffer.set_voxel_f(sdf, x, y, z, VoxelBuffer::CHANNEL_SDF);
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
		v.f = sdf_sphere_heightmap(
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
			out_values[i] = sdf_sphere_heightmap(
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

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, Image::get_class_static()),
			"set_image",
			"get_image"
	);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "factor"), "set_factor", "get_factor");
}

} // namespace zylann::voxel
