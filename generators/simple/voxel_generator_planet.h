#ifndef VOXEL_GENERATOR_PLANET_H
#define VOXEL_GENERATOR_PLANET_H

#include "../../storage/voxel_buffer.h"
#include "../../util/containers/span.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/godot/core/typed_array.h"
#include "../../util/math/constants.h"
#include "../../util/math/vector3f.h"
#include "../../util/math/vector3i.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_generator.h"

namespace zylann {
class ZN_FastNoiseLite;
}

namespace zylann::voxel {

class VoxelPlanetAtmosphericGas : public Resource {
	GDCLASS(VoxelPlanetAtmosphericGas, Resource)

public:
	VoxelPlanetAtmosphericGas();

	void set_gas_name(const String &p_name);
	String get_gas_name() const;

	void set_name(const String &p_name) {
		set_gas_name(p_name);
	}
	String get_name() const {
		return get_gas_name();
	}

	void set_percentage(float p_percentage);
	float get_percentage() const;

private:
	static void _bind_methods();

private:
	String _name;
	float _percentage = 0.0f;
};

class VoxelPlanetMineral : public Resource {
	GDCLASS(VoxelPlanetMineral, Resource)

public:
	VoxelPlanetMineral();

	void set_mineral_name(const String &p_name);
	String get_mineral_name() const;

	void set_name(const String &p_name) {
		set_mineral_name(p_name);
	}
	String get_name() const {
		return get_mineral_name();
	}

	void set_percentage(float p_percentage);
	float get_percentage() const;

private:
	static void _bind_methods();

private:
	String _name;
	float _percentage = 0.0f;
};

// Generates a spherical planet SDF from noise, along with procedural data channels
// (slope mask, water table proximity, surface temperature).
class VoxelGeneratorPlanet : public VoxelGenerator {
	GDCLASS(VoxelGeneratorPlanet, VoxelGenerator)

public:
	VoxelGeneratorPlanet();
	~VoxelGeneratorPlanet();

	void set_height_noise(Ref<ZN_FastNoiseLite> noise);
	Ref<ZN_FastNoiseLite> get_height_noise() const;

	void set_height_noise_amplitude(float amplitude);
	float get_height_noise_amplitude() const;

	void set_detail_noise_min_slope_degrees(float degrees);
	float get_detail_noise_min_slope_degrees() const;

	void set_detail_noise_max_slope_degrees(float degrees);
	float get_detail_noise_max_slope_degrees() const;

	void set_detail_noise_min_slope_falloff_degrees(float degrees);
	float get_detail_noise_min_slope_falloff_degrees() const;

	void set_detail_noise_max_slope_falloff_degrees(float degrees);
	float get_detail_noise_max_slope_falloff_degrees() const;

	void set_moisture_enabled(bool enabled);
	bool is_moisture_enabled() const;

	void set_moisture_max_distance(float distance);
	float get_moisture_max_distance() const;

	void set_temperature_blend_by_latitude_altitude_enabled(bool enabled);
	bool is_temperature_blend_by_latitude_altitude_enabled() const;

	void set_radius(float radius);
	float get_radius() const;

	void set_detail_noise(Ref<ZN_FastNoiseLite> noise);
	Ref<ZN_FastNoiseLite> get_detail_noise() const;

	void set_detail_noise_enabled(bool enabled);
	bool is_detail_noise_enabled() const;

	void set_detail_noise_amplitude(float amplitude);
	float get_detail_noise_amplitude() const;

	void set_rotation_speed(float speed);
	float get_rotation_speed() const;

	void set_rotation_axis(Vector3 axis);
	Vector3 get_rotation_axis() const;

	void set_atmospheric_density(float density);
	float get_atmospheric_density() const;

	void set_atmosphere_thickness(int thickness);
	int get_atmosphere_thickness() const;

	void set_atmospheric_composition(TypedArray<VoxelPlanetAtmosphericGas> composition);
	TypedArray<VoxelPlanetAtmosphericGas> get_atmospheric_composition() const;

	void set_moisture_water_table_radius(int radius);
	int get_moisture_water_table_radius() const;

	void set_mineral_composition(TypedArray<VoxelPlanetMineral> composition);
	TypedArray<VoxelPlanetMineral> get_mineral_composition() const;

	void set_min_surface_temperature(float temp);
	float get_min_surface_temperature() const;
	void set_minimum_surface_temperature(float temp) {
		set_min_surface_temperature(temp);
	}
	float get_minimum_surface_temperature() const {
		return get_min_surface_temperature();
	}

	void set_max_surface_temperature(float temp);
	float get_max_surface_temperature() const;
	void set_maximum_surface_temperature(float temp) {
		set_max_surface_temperature(temp);
	}
	float get_maximum_surface_temperature() const {
		return get_max_surface_temperature();
	}

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

	void _on_height_noise_changed();
	void _on_detail_noise_changed();

private:
	// Proper references used for external access.
	Ref<ZN_FastNoiseLite> _height_noise;
	Ref<ZN_FastNoiseLite> _detail_noise;

	struct Parameters {
		float radius = 10.f;

		// Primary surface shape.
		Ref<ZN_FastNoiseLite> height_noise;
		float height_noise_amplitude = 1.f;

		// Detail noise is attenuated by a slope mask stored in CHANNEL_DATA5.
		bool detail_noise_enabled = false;
		float detail_noise_min_slope_degrees = 0.f;
		float detail_noise_max_slope_degrees = 180.f;
		// Widths of the linear blend-out bands, located outside the [min, max] range.
		float detail_noise_min_slope_falloff_degrees = 0.f;
		float detail_noise_max_slope_falloff_degrees = 0.f;

		// Precomputed slope band edges. Cosines decrease as the angle increases, which lets us classify a
		// slope without calling `acos` outside of the falloff bands.
		float slope_lo_out_rad = 0.f;
		float slope_lo_in_rad = 0.f;
		float slope_hi_in_rad = math::PI<float>;
		float slope_hi_out_rad = math::PI<float>;
		float slope_lo_out_cos = 1.f;
		float slope_lo_in_cos = 1.f;
		float slope_hi_in_cos = -1.f;
		float slope_hi_out_cos = -1.f;

		// When enabled, moisture proximity is derived from the distance to the water table.
		bool moisture_enabled = false;
		float moisture_max_distance = 1.f;

		// When enabled, surface temperature is blended by latitude and altitude.
		bool temperature_blend_by_latitude_altitude_enabled = false;

		// Detail noise (fractal Perlin) blended into the surface for fine detail.
		Ref<ZN_FastNoiseLite> detail_noise;
		// Full amplitude of the detail noise, in SDF units, when the attenuation factor is 1.0.
		float detail_noise_amplitude = 0.5f;

		float rotation_speed = 0.0f;
		Vector3 rotation_axis = Vector3(0.0f, 1.0f, 0.0f);
		float atmospheric_density = 0.0f;
		int atmosphere_thickness = 0;
		int moisture_water_table_radius = 0;
		float temperature_min_surface_temperature = 273.15f;
		float temperature_max_surface_temperature = 273.15f;

		struct GasItem {
			String name;
			float percentage = 0.0f;
		};
		StdVector<GasItem> atmospheric_composition;

		struct MineralItem {
			String name;
			float percentage = 0.0f;
		};
		StdVector<MineralItem> mineral_composition;
	};

	Parameters _parameters;
	RWLock _parameters_lock;

	StdVector<Ref<VoxelPlanetAtmosphericGas>> _atmospheric_composition;
	StdVector<Ref<VoxelPlanetMineral>> _mineral_composition;

	// Blends the detail noise into the SDF value, attenuated by the slope mask.
	// Returns the SDF unchanged when the noise is disabled or has no amplitude.
	float _apply_noise(
			float sdf,
			float pos_x,
			float pos_y,
			float pos_z,
			float slope_weight,
			const Parameters &params
	) const;

	// Surface SDF without the detail noise.
	float _base_sdf(float pos_x, float pos_y, float pos_z, const Parameters &params) const;

	// Angle between the surface normal and the radial "up" direction, mapped to 0..1 by the slope range.
	// `epsilon` is the distance used to sample the SDF gradient, it should match the voxel stride.
	float _compute_slope_weight(float pos_x, float pos_y, float pos_z, float epsilon, const Parameters &params) const;

	// Slope weight used to attenuate detail noise, or 0 when disabled.
	float _compute_detail_noise_slope_mask(
			float pos_x,
			float pos_y,
			float pos_z,
			float base_sdf,
			float stride,
			const Parameters &params
	) const;

	// Water table proximity used for the moisture channel, or 0 when disabled.
	float _compute_moisture(float pos_x, float pos_y, float pos_z, const Parameters &params) const;

	// Normalized surface temperature, or 0 when disabled.
	float _compute_temperature(float pos_x, float pos_y, float pos_z, const Parameters &params) const;

	// Refreshes the precomputed slope band edges. The parameters lock must be held for writing.
	static void _update_slope_thresholds(Parameters &params);
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_PLANET_H
