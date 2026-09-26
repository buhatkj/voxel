#include "voxel_generator_planet.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/containers/span.h"
#include "../../util/godot/core/typed_array.h"
#include "../../util/math/funcs.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

#ifdef ZN_GODOT
#include "../../util/godot/core/callable_mp.h"
#include "../../util/godot/core/class_db.h"
#endif

namespace zylann::voxel {

// Surface temperatures are authored in Kelvin but stored in CHANNEL_DATA7 normalized against this range.
static constexpr float MAX_TEMPERATURE_KELVIN = 6000.f;

VoxelPlanetAtmosphericGas::VoxelPlanetAtmosphericGas() {}

void VoxelPlanetAtmosphericGas::set_gas_name(const String &p_name) {
	if (_name == p_name) {
		return;
	}
	_name = p_name;
	emit_changed();
}

String VoxelPlanetAtmosphericGas::get_gas_name() const {
	return _name;
}

void VoxelPlanetAtmosphericGas::set_percentage(float p_percentage) {
	if (_percentage == p_percentage) {
		return;
	}
	_percentage = p_percentage;
	emit_changed();
}

float VoxelPlanetAtmosphericGas::get_percentage() const {
	return _percentage;
}

void VoxelPlanetAtmosphericGas::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_gas_name", "name"), &VoxelPlanetAtmosphericGas::set_gas_name);
	ClassDB::bind_method(D_METHOD("get_gas_name"), &VoxelPlanetAtmosphericGas::get_gas_name);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &VoxelPlanetAtmosphericGas::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &VoxelPlanetAtmosphericGas::get_name);
	ClassDB::bind_method(D_METHOD("set_percentage", "percentage"), &VoxelPlanetAtmosphericGas::set_percentage);
	ClassDB::bind_method(D_METHOD("get_percentage"), &VoxelPlanetAtmosphericGas::get_percentage);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "gas_name"), "set_gas_name", "get_gas_name");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "percentage", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"),
			"set_percentage",
			"get_percentage"
	);
}

VoxelPlanetMineral::VoxelPlanetMineral() {}

void VoxelPlanetMineral::set_mineral_name(const String &p_name) {
	if (_name == p_name) {
		return;
	}
	_name = p_name;
	emit_changed();
}

String VoxelPlanetMineral::get_mineral_name() const {
	return _name;
}

void VoxelPlanetMineral::set_percentage(float p_percentage) {
	if (_percentage == p_percentage) {
		return;
	}
	_percentage = p_percentage;
	emit_changed();
}

float VoxelPlanetMineral::get_percentage() const {
	return _percentage;
}

void VoxelPlanetMineral::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mineral_name", "name"), &VoxelPlanetMineral::set_mineral_name);
	ClassDB::bind_method(D_METHOD("get_mineral_name"), &VoxelPlanetMineral::get_mineral_name);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &VoxelPlanetMineral::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &VoxelPlanetMineral::get_name);
	ClassDB::bind_method(D_METHOD("set_percentage", "percentage"), &VoxelPlanetMineral::set_percentage);
	ClassDB::bind_method(D_METHOD("get_percentage"), &VoxelPlanetMineral::get_percentage);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mineral_name"), "set_mineral_name", "get_mineral_name");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "percentage", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"),
			"set_percentage",
			"get_percentage"
	);
}

VoxelGeneratorPlanet::VoxelGeneratorPlanet() {
	_update_slope_thresholds(_parameters);
	Ref<ZN_FastNoiseLite> noise;
	noise.instantiate();
	set_height_noise(noise);
}

VoxelGeneratorPlanet::~VoxelGeneratorPlanet() {
	if (_height_noise.is_valid()) {
		_height_noise->disconnect(
				VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelGeneratorPlanet::_on_height_noise_changed)
		);
	}
	if (_detail_noise.is_valid()) {
		_detail_noise->disconnect(
				VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelGeneratorPlanet::_on_detail_noise_changed)
		);
	}
	
}

void VoxelGeneratorPlanet::set_height_noise(Ref<ZN_FastNoiseLite> noise) {
	if (_height_noise == noise) {
		return;
	}
	if (_height_noise.is_valid()) {
		_height_noise->disconnect(
				VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelGeneratorPlanet::_on_height_noise_changed)
		);
	}
	_height_noise = noise;
	Ref<ZN_FastNoiseLite> copy;
	if (noise.is_valid()) {
		_height_noise->connect(
				VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelGeneratorPlanet::_on_height_noise_changed)
		);
		// The noise resource is not thread-safe, so we keep a private copy for use in worker threads.
		copy = noise->duplicate();
	}
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.height_noise = copy;
	}
	emit_changed();
}

