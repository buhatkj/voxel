#include "test_voxel_generator_planet.h"
#include "../../generators/simple/voxel_generator_planet.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/testing/test_macros.h"

#ifdef VOXEL_ENABLE_GPU
#include "../../engine/voxel_engine.h"
#include "../../generators/generate_block_gpu_task.h"
#include "../../util/godot/classes/time.h"
#include "../../util/string/format.h"
#include <atomic>
#endif

namespace zylann::voxel::tests {

void test_voxel_generator_planet_large_coordinates() {
	Ref<VoxelGeneratorPlanet> generator;
	generator.instantiate();
	ZN_TEST_ASSERT(generator.is_valid());
	ZN_TEST_ASSERT(generator->get_reference_count() == 1);
	ZN_TEST_ASSERT(generator->get_height_noise().is_valid());
	generator->set_height_noise(Ref<ZN_FastNoiseLite>());
	generator->set_radius(100000.f);
	const Vector3i origin(100000, -10, -10);
	VoxelBuffer voxels(VoxelBuffer::ALLOCATOR_DEFAULT);
	voxels.create(Vector3i(3, 3, 3));
	voxels.set_channel_depth(VoxelBuffer::CHANNEL_SDF, VoxelBuffer::DEPTH_32_BIT);
	generator->generate_block({ voxels, origin, 2 });
	Vector3i pos;
	for (pos.z = 0; pos.z < 3; ++pos.z) {
		for (pos.x = 0; pos.x < 3; ++pos.x) {
			for (pos.y = 0; pos.y < 3; ++pos.y) {
				const float expected = generator->generate_single(origin + (pos << 2), VoxelBuffer::CHANNEL_SDF).f;
				ZN_TEST_ASSERT(Math::is_finite(expected));
				ZN_TEST_ASSERT(voxels.get_voxel_f(pos, VoxelBuffer::CHANNEL_SDF) == expected);
			}
		}
	}
}

#ifdef VOXEL_ENABLE_GPU
namespace {

struct PlanetDispatchState {
	StdVector<GenerateBlockGPUTaskResult> results;
	bool failed = false;
	std::atomic_bool complete{ false };
};

struct PlanetConsumerTask : IGeneratingVoxelsThreadedTask {
	std::shared_ptr<PlanetDispatchState> state;

	void set_gpu_results(StdVector<GenerateBlockGPUTaskResult> &&results) override {
		state->results = std::move(results);
	}

	void set_gpu_failed() override {
		state->failed = true;
	}

