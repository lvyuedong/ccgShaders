/**************************************************
Source code originally comes from ccg_base_twosided.cpp by Jan Sandstrom (Pixero)
modified by lvyuedong
***************************************************/

#include <math.h>
#include "shader.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#pragma warning(disable : 4267)
//#pragma warning (disable : 4244)

struct ccg_base_twosided {
  miTag		front_shader;
  miTag		back_shader;
  miBoolean	disableShadowChain;
};

extern "C" DLLEXPORT int ccg_base_twosided_version() { return 1;}

extern "C" DLLEXPORT miBoolean ccg_base_twosided(
  miColor   *result,
  miState   *state,
  struct    ccg_base_twosided *paras)
{
	const miState	*orig_state;
	miState	copy_state;

	miBoolean facing = miTRUE;
	if(state->inv_normal)	facing = miFALSE;

	if (state->type == miRAY_SHADOW)
	{
		miColor	LayerResult_shadow;
		LayerResult_shadow.r = LayerResult_shadow.g = LayerResult_shadow.b = LayerResult_shadow.a = 0;
		//call shadow shaders
		orig_state = state;
		copy_state = *orig_state;
		state = &copy_state;
		if(facing) mi_call_shader(&LayerResult_shadow, miSHADER_SHADOW, state, *mi_eval_tag(&paras->front_shader));
		else mi_call_shader(&LayerResult_shadow, miSHADER_SHADOW, state, *mi_eval_tag(&paras->back_shader));
		state = (miState*)orig_state;

		if(*mi_eval_boolean(&paras->disableShadowChain)){
			result->r = LayerResult_shadow.r;
			result->g = LayerResult_shadow.g;
			result->b = LayerResult_shadow.b;
			result->a = LayerResult_shadow.a;
		}else {
					if(state->options->shadow=='s'){
						ccg_shadow_choose_volume(state);
						mi_trace_shadow_seg(result, state);
					}
					result->r *= LayerResult_shadow.r;
					result->g *= LayerResult_shadow.g;
					result->b *= LayerResult_shadow.b;
					result->a *= LayerResult_shadow.a;
			  }
		return(miTRUE);
	}

	if(state->type == miRAY_DISPLACE) return(miFALSE);

	miColor LayerResult;
	LayerResult.r = LayerResult.g = LayerResult.b = LayerResult.a = 0.0;

	orig_state = state;
	copy_state = *orig_state;
	state = &copy_state;
	if(facing) mi_call_shader(&LayerResult, miSHADER_MATERIAL, state, *mi_eval_tag(&paras->front_shader));
	else mi_call_shader(&LayerResult, miSHADER_MATERIAL, state, *mi_eval_tag(&paras->back_shader));
	state = (miState*)orig_state;

	*result = LayerResult;
	return miTRUE;
}
