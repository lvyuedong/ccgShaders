#include "shader.h"

struct ccg_lookup_background {
	miVector	zoom;
	miVector	pan;
	miBoolean	torus_u;
	miBoolean	torus_v;
	miTag		tex;
	miBoolean	visible_in_reflection;
	miBoolean	visible_in_refraction;
	miBoolean	visible_in_transparent;
	miBoolean	visible_in_environment;
	miBoolean	visible_in_finalgather;
};

extern "C" DLLEXPORT int ccg_lookup_background_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_lookup_background(
	miColor		*result,
	miState		*state,
	struct ccg_lookup_background *paras)
{
	miVector	*zoom;
	miVector	*pan;
	miVector	coord;
	miTag		tex = *mi_eval_tag(&paras->tex);
	int vrefl = *mi_eval_boolean(&paras->visible_in_reflection);
	int vrefr = *mi_eval_boolean(&paras->visible_in_refraction);
	int vtran = *mi_eval_boolean(&paras->visible_in_transparent);
	int vfina = *mi_eval_boolean(&paras->visible_in_finalgather);
	int venvi = *mi_eval_boolean(&paras->visible_in_environment);

	if(state->type==miRAY_REFLECT && !vrefl) {
		result->r = result->g = result->b = result->a = 0;
		return(miTRUE);
	}
	if(state->type==miRAY_REFRACT && !vrefr) {
		result->r = result->g = result->b = result->a = 0;
		return(miTRUE);
	}
	if(state->type==miRAY_TRANSPARENT && !vtran) {
		result->r = result->g = result->b = result->a = 0;
		return(miTRUE);
	}
	if(state->type==miRAY_ENVIRONMENT && !venvi) {
		result->r = result->g = result->b = result->a = 0;
		return(miTRUE);
	}
	if(state->type==miRAY_FINALGATHER && !vfina) {
		result->r = result->g = result->b = result->a = 0;
		return(miTRUE);
	}

	if (!tex) {
		result->r = result->g = result->b = result->a = 0;
		return(miFALSE);
	}
	zoom = mi_eval_vector(&paras->zoom);
	pan  = mi_eval_vector(&paras->pan);
	coord.x = state->raster_x / state->camera->x_resolution * .9999f;
	coord.y = state->raster_y / state->camera->y_resolution * .9999f;
	coord.z = 0;
	coord.x = pan->x + (zoom->x ? zoom->x * coord.x : coord.x);
	coord.y = pan->y + (zoom->y ? zoom->y * coord.y : coord.y);
	if (*mi_eval_boolean(&paras->torus_u))
		coord.x -= floor(coord.x);
	if (*mi_eval_boolean(&paras->torus_v))
		coord.y -= floor(coord.y);
	if (coord.x < 0 || coord.y < 0 || coord.x >= 1 || coord.y >= 1) {
		result->r = result->g = result->b = result->a = 0;
		return(miTRUE);
	} else
		return(mi_lookup_color_texture(result, state, tex, &coord));
}