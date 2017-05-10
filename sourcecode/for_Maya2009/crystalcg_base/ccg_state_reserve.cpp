
#include "shader.h"
#include "ccgFunction.h"

struct ccg_state_reserve_return {
	miColor		color_result;
	miVector	vector_result;
	miScalar	scalar_result;
};

struct ccg_state_reserve {
	miColor		color_input;
	miVector	vector_input;
	miScalar	scalar_input;
};

extern "C" DLLEXPORT int ccg_state_reserve_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_state_reserve(
	struct ccg_state_reserve_return	*result,
	miState		*state,
	struct ccg_state_reserve *paras)
{
	result->color_result = *ccg_mi_eval_color(state, &paras->color_input);
	result->vector_result = *ccg_mi_eval_vector(state, &paras->vector_input);
	result->scalar_result = *ccg_mi_eval_scalar(state, &paras->scalar_input);

	return(miTRUE);
}