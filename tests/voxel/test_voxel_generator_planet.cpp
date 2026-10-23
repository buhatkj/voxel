#include "test_voxel_generator_planet.h"
#include "../../generators/simple/voxel_generator_planet.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/testing/test_macros.h"
#include <array>

namespace zylann::voxel::tests {

namespace {

float sample_moisture(VoxelGeneratorPlanet &generator, Vector3i position) {
	return generator.generate_single(position, VoxelBuffer::CHANNEL_DATA6).f;
}

} // namespace

void test_voxel_generator_planet_moisture() {
	Ref<VoxelGeneratorPlanet> generator;
	generator.instantiate();
	generator->set_moisture_water_table_radius(100);
	generator->set_moisture_max_distance(10.f);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(90, 0, 0)) == 0.f);
	generator->set_moisture_enabled(true);
	ZN_TEST_ASSERT(!generator->is_moisture_climate_enabled());
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i()) == 1.f);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(90, 0, 0)) == 1.f);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(100, 0, 0)) == 1.f);
	// The existing radial distance uses an approximate square root.
	ZN_TEST_ASSERT(Math::abs(sample_moisture(**generator, Vector3i(105, 0, 0)) - 0.5f) < 0.03f);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(120, 0, 0)) == 0.f);
	generator->set_moisture_max_distance(0.f);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(100, 0, 0)) == 1.f);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(101, 0, 0)) == 0.f);
}

void test_voxel_generator_planet_climate() {
	Ref<VoxelGeneratorPlanet> generator;
	generator.instantiate();
	generator->set_radius(100.f);
	generator->set_moisture_enabled(true);
	generator->set_moisture_climate_enabled(true);
	generator->set_moisture_max_distance(0.f);
	generator->set_moisture_upwind_sample_count(0);
	generator->set_min_surface_temperature(293.15f);
	generator->set_max_surface_temperature(293.15f);

	const Vector3i equator(200, 0, 0);
	const float wet = sample_moisture(**generator, equator);
	ZN_TEST_ASSERT(Math::abs(wet - 0.9f) < 0.0001f);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(173, 100, 0)) < wet);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(0, 200, 0)) < wet);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(173, 100, 0)) ==
			sample_moisture(**generator, Vector3i(173, -100, 0)));
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(400, 0, 0)) == wet);

	generator->set_min_surface_temperature(313.15f);
	generator->set_max_surface_temperature(313.15f);
	const float hot = sample_moisture(**generator, equator);
	ZN_TEST_ASSERT(hot < wet);
	// Climate does not depend on whether temperature output to DATA7 is enabled.
	generator->set_temperature_blend_by_latitude_altitude_enabled(true);
	ZN_TEST_ASSERT(sample_moisture(**generator, equator) == hot);

	generator->set_moisture_upwind_search_distance(0.f);
	generator->set_moisture_upwind_sample_count(8);
	ZN_TEST_ASSERT(sample_moisture(**generator, equator) == hot);
	generator->set_moisture_upwind_search_distance(100.f);
	const float inland = sample_moisture(**generator, equator);
	ZN_TEST_ASSERT(inland < hot);
	for (const int sample_count : { 1, 64 }) {
		generator->set_moisture_upwind_sample_count(sample_count);
		ZN_TEST_ASSERT(Math::abs(sample_moisture(**generator, equator) - inland) < 0.00001f);
	}
	generator->set_moisture_water_table_radius(150);
	ZN_TEST_ASSERT(sample_moisture(**generator, equator) > hot);
	ZN_TEST_ASSERT(sample_moisture(**generator, Vector3i(140, 0, 0)) == 1.f);

	generator->set_moisture_upwind_sample_count(-1);
	ZN_TEST_ASSERT(generator->get_moisture_upwind_sample_count() == 0);
	generator->set_moisture_upwind_sample_count(65);
	ZN_TEST_ASSERT(generator->get_moisture_upwind_sample_count() == 64);
	generator->set_moisture_upwind_search_distance(-1.f);
	ZN_TEST_ASSERT(generator->get_moisture_upwind_search_distance() == 0.f);
	generator->set_radius(0.f);
	ZN_TEST_ASSERT(sample_moisture(**generator, equator) == 0.f);
	generator->set_radius(100.f);
	generator->set_moisture_enabled(false);
	ZN_TEST_ASSERT(sample_moisture(**generator, equator) == 0.f);
}