void VoxelGeneratorPlanet::_on_height_noise_changed() {
	ERR_FAIL_COND(_height_noise.is_null());
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.height_noise = _height_noise->duplicate();
	}
	emit_changed();
}

Ref<ZN_FastNoiseLite> VoxelGeneratorPlanet::get_height_noise() const {
	return _height_noise;
}

void VoxelGeneratorPlanet::set_height_noise_amplitude(float amplitude) {
	if (amplitude < 0.f) {
		amplitude = 0.f;
	}
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.height_noise_amplitude = amplitude;
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_height_noise_amplitude() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.height_noise_amplitude;
}

void VoxelGeneratorPlanet::set_detail_noise_min_slope_degrees(float degrees) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.detail_noise_min_slope_degrees = math::clamp(degrees, 0.f, 180.f);
		_update_slope_thresholds(_parameters);
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_detail_noise_min_slope_degrees() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.detail_noise_min_slope_degrees;
}

void VoxelGeneratorPlanet::set_detail_noise_max_slope_degrees(float degrees) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.detail_noise_max_slope_degrees = math::clamp(degrees, 0.f, 180.f);
		_update_slope_thresholds(_parameters);
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_detail_noise_max_slope_degrees() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.detail_noise_max_slope_degrees;
}

void VoxelGeneratorPlanet::set_detail_noise_min_slope_falloff_degrees(float degrees) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.detail_noise_min_slope_falloff_degrees = math::clamp(degrees, 0.f, 180.f);
		_update_slope_thresholds(_parameters);
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_detail_noise_min_slope_falloff_degrees() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.detail_noise_min_slope_falloff_degrees;
}

void VoxelGeneratorPlanet::set_detail_noise_max_slope_falloff_degrees(float degrees) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.detail_noise_max_slope_falloff_degrees = math::clamp(degrees, 0.f, 180.f);
		_update_slope_thresholds(_parameters);
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_detail_noise_max_slope_falloff_degrees() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.detail_noise_max_slope_falloff_degrees;
}

void VoxelGeneratorPlanet::set_data6_water_table_enabled(bool enabled) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.data6_water_table_enabled = enabled;
	}
	emit_changed();
}

bool VoxelGeneratorPlanet::is_data6_water_table_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.data6_water_table_enabled;
}

void VoxelGeneratorPlanet::set_data6_water_table_max_distance(float distance) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.data6_water_table_max_distance = math::max(distance, 0.f);
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_data6_water_table_max_distance() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.data6_water_table_max_distance;
}

void VoxelGeneratorPlanet::set_data7_latitude_enabled(bool enabled) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.data7_latitude_enabled = enabled;
	}
	emit_changed();
}

bool VoxelGeneratorPlanet::is_data7_latitude_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.data7_latitude_enabled;
}

void VoxelGeneratorPlanet::set_radius(float radius) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.radius = radius;
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_radius() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.radius;
}

void VoxelGeneratorPlanet::set_detail_noise(Ref<ZN_FastNoiseLite> noise) {
	if (_detail_noise == noise) {
		return;
	}
	if (_detail_noise.is_valid()) {
		_detail_noise->disconnect(
				VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelGeneratorPlanet::_on_detail_noise_changed)
		);
	}
	_detail_noise = noise;
	Ref<ZN_FastNoiseLite> copy;
	if (noise.is_valid()) {
		_detail_noise->connect(
				VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelGeneratorPlanet::_on_detail_noise_changed)
		);
		// The noise resource is not thread-safe, so we keep a private copy for use in worker threads.
		copy = noise->duplicate();
	}
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.detail_noise = copy;
	}
	emit_changed();
}

void VoxelGeneratorPlanet::_on_detail_noise_changed() {
	ERR_FAIL_COND(_detail_noise.is_null());
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.detail_noise = _detail_noise->duplicate();
	}
	emit_changed();
}

Ref<ZN_FastNoiseLite> VoxelGeneratorPlanet::get_detail_noise() const {
	return _detail_noise;
}

void VoxelGeneratorPlanet::set_detail_noise_enabled(bool enabled) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.detail_noise_enabled = enabled;
	}
	emit_changed();
}

bool VoxelGeneratorPlanet::is_detail_noise_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.detail_noise_enabled;
}

