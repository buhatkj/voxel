#ifndef ZN_FAST_NOISE_LITE_SHADER_H
#define ZN_FAST_NOISE_LITE_SHADER_H

#include "fast_noise_lite.h"

namespace zylann {

// Accepts either CodeGenHelper or the graph's ShaderGenContext.
template <typename T>
void add_fast_noise_lite_gradient_state_config(T &ctx, const ZN_FastNoiseLiteGradient &fnl) {
	ctx.add_format(
			"fnl_state warp_state = fnlCreateState({});\n"
			"warp_state.domain_warp_type = {};\n"
			"warp_state.domain_warp_amp = {};\n"
			"warp_state.fractal_type = {};\n"
			"warp_state.octaves = {};\n"
			"warp_state.gain = {};\n"
			"warp_state.frequency = {};\n"
			"warp_state.lacunarity = {};\n"
			"warp_state.rotation_type_3d = {};\n",
			fnl.get_seed(),
			fnl.get_noise_type(),
			fnl.get_amplitude(),
			fnl.get_fractal_type_fnl(),
			fnl.get_fractal_octaves(),
			fnl.get_fractal_gain(),
			1.f / fnl.get_period(),
			fnl.get_fractal_lacunarity(),
			fnl.get_rotation_type_3d()
	);
}

template <typename T>
void add_fast_noise_lite_state_config(T &ctx, const ZN_FastNoiseLite &fnl) {
	ctx.add_format(
			"fnl_state state = fnlCreateState({});\n"
			"state.noise_type = {};\n"
			"state.fractal_type = {};\n"
			"state.octaves = {};\n"
			"state.gain = {};\n"
			"state.frequency = {};\n"
			"state.lacunarity = {};\n"
			"state.cellular_distance_func = {};\n"
			"state.cellular_return_type = {};\n"
			"state.cellular_jitter_mod = {};\n"
			"state.rotation_type_3d = {};\n"
			"state.weighted_strength = {};\n"
			"state.ping_pong_strength = {};\n",
			fnl.get_seed(),
			fnl.get_noise_type(),
			fnl.get_fractal_type(),
			fnl.get_fractal_octaves(),
			fnl.get_fractal_gain(),
			1.f / fnl.get_period(),
			fnl.get_fractal_lacunarity(),
			fnl.get_cellular_distance_function(),
			fnl.get_cellular_return_type(),
			fnl.get_cellular_jitter(),
			fnl.get_rotation_type_3d(),
			fnl.get_fractal_weighted_strength(),
			fnl.get_fractal_ping_pong_strength()
	);
	const Ref<ZN_FastNoiseLiteGradient> warp = fnl.get_warp_noise();
	if (warp.is_valid()) {
		add_fast_noise_lite_gradient_state_config(ctx, **warp);
	}
}

} // namespace zylann

#endif // ZN_FAST_NOISE_LITE_SHADER_H
