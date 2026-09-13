#ifndef VOXEL_GENERATOR_PLANET_H
#define VOXEL_GENERATOR_PLANET_H

#include "../../constants/voxel_constants.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/containers/span.h"
#include "../../util/godot/macros.h"
#include "../../util/math/vector3f.h"
#include "../../util/math/vector3i.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_generator.h"

ZN_GODOT_FORWARD_DECLARE(class Image)

namespace zylann {
class ZN_FastNoiseLite;
}

namespace zylann::voxel {

// Generates a spherical planet SDF from a heightmap image or noise.
// This is a CPU-only, graph-free equivalent of the `SdfSphereHeightmap` graph node.
class VoxelGeneratorPlanet : public VoxelGenerator {
	GDCLASS(VoxelGeneratorPlanet, VoxelGenerator)

public:
	enum HeightSource {
		SOURCE_IMAGE = 0,
		SOURCE_NOISE = 1
	};

	VoxelGeneratorPlanet();
	~VoxelGeneratorPlanet();

	void set_height_source(HeightSource source);
	HeightSource get_height_source() const;

	void set_image(Ref<Image> im);
	Ref<Image> get_image() const;

	void set_height_noise(Ref<ZN_FastNoiseLite> noise);
	Ref<ZN_FastNoiseLite> get_height_noise() const;

	void set_height_noise_amplitude(float amplitude);
	float get_height_noise_amplitude() const;

	void set_image_data5(Ref<Image> im);
	Ref<Image> get_image_data5() const;

	void set_image_data6(Ref<Image> im);
	Ref<Image> get_image_data6() const;

	void set_image_data7(Ref<Image> im);
	Ref<Image> get_image_data7() const;

	void set_radius(float radius);
	float get_radius() const;

	void set_factor(float factor);
	float get_factor() const;

	void set_detail_noise(Ref<ZN_FastNoiseLite> noise);
	Ref<ZN_FastNoiseLite> get_detail_noise() const;

	void set_detail_noise_enabled(bool enabled);
	bool is_detail_noise_enabled() const;

	void set_detail_noise_amplitude(float amplitude);
	float get_detail_noise_amplitude() const;

	Result generate_block(VoxelGenerator::VoxelQueryData input) override;

	bool supports_single_generation() const override {
		return true;
	}

	bool supports_series_generation() const override {
		return true;
	}

	VoxelSingleValue generate_single(Vector3i pos, unsigned int channel) override;

	void generate_series(
			Span<const float> positions_x,
			Span<const float> positions_y,
			Span<const float> positions_z,
			unsigned int channel,
			Span<float> out_values,
			Vector3f min_pos,
			Vector3f max_pos
	) override;

	int get_used_channels_mask() const override;

private:
	static void _bind_methods();

private:
	// Proper reference used for external access.
	HeightSource _height_source = SOURCE_NOISE;
	Ref<Image> _image;
	Ref<ZN_FastNoiseLite> _height_noise;
	Ref<Image> _image_data5;
	Ref<Image> _image_data6;
	Ref<Image> _image_data7;
	// Proper reference used for external access.
	Ref<ZN_FastNoiseLite> _detail_noise;

	struct Parameters {
		HeightSource height_source = SOURCE_NOISE;

		// This is a read-only copy of the image.
		// It wastes memory for sure, but Godot does not offer any way to secure this better.
		Ref<Image> image;
		float radius = 10.f;
		float factor = 1.f;
		// Height range of the image, used to skip sampling when far from the surface.
		float min_height = 0.f;
		float max_height = 1.f;
		// Image dimensions, used to normalize UV coordinates.
		float norm_x = 1.f;
		float norm_y = 1.f;

		// Height noise (primary surface shape when height_source == SOURCE_NOISE).
		Ref<ZN_FastNoiseLite> height_noise;
		float height_noise_amplitude = 1.f;

		// Separate images for data channels (DATA5: green, DATA6: blue, DATA7: alpha).
		Ref<Image> image_data5;
		float norm_x_data5 = 1.f;
		float norm_y_data5 = 1.f;

		Ref<Image> image_data6;
		float norm_x_data6 = 1.f;
		float norm_y_data6 = 1.f;

		Ref<Image> image_data7;
		float norm_x_data7 = 1.f;
		float norm_y_data7 = 1.f;

		// Detail noise (fractal Perlin) blended into the surface for fine detail.
		Ref<ZN_FastNoiseLite> detail_noise;
		bool detail_noise_enabled = false;
		// Full amplitude of the detail noise, in SDF units, when the attenuation factor is 1.0.
		float detail_noise_amplitude = 0.5f;
	};

	Parameters _parameters;
	RWLock _parameters_lock;

	// Blends the detail noise into the SDF value, attenuated by the G channel (CHANNEL_DATA5)
	// of image_data5. Returns the SDF unchanged when the noise is disabled or has no amplitude.
	float _apply_noise(
			float sdf,
			float pos_x,
			float pos_y,
			float pos_z,
			const Parameters &params
	) const;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelGeneratorPlanet::HeightSource);

#endif // VOXEL_GENERATOR_PLANET_H