	void run(ThreadedTaskContext &) override {
		state->complete.store(true);
	}
};

void check_planet_gpu(VoxelGeneratorPlanet &generator, Vector3i origin, uint8_t lod) {
	VoxelGenerator::ShaderSourceData source;
	ZN_TEST_ASSERT(generator.supports_shaders());
	ZN_TEST_ASSERT(generator.get_shader_source(source));
	ZN_TEST_ASSERT(source.uses_voxel_size);
	ZN_TEST_ASSERT(source.outputs.size() == 4);
	ZN_TEST_ASSERT(source.outputs[0].type == VoxelGenerator::ShaderOutput::TYPE_SDF);
	ZN_TEST_ASSERT(source.outputs[1].type == VoxelGenerator::ShaderOutput::TYPE_DATA5);
	ZN_TEST_ASSERT(source.outputs[2].type == VoxelGenerator::ShaderOutput::TYPE_DATA6);
	ZN_TEST_ASSERT(source.outputs[3].type == VoxelGenerator::ShaderOutput::TYPE_DATA7);

	generator.compile_shaders();
	const VoxelGenerator::BlockRenderingShader shader = generator.get_block_rendering_shader_data();
	ZN_TEST_ASSERT(shader.is_valid());

	const Vector3i size(5, 4, 3);
	VoxelBuffer expected(VoxelBuffer::ALLOCATOR_DEFAULT);
	VoxelBuffer actual(VoxelBuffer::ALLOCATOR_DEFAULT);
	expected.create(size);
	actual.create(size);
	const unsigned int channels[] = {
		VoxelBuffer::CHANNEL_SDF, VoxelBuffer::CHANNEL_DATA5, VoxelBuffer::CHANNEL_DATA6, VoxelBuffer::CHANNEL_DATA7
	};
	for (const unsigned int channel : channels) {
		expected.set_channel_depth(channel, VoxelBuffer::DEPTH_32_BIT);
		actual.set_channel_depth(channel, VoxelBuffer::DEPTH_32_BIT);
	}
	generator.generate_block({ expected, origin, lod });

	const std::shared_ptr<PlanetDispatchState> state = make_shared_instance<PlanetDispatchState>();
	PlanetConsumerTask *consumer = ZN_NEW(PlanetConsumerTask);
	consumer->state = state;
	GenerateBlockGPUTask *task = ZN_NEW(GenerateBlockGPUTask);
	task->consumer_task = consumer;
	task->generator_shader = shader.shader;
	task->generator_shader_params = shader.parameters;
	task->generator_shader_outputs = shader.outputs;
	task->origin_in_voxels = origin;
	task->lod_index = lod;
	task->boxes_to_generate.push_back(Box3i(Vector3i(), size));
	VoxelEngine::get_singleton().push_gpu_task(task);

	const uint64_t start_time = Time::get_singleton()->get_ticks_usec();
	while (!state->complete.load()) {
		Thread::sleep_usec(1000);
		ZN_TEST_ASSERT(Time::get_singleton()->get_ticks_usec() - start_time < 10'000'000);
	}
	ZN_TEST_ASSERT(!state->failed);
	ZN_TEST_ASSERT(state->results.size() == 4);
	GenerateBlockGPUTaskResult::convert_to_voxel_buffer(to_span(state->results), actual);

	Vector3i pos;
	for (pos.z = 0; pos.z < size.z; ++pos.z) {
		for (pos.x = 0; pos.x < size.x; ++pos.x) {
			for (pos.y = 0; pos.y < size.y; ++pos.y) {
				for (const unsigned int channel : channels) {
					const float a = actual.get_voxel_f(pos, channel);
					const float b = expected.get_voxel_f(pos, channel);
					const float tolerance = channel == VoxelBuffer::CHANNEL_SDF
							? 0.02f
							: (channel == VoxelBuffer::CHANNEL_DATA5 ? 0.01f : 0.0001f);
					if (!Math::is_finite(a) || Math::abs(a - b) > tolerance) {
						ZN_PRINT_ERROR(format(
								"Planet GPU mismatch: channel {}, LOD {}, position ({}, {}, {}), GPU {}, CPU {}",
								channel, lod, pos.x, pos.y, pos.z, a, b
						));
						ZN_TEST_ASSERT(false);
					}
				}
			}
		}
	}
}

} // namespace

void test_voxel_generator_planet_gpu() {
	VoxelEngine::get_singleton().try_initialize_gpu_features();
	Ref<VoxelGeneratorPlanet> generator;
	generator.instantiate();
	generator->set_height_noise(Ref<ZN_FastNoiseLite>());
	check_planet_gpu(**generator, Vector3i(-2, -2, -2), 0);

	generator->set_radius(32.f);
	ZN_TEST_ASSERT(!generator->get_block_rendering_shader_data().is_valid());
	ZN_TEST_ASSERT(generator->get_detail_rendering_shader() == nullptr);
	generator->set_detail_noise_enabled(true);
	generator->set_moisture_enabled(true);
	generator->set_moisture_water_table_radius(30);
	generator->set_moisture_max_distance(8.f);
	generator->set_temperature_blend_by_latitude_altitude_enabled(true);
	generator->set_atmosphere_thickness(10);
	generator->set_min_surface_temperature(200.f);
	generator->set_max_surface_temperature(320.f);
	check_planet_gpu(**generator, Vector3i(-4, 28, -4), 2);

	Ref<ZN_FastNoiseLite> height;
	height.instantiate();
	height->set_period(19.f);
	height->set_fractal_weighted_strength(0.6f);
	height->set_fractal_ping_pong_strength(3.f);
	height->set_rotation_type_3d(ZN_FastNoiseLite::ROTATION_3D_IMPROVE_XZ_PLANES);
	Ref<ZN_FastNoiseLiteGradient> warp;
	warp.instantiate();
	warp->set_amplitude(2.f);
	warp->set_period(23.f);
	warp->set_rotation_type_3d(ZN_FastNoiseLiteGradient::ROTATION_3D_IMPROVE_XY_PLANES);
	height->set_warp_noise(warp);
	generator->set_height_noise(height);
	generator->set_height_noise_amplitude(4.f);

	Ref<ZN_FastNoiseLite> detail;
	detail.instantiate();
	detail->set_noise_type(ZN_FastNoiseLite::TYPE_PERLIN);
	detail->set_period(7.f);
	detail->set_seed(-57);
	generator->set_detail_noise(detail);
	generator->set_detail_noise_amplitude(1.5f);
	generator->set_detail_noise_min_slope_degrees(10.f);
	generator->set_detail_noise_max_slope_degrees(50.f);
	generator->set_detail_noise_min_slope_falloff_degrees(10.f);
	generator->set_detail_noise_max_slope_falloff_degrees(30.f);

	for (int i = 0; i < 6; ++i) {
		height->set_noise_type(static_cast<ZN_FastNoiseLite::NoiseType>(i));
		height->set_fractal_type(static_cast<ZN_FastNoiseLite::FractalType>(i % 4));
		warp->set_noise_type(static_cast<ZN_FastNoiseLiteGradient::NoiseType>(i % 3));
		warp->set_fractal_type(static_cast<ZN_FastNoiseLiteGradient::FractalType>(i % 3));
		check_planet_gpu(**generator, Vector3i(-4, 28, -4), 0);
		check_planet_gpu(**generator, Vector3i(-4, 28, -4), 2);
		// Nested noise changes must invalidate both shader caches.
		warp->set_seed(warp->get_seed() + 1);
		ZN_TEST_ASSERT(!generator->get_block_rendering_shader_data().is_valid());
		ZN_TEST_ASSERT(generator->get_detail_rendering_shader() == nullptr);
	}

	generator->set_height_noise(Ref<ZN_FastNoiseLite>());
	generator->set_detail_noise_enabled(false);
	generator->set_moisture_max_distance(0.f);
	generator->set_radius(0.f);
	generator->set_min_surface_temperature(320.f);
	generator->set_max_surface_temperature(200.f);
	check_planet_gpu(**generator, Vector3i(-2, -2, -2), 0);
	generator->set_radius(100000.f);
	check_planet_gpu(**generator, Vector3i(100000, -10, -10), 2);
}
#endif

} // namespace zylann::voxel::tests
