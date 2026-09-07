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

namespace zylann::voxel {

// Generates a spherical planet SDF from a heightmap image.
// This is a CPU-only, graph-free equivalent of the `SdfSphereHeightmap` graph node.
class VoxelGeneratorPlanet : public VoxelGenerator {
	GDCLASS(VoxelGeneratorPlanet, VoxelGenerator)

public:
	VoxelGeneratorPlanet();
	~VoxelGeneratorPlanet();

	void set_image(Ref<Image> im);
	Ref<Image> get_image() const;

	void set_radius(float radius);
	float get_radius() const;

	void set_factor(float factor);
	float get_factor() const;

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
	Ref<Image> _image;

	struct Parameters {
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
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_PLANET_H