void VoxelGeneratorPlanet::set_detail_noise_amplitude(float amplitude) {
	if (amplitude < 0.f) {
		amplitude = 0.f;
	}
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.detail_noise_amplitude = amplitude;
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_detail_noise_amplitude() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.detail_noise_amplitude;
}

void VoxelGeneratorPlanet::set_rotation_speed(float speed) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.rotation_speed = speed;
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_rotation_speed() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.rotation_speed;
}

void VoxelGeneratorPlanet::set_rotation_axis(Vector3 axis) {
	if (axis.is_zero_approx()) {
		axis = Vector3(0.0f, 1.0f, 0.0f);
	} else {
		axis.normalize();
	}
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.rotation_axis = axis;
	}
	emit_changed();
}

Vector3 VoxelGeneratorPlanet::get_rotation_axis() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.rotation_axis;
}

void VoxelGeneratorPlanet::set_atmospheric_density(float density) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.atmospheric_density = density;
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_atmospheric_density() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.atmospheric_density;
}

void VoxelGeneratorPlanet::set_atmosphere_thickness(int thickness) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.atmosphere_thickness = thickness;
	}
	emit_changed();
}

int VoxelGeneratorPlanet::get_atmosphere_thickness() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.atmosphere_thickness;
}

void VoxelGeneratorPlanet::set_atmospheric_composition(TypedArray<VoxelPlanetAtmosphericGas> composition) {
	zylann::godot::copy_to(_atmospheric_composition, composition);
	StdVector<Parameters::GasItem> items;
	items.resize(_atmospheric_composition.size());
	for (size_t i = 0; i < _atmospheric_composition.size(); ++i) {
		if (_atmospheric_composition[i].is_valid()) {
			items[i].name = _atmospheric_composition[i]->get_gas_name();
			items[i].percentage = _atmospheric_composition[i]->get_percentage();
		}
	}
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.atmospheric_composition = items;
	}
	emit_changed();
}

TypedArray<VoxelPlanetAtmosphericGas> VoxelGeneratorPlanet::get_atmospheric_composition() const {
	return zylann::godot::to_typed_array(to_span(_atmospheric_composition));
}

void VoxelGeneratorPlanet::set_water_table_radius(int radius) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.water_table_radius = radius;
	}
	emit_changed();
}

int VoxelGeneratorPlanet::get_water_table_radius() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.water_table_radius;
}

void VoxelGeneratorPlanet::set_mineral_composition(TypedArray<VoxelPlanetMineral> composition) {
	zylann::godot::copy_to(_mineral_composition, composition);
	StdVector<Parameters::MineralItem> items;
	items.resize(_mineral_composition.size());
	for (size_t i = 0; i < _mineral_composition.size(); ++i) {
		if (_mineral_composition[i].is_valid()) {
			items[i].name = _mineral_composition[i]->get_mineral_name();
			items[i].percentage = _mineral_composition[i]->get_percentage();
		}
	}
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.mineral_composition = items;
	}
	emit_changed();
}

TypedArray<VoxelPlanetMineral> VoxelGeneratorPlanet::get_mineral_composition() const {
	return zylann::godot::to_typed_array(to_span(_mineral_composition));
}

void VoxelGeneratorPlanet::set_min_surface_temperature(float temp) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.min_surface_temperature = math::clamp(temp, 0.f, MAX_TEMPERATURE_KELVIN);
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_min_surface_temperature() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.min_surface_temperature;
}

void VoxelGeneratorPlanet::set_max_surface_temperature(float temp) {
	{
		RWLockWrite wlock(_parameters_lock);
		_parameters.max_surface_temperature = math::clamp(temp, 0.f, MAX_TEMPERATURE_KELVIN);
	}
	emit_changed();
}

float VoxelGeneratorPlanet::get_max_surface_temperature() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.max_surface_temperature;
}

void VoxelGeneratorPlanet::_update_slope_thresholds(Parameters &params) {
	const float pi = math::PI<float>;
	const float min_rad = math::deg_to_rad(params.detail_noise_min_slope_degrees);
	const float max_rad = math::deg_to_rad(params.detail_noise_max_slope_degrees);
	params.slope_lo_in_rad = min_rad;
	params.slope_hi_in_rad = max_rad;
	params.slope_lo_out_rad =
			math::max(min_rad - math::deg_to_rad(params.detail_noise_min_slope_falloff_degrees), 0.f);
	params.slope_hi_out_rad =
			math::min(max_rad + math::deg_to_rad(params.detail_noise_max_slope_falloff_degrees), pi);
	params.slope_lo_in_cos = Math::cos(params.slope_lo_in_rad);
	params.slope_hi_in_cos = Math::cos(params.slope_hi_in_rad);
	params.slope_lo_out_cos = Math::cos(params.slope_lo_out_rad);
	params.slope_hi_out_cos = Math::cos(params.slope_hi_out_rad);
}