void test_voxel_generator_planet_rain_shadow() {
	Ref<VoxelGeneratorPlanet> generator;
	generator.instantiate();
	Ref<ZN_FastNoiseLite> noise;
	noise.instantiate();
	noise->set_seed(42);
	noise->set_period(120.f);
	generator->set_height_noise(noise);
	generator->set_height_noise_amplitude(0.f);
	generator->set_radius(100.f);
	generator->set_moisture_enabled(true);
	generator->set_moisture_climate_enabled(true);
	generator->set_moisture_max_distance(0.f);
	generator->set_moisture_upwind_sample_count(1);
	generator->set_moisture_upwind_search_distance(150.f);
	generator->set_min_surface_temperature(313.15f);
	generator->set_max_surface_temperature(313.15f);
	const float flat_inland = sample_moisture(**generator, Vector3i(200, 0, 0));
	generator->set_height_noise_amplitude(40.f);

	bool found_barrier = false;
	for (int i = 0; i < 128; ++i) {
		const float longitude = 2.f * math::PI<float> * float(i) / 128.f;
		const Vector3i p(int(200.f * Math::cos(longitude)), 0, int(200.f * Math::sin(longitude)));
		const Vector3f direction = math::normalized(Vector3f(float(p.x), 0.f, float(p.z)));
		const Vector3f east(-direction.z, 0.f, direction.x);
		const Vector3f upwind = direction * Math::cos(1.5f) + east * Math::sin(1.5f);
		const float local_height = noise->get_noise_3d(direction.x * 100.f, 0.f, direction.z * 100.f);
		const float upwind_height = noise->get_noise_3d(upwind.x * 100.f, 0.f, upwind.z * 100.f);
		if (upwind_height > local_height + 0.05f) {
			ZN_TEST_ASSERT(sample_moisture(**generator, p) < flat_inland);
			found_barrier = true;
			break;
		}
	}
	ZN_TEST_ASSERT(found_barrier);
}

void test_voxel_generator_planet_climate_generation_paths() {
	Ref<VoxelGeneratorPlanet> generator;
	generator.instantiate();
	Ref<ZN_FastNoiseLite> noise;
	noise.instantiate();
	noise->set_seed(42);
	noise->set_period(120.f);
	generator->set_height_noise(noise);
	generator->set_height_noise_amplitude(40.f);
	generator->set_radius(100.f);
	generator->set_moisture_water_table_radius(100);
	generator->set_moisture_enabled(true);
	generator->set_moisture_climate_enabled(true);
	generator->set_moisture_upwind_search_distance(150.f);
	generator->set_min_surface_temperature(273.15f);
	generator->set_max_surface_temperature(313.15f);

	VoxelBuffer buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
	buffer.create(Vector3i(2, 2, 2));
	buffer.set_channel_depth(VoxelBuffer::CHANNEL_DATA6, VoxelBuffer::DEPTH_32_BIT);
	const Vector3i origin(180, 30, 20);
	std::array<float, 8> xs;
	std::array<float, 8> ys;
	std::array<float, 8> zs;
	std::array<float, 8> values;
	for (const int sample_count : { 0, 1, 8, 64 }) {
		generator->set_moisture_upwind_sample_count(sample_count);
		for (const uint32_t lod : { 0u, 1u, 3u }) {
			generator->generate_block({ buffer, origin, lod });
			unsigned int i = 0;
			for (int z = 0; z < 2; ++z) {
				for (int x = 0; x < 2; ++x) {
					for (int y = 0; y < 2; ++y, ++i) {
						const Vector3i p = origin + Vector3i(x, y, z) * (1 << lod);
						xs[i] = float(p.x);
						ys[i] = float(p.y);
						zs[i] = float(p.z);
						const float expected = sample_moisture(**generator, p);
						ZN_TEST_ASSERT(expected >= 0.f && expected <= 1.f);
						ZN_TEST_ASSERT(Math::abs(buffer.get_voxel_f(x, y, z, VoxelBuffer::CHANNEL_DATA6) - expected) <
								0.00001f);
					}
				}
			}
			generator->generate_series(
					to_span(xs), to_span(ys), to_span(zs), VoxelBuffer::CHANNEL_DATA6, to_span(values),
					Vector3f(180, 30, 20), Vector3f(188, 38, 28)
			);
			for (unsigned int j = 0; j < values.size(); ++j) {
				const float expected = sample_moisture(**generator, Vector3i(int(xs[j]), int(ys[j]), int(zs[j])));
				ZN_TEST_ASSERT(values[j] == expected);
			}
		}
	}
}

} // namespace zylann::voxel::tests
