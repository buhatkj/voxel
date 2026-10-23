#ifndef VOXEL_TEST_VOXEL_GENERATOR_PLANET_H
#define VOXEL_TEST_VOXEL_GENERATOR_PLANET_H

namespace zylann::voxel::tests {

void test_voxel_generator_planet_large_coordinates();
#ifdef VOXEL_ENABLE_GPU
void test_voxel_generator_planet_gpu();
#endif

} // namespace zylann::voxel::tests

#endif // VOXEL_TEST_VOXEL_GENERATOR_PLANET_H