float VoxelGeneratorPlanet::_base_sdf(float pos_x, float pos_y, float pos_z, const Parameters &params) const {
	const float d = Math::Fast_Sqrt(pos_x * pos_x + pos_y * pos_y + pos_z * pos_z) + 0.0001f;
	float sdf = d - params.radius;
	if (params.height_noise.is_valid() && params.height_noise_amplitude != 0.f) {
		sdf -= params.height_noise_amplitude * params.height_noise->get_noise_3d(pos_x, pos_y, pos_z);
	}
	return sdf;
}

float VoxelGeneratorPlanet::_compute_slope_weight(
		float pos_x,
		float pos_y,
		float pos_z,
		float epsilon,
		const Parameters &params
) const {
	// Surface normal from the gradient of the base SDF.
	const float e = epsilon;
	const float gx = _base_sdf(pos_x + e, pos_y, pos_z, params) - _base_sdf(pos_x - e, pos_y, pos_z, params);
	const float gy = _base_sdf(pos_x, pos_y + e, pos_z, params) - _base_sdf(pos_x, pos_y - e, pos_z, params);
	const float gz = _base_sdf(pos_x, pos_y, pos_z + e, params) - _base_sdf(pos_x, pos_y, pos_z - e, params);

	const float g_len2 = gx * gx + gy * gy + gz * gz;
	const float p_len2 = pos_x * pos_x + pos_y * pos_y + pos_z * pos_z;

	// Degenerate cases (flat field or planet center): consider the surface horizontal.
	float cosine = 1.f;
	if (g_len2 > 1e-12f && p_len2 > 1e-12f) {
		const float inv = 1.f / Math::sqrt(g_len2 * p_len2);
		cosine = math::clamp((gx * pos_x + gy * pos_y + gz * pos_z) * inv, -1.f, 1.f);
	}

	// Cosine decreases as the slope angle increases, so the inner band is bracketed in reverse.
	if (cosine <= params.slope_lo_in_cos && cosine >= params.slope_hi_in_cos) {
		return 1.f;
	}
	if (cosine > params.slope_lo_in_cos) {
		if (cosine >= params.slope_lo_out_cos) {
			return 0.f;
		}
		const float angle = Math::acos(cosine);
		return math::clamp(
				(angle - params.slope_lo_out_rad) / (params.slope_lo_in_rad - params.slope_lo_out_rad), 0.f, 1.f
		);
	}
	if (cosine <= params.slope_hi_out_cos) {
		return 0.f;
	}
	const float angle = Math::acos(cosine);
	return math::clamp(
			(params.slope_hi_out_rad - angle) / (params.slope_hi_out_rad - params.slope_hi_in_rad), 0.f, 1.f
	);
}

float VoxelGeneratorPlanet::_compute_detail_noise_slope_mask(
		float pos_x,
		float pos_y,
		float pos_z,
		float base_sdf,
		float stride,
		const Parameters &params
) const {
	if (!params.detail_noise_enabled) {
		return 0.f;
	}
	// The slope gradient is expensive, and only voxels near the surface can make use of it.
	const float band = params.detail_noise_amplitude + 2.f * stride;
	if (Math::abs(base_sdf) > band) {
		return 0.f;
	}
	return _compute_slope_weight(pos_x, pos_y, pos_z, stride, params);
}

float VoxelGeneratorPlanet::_compute_data6(float pos_x, float pos_y, float pos_z, const Parameters &params) const {
	if (!params.data6_water_table_enabled) {
		return 0.f;
	}
	const float max_distance = params.data6_water_table_max_distance;
	if (max_distance <= 0.f) {
		return 0.f;
	}
	const float d = Math::Fast_Sqrt(pos_x * pos_x + pos_y * pos_y + pos_z * pos_z);
	const float t = Math::abs(d - float(params.water_table_radius)) / max_distance;
	if (t >= 1.f) {
		return 0.f;
	}
	// Exponential falloff, rescaled so it reaches exactly 0 at `max_distance`.
	constexpr float k = 5.f;
	constexpr float floor_value = 0.006737947f; // exp(-k)
	return (Math::exp(-k * t) - floor_value) / (1.f - floor_value);
}

float VoxelGeneratorPlanet::_compute_data7(float pos_x, float pos_y, float pos_z, const Parameters &params) const {
	if (!params.data7_latitude_enabled) {
		return 0.f;
	}
	const float t_min = math::min(params.min_surface_temperature, params.max_surface_temperature);
	const float t_max = math::max(params.min_surface_temperature, params.max_surface_temperature);
	const float inv_scale = 1.f / MAX_TEMPERATURE_KELVIN;
	if (params.radius <= 0.f) {
		return math::clamp(params.max_surface_temperature, t_min, t_max) * inv_scale;
	}
	// The planet is generated centered on the origin, so the local Y axis is the polar axis.
	const float latitude_t = math::clamp(Math::abs(pos_y) / params.radius, 0.f, 1.f);
	float altitude_t = 0.f;
	if (params.atmosphere_thickness > 0) {
		const float d = Math::Fast_Sqrt(pos_x * pos_x + pos_y * pos_y + pos_z * pos_z);
		altitude_t = math::clamp((d - params.radius) / float(params.atmosphere_thickness), 0.f, 1.f);
	}
	const float t = math::clamp(latitude_t + altitude_t, 0.f, 1.f);
	const float kelvin =
			params.max_surface_temperature + (params.min_surface_temperature - params.max_surface_temperature) * t;
	return math::clamp(kelvin, t_min, t_max) * inv_scale;
}

float VoxelGeneratorPlanet::_apply_noise(
		float sdf,
		float pos_x,
		float pos_y,
		float pos_z,
		float slope_weight,
		const Parameters &params
) const {
	if (!params.detail_noise_enabled || params.detail_noise.is_null()) {
		return sdf;
	}
	// The slope mask attenuates the detail noise.
	const float attenuation = math::clamp(slope_weight, 0.f, 1.f);
	const float amplitude = params.detail_noise_amplitude * attenuation;
	if (amplitude <= 0.f) {
		return sdf;
	}
	const ZN_FastNoiseLite &noise = **params.detail_noise;
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

	const Vector3i bs = out_buffer.get_size();
	const int stride = 1 << input.lod;
	const float stride_f = float(stride);

	const bool has_any_data =
			params.detail_noise_enabled || params.data6_water_table_enabled || params.data7_latitude_enabled;
	const ZN_FastNoiseLite *height_noise = params.height_noise.is_valid() ? &**params.height_noise : nullptr;
	const float height_noise_amp = params.height_noise_amplitude;

	int gz = input.origin_in_voxels.z;
	for (int z = 0; z < bs.z; ++z, gz += stride) {
		int gx = input.origin_in_voxels.x;
		for (int x = 0; x < bs.x; ++x, gx += stride) {
			int gy = input.origin_in_voxels.y;
			for (int y = 0; y < bs.y; ++y, gy += stride) {
				const float d = Math::Fast_Sqrt(float(gx * gx + gy * gy + gz * gz)) + 0.0001f;
				float base_sdf = d - params.radius;
				if (height_noise != nullptr && height_noise_amp != 0.f) {
					base_sdf -= height_noise_amp * height_noise->get_noise_3d(gx, gy, gz);
				}

				const float slope_weight = _compute_detail_noise_slope_mask(gx, gy, gz, base_sdf, stride_f, params);

				const float sdf = _apply_noise(base_sdf, gx, gy, gz, slope_weight, params);
				out_buffer.set_voxel_f(sdf, x, y, z, VoxelBuffer::CHANNEL_SDF);

				if (has_any_data) {
					out_buffer.set_voxel_f(slope_weight, x, y, z, VoxelBuffer::CHANNEL_DATA5);
					out_buffer.set_voxel_f(_compute_data6(gx, gy, gz, params), x, y, z, VoxelBuffer::CHANNEL_DATA6);
					out_buffer.set_voxel_f(_compute_data7(gx, gy, gz, params), x, y, z, VoxelBuffer::CHANNEL_DATA7);
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
		const float fx = float(pos.x);
		const float fy = float(pos.y);
		const float fz = float(pos.z);
		const float base_sdf = _base_sdf(fx, fy, fz, params);
		const float slope_weight = _compute_detail_noise_slope_mask(fx, fy, fz, base_sdf, 1.f, params);
		v.f = _apply_noise(base_sdf, fx, fy, fz, slope_weight, params);
	} else if (channel == VoxelBuffer::CHANNEL_DATA5) {
		const float base_sdf = _base_sdf(float(pos.x), float(pos.y), float(pos.z), params);
		v.f = _compute_detail_noise_slope_mask(float(pos.x), float(pos.y), float(pos.z), base_sdf, 1.f, params);
	} else if (channel == VoxelBuffer::CHANNEL_DATA6) {
		v.f = _compute_data6(float(pos.x), float(pos.y), float(pos.z), params);
	} else if (channel == VoxelBuffer::CHANNEL_DATA7) {
		v.f = _compute_data7(float(pos.x), float(pos.y), float(pos.z), params);
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
		const ZN_FastNoiseLite *height_noise = params.height_noise.is_valid() ? &**params.height_noise : nullptr;
		const float height_noise_amp = params.height_noise_amplitude;
		for (size_t i = 0; i < count; ++i) {
			const float px = positions_x[i];
			const float py = positions_y[i];
			const float pz = positions_z[i];
			const float d = Math::Fast_Sqrt(px * px + py * py + pz * pz) + 0.0001f;
			float base_sdf = d - params.radius;
			if (height_noise != nullptr && height_noise_amp != 0.f) {
				base_sdf -= height_noise_amp * height_noise->get_noise_3d(px, py, pz);
			}
			const float slope_weight = _compute_detail_noise_slope_mask(px, py, pz, base_sdf, 1.f, params);
			out_values[i] = _apply_noise(base_sdf, px, py, pz, slope_weight, params);
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA5) {
		for (size_t i = 0; i < count; ++i) {
			const float base_sdf = _base_sdf(positions_x[i], positions_y[i], positions_z[i], params);
			out_values[i] = _compute_detail_noise_slope_mask(
					positions_x[i], positions_y[i], positions_z[i], base_sdf, 1.f, params
			);
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA6) {
		for (size_t i = 0; i < count; ++i) {
			out_values[i] = _compute_data6(positions_x[i], positions_y[i], positions_z[i], params);
		}
	} else if (channel == VoxelBuffer::CHANNEL_DATA7) {
		for (size_t i = 0; i < count; ++i) {
			out_values[i] = _compute_data7(positions_x[i], positions_y[i], positions_z[i], params);
		}
	} else {
		for (size_t i = 0; i < count; ++i) {
			out_values[i] = 0.f;
		}
	}
}

void VoxelGeneratorPlanet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_height_noise", "noise"), &VoxelGeneratorPlanet::set_height_noise);
	ClassDB::bind_method(D_METHOD("get_height_noise"), &VoxelGeneratorPlanet::get_height_noise);
	ClassDB::bind_method(
			D_METHOD("set_height_noise_amplitude", "amplitude"), &VoxelGeneratorPlanet::set_height_noise_amplitude
	);
	ClassDB::bind_method(D_METHOD("get_height_noise_amplitude"), &VoxelGeneratorPlanet::get_height_noise_amplitude);

	ClassDB::bind_method(
				D_METHOD("set_detail_noise_min_slope_degrees", "degrees"),
				&VoxelGeneratorPlanet::set_detail_noise_min_slope_degrees
	);
	ClassDB::bind_method(
			D_METHOD("get_detail_noise_min_slope_degrees"), &VoxelGeneratorPlanet::get_detail_noise_min_slope_degrees
	);
	ClassDB::bind_method(
			D_METHOD("set_detail_noise_max_slope_degrees", "degrees"),
			&VoxelGeneratorPlanet::set_detail_noise_max_slope_degrees
	);
	ClassDB::bind_method(
			D_METHOD("get_detail_noise_max_slope_degrees"), &VoxelGeneratorPlanet::get_detail_noise_max_slope_degrees
	);
	ClassDB::bind_method(
			D_METHOD("set_detail_noise_min_slope_falloff_degrees", "degrees"),
			&VoxelGeneratorPlanet::set_detail_noise_min_slope_falloff_degrees
	);
	ClassDB::bind_method(
			D_METHOD("get_detail_noise_min_slope_falloff_degrees"),
			&VoxelGeneratorPlanet::get_detail_noise_min_slope_falloff_degrees
	);
	ClassDB::bind_method(
			D_METHOD("set_detail_noise_max_slope_falloff_degrees", "degrees"),
			&VoxelGeneratorPlanet::set_detail_noise_max_slope_falloff_degrees
	);
	ClassDB::bind_method(
			D_METHOD("get_detail_noise_max_slope_falloff_degrees"),
			&VoxelGeneratorPlanet::get_detail_noise_max_slope_falloff_degrees
	);

	ClassDB::bind_method(
			D_METHOD("set_data6_water_table_enabled", "enabled"), &VoxelGeneratorPlanet::set_data6_water_table_enabled
	);
	ClassDB::bind_method(D_METHOD("is_data6_water_table_enabled"), &VoxelGeneratorPlanet::is_data6_water_table_enabled);
	ClassDB::bind_method(
			D_METHOD("set_data6_water_table_max_distance", "distance"),
			&VoxelGeneratorPlanet::set_data6_water_table_max_distance
	);
	ClassDB::bind_method(
			D_METHOD("get_data6_water_table_max_distance"), &VoxelGeneratorPlanet::get_data6_water_table_max_distance
	);

	ClassDB::bind_method(
			D_METHOD("set_data7_latitude_enabled", "enabled"), &VoxelGeneratorPlanet::set_data7_latitude_enabled
	);
	ClassDB::bind_method(D_METHOD("is_data7_latitude_enabled"), &VoxelGeneratorPlanet::is_data7_latitude_enabled);

	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &VoxelGeneratorPlanet::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &VoxelGeneratorPlanet::get_radius);

	ClassDB::bind_method(D_METHOD("set_detail_noise", "noise"), &VoxelGeneratorPlanet::set_detail_noise);
	ClassDB::bind_method(D_METHOD("get_detail_noise"), &VoxelGeneratorPlanet::get_detail_noise);
	ClassDB::bind_method(D_METHOD("set_detail_noise_enabled", "enabled"), &VoxelGeneratorPlanet::set_detail_noise_enabled);
	ClassDB::bind_method(D_METHOD("is_detail_noise_enabled"), &VoxelGeneratorPlanet::is_detail_noise_enabled);
	ClassDB::bind_method(
			D_METHOD("set_detail_noise_amplitude", "amplitude"), &VoxelGeneratorPlanet::set_detail_noise_amplitude
	);
	ClassDB::bind_method(D_METHOD("get_detail_noise_amplitude"), &VoxelGeneratorPlanet::get_detail_noise_amplitude);

	ClassDB::bind_method(D_METHOD("set_rotation_speed", "speed"), &VoxelGeneratorPlanet::set_rotation_speed);
	ClassDB::bind_method(D_METHOD("get_rotation_speed"), &VoxelGeneratorPlanet::get_rotation_speed);

	ClassDB::bind_method(D_METHOD("set_rotation_axis", "axis"), &VoxelGeneratorPlanet::set_rotation_axis);
	ClassDB::bind_method(D_METHOD("get_rotation_axis"), &VoxelGeneratorPlanet::get_rotation_axis);

	ClassDB::bind_method(
			D_METHOD("set_atmospheric_density", "density"), &VoxelGeneratorPlanet::set_atmospheric_density
	);
	ClassDB::bind_method(D_METHOD("get_atmospheric_density"), &VoxelGeneratorPlanet::get_atmospheric_density);

	ClassDB::bind_method(
			D_METHOD("set_atmosphere_thickness", "thickness"), &VoxelGeneratorPlanet::set_atmosphere_thickness
	);
	ClassDB::bind_method(D_METHOD("get_atmosphere_thickness"), &VoxelGeneratorPlanet::get_atmosphere_thickness);

	ClassDB::bind_method(
			D_METHOD("set_atmospheric_composition", "composition"), &VoxelGeneratorPlanet::set_atmospheric_composition
	);
	ClassDB::bind_method(
			D_METHOD("get_atmospheric_composition"), &VoxelGeneratorPlanet::get_atmospheric_composition
	);

	ClassDB::bind_method(D_METHOD("set_water_table_radius", "radius"), &VoxelGeneratorPlanet::set_water_table_radius);
	ClassDB::bind_method(D_METHOD("get_water_table_radius"), &VoxelGeneratorPlanet::get_water_table_radius);

	ClassDB::bind_method(
			D_METHOD("set_mineral_composition", "composition"), &VoxelGeneratorPlanet::set_mineral_composition
	);
	ClassDB::bind_method(D_METHOD("get_mineral_composition"), &VoxelGeneratorPlanet::get_mineral_composition);

	ClassDB::bind_method(
			D_METHOD("set_min_surface_temperature", "temp"), &VoxelGeneratorPlanet::set_min_surface_temperature
	);
	ClassDB::bind_method(D_METHOD("get_min_surface_temperature"), &VoxelGeneratorPlanet::get_min_surface_temperature);
	ClassDB::bind_method(
			D_METHOD("set_minimum_surface_temperature", "temp"), &VoxelGeneratorPlanet::set_minimum_surface_temperature
	);
	ClassDB::bind_method(
			D_METHOD("get_minimum_surface_temperature"), &VoxelGeneratorPlanet::get_minimum_surface_temperature
	);

	ClassDB::bind_method(
			D_METHOD("set_max_surface_temperature", "temp"), &VoxelGeneratorPlanet::set_max_surface_temperature
	);
	ClassDB::bind_method(D_METHOD("get_max_surface_temperature"), &VoxelGeneratorPlanet::get_max_surface_temperature);
	ClassDB::bind_method(
			D_METHOD("set_maximum_surface_temperature", "temp"), &VoxelGeneratorPlanet::set_maximum_surface_temperature
	);
	ClassDB::bind_method(
			D_METHOD("get_maximum_surface_temperature"), &VoxelGeneratorPlanet::get_maximum_surface_temperature
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
	ADD_GROUP("Detail Noise", "detail_noise");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "detail_noise_min_slope_degrees", PROPERTY_HINT_RANGE, "0.0, 180.0, 0.1"),
			"set_detail_noise_min_slope_degrees",
			"get_detail_noise_min_slope_degrees"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "detail_noise_max_slope_degrees", PROPERTY_HINT_RANGE, "0.0, 180.0, 0.1"),
			"set_detail_noise_max_slope_degrees",
			"get_detail_noise_max_slope_degrees"
	);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::FLOAT,
					"detail_noise_min_slope_falloff_degrees",
					PROPERTY_HINT_RANGE,
					"0.0, 180.0, 0.1"
			),
			"set_detail_noise_min_slope_falloff_degrees",
			"get_detail_noise_min_slope_falloff_degrees"
	);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::FLOAT,
					"detail_noise_max_slope_falloff_degrees",
					PROPERTY_HINT_RANGE,
					"0.0, 180.0, 0.1"
			),
			"set_detail_noise_max_slope_falloff_degrees",
			"get_detail_noise_max_slope_falloff_degrees"
	);
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
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "data6_water_table_enabled"),
			"set_data6_water_table_enabled",
			"is_data6_water_table_enabled"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "data6_water_table_max_distance", PROPERTY_HINT_RANGE, "0.0, 1000.0, 0.01, or_greater"),
			"set_data6_water_table_max_distance",
			"get_data6_water_table_max_distance"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "data7_latitude_enabled"),
			"set_data7_latitude_enabled",
			"is_data7_latitude_enabled"
	);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius"), "set_radius", "get_radius");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "rotation_speed"),
			"set_rotation_speed",
			"get_rotation_speed"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::VECTOR3, "rotation_axis"),
			"set_rotation_axis",
			"get_rotation_axis"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "atmospheric_density"),
			"set_atmospheric_density",
			"get_atmospheric_density"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "atmosphere_thickness"),
			"set_atmosphere_thickness",
			"get_atmosphere_thickness"
	);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::ARRAY,
					"atmospheric_composition",
					PROPERTY_HINT_ARRAY_TYPE,
					MAKE_RESOURCE_TYPE_HINT(VoxelPlanetAtmosphericGas::get_class_static())
			),
			"set_atmospheric_composition",
			"get_atmospheric_composition"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "water_table_radius"),
			"set_water_table_radius",
			"get_water_table_radius"
	);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::ARRAY,
					"mineral_composition",
					PROPERTY_HINT_ARRAY_TYPE,
					MAKE_RESOURCE_TYPE_HINT(VoxelPlanetMineral::get_class_static())
			),
			"set_mineral_composition",
			"get_mineral_composition"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "min_surface_temperature", PROPERTY_HINT_RANGE, "0.0, 6000.0, 0.01"),
			"set_min_surface_temperature",
			"get_min_surface_temperature"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_surface_temperature", PROPERTY_HINT_RANGE, "0.0, 6000.0, 0.01"),
			"set_max_surface_temperature",
			"get_max_surface_temperature"
	);
}

} // namespace zylann::voxel
