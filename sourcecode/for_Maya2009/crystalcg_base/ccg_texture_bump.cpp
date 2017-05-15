#include <math.h>
#include "shader.h"

#pragma warning(disable : 4267)

/*------------------------------------------ ccg_bump_map -------------------*/

struct ccg_bump_map {
	miVector	u;
	miVector	v;
	miVector	coord;
	miVector	step;
	miScalar	factor;
	miBoolean	torus_u;
	miBoolean	torus_v;
	miBoolean	alpha;
	miTag		tex;
	miBoolean	clamp;
	miBoolean	inverse;
};

extern "C" DLLEXPORT int ccg_bump_map_version(void) {return(2);}

extern "C" DLLEXPORT miBoolean ccg_bump_map(
	miVector	*result,
	miState		*state,
	struct ccg_bump_map *paras)
{
	miTag		tex	= *mi_eval_tag    (&paras->tex);
	miBoolean	alpha	= *mi_eval_boolean(&paras->alpha);
	miVector	coord	= *mi_eval_vector (&paras->coord);
	miVector	step	= *mi_eval_vector (&paras->step);
	miVector	u	= *mi_eval_vector (&paras->u);
	miVector	v	= *mi_eval_vector (&paras->v);
	miScalar	factor	= *mi_eval_scalar (&paras->factor);
	miBoolean	clamp	= *mi_eval_boolean(&paras->clamp);
	miBoolean revert = *mi_eval_boolean(&paras->inverse);
	miVector	coord_u, coord_v;
	miScalar	val, val_u, val_v;
	miColor		color;

	coord_u.x = coord.x + (step.x ? step.x : 0.001f);
	coord_u.y = coord.y;
	coord_u.z = coord.z;
	coord_v.x = coord.x;
	coord_v.y = coord.y + (step.y ? step.y : 0.001f);
	coord_v.z = coord.z;
	if (clamp && (coord.x   < 0 || coord.x   >= 1 ||
		      coord.y   < 0 || coord.y   >= 1 ||
		      coord.z   < 0 || coord.z   >= 1 ||
		      coord_u.x < 0 || coord_u.x >= 1 ||
		      coord_v.y < 0 || coord_v.y >= 1)) {
		*result = state->normal;
		return(miTRUE);
	}
	if (!tex || !mi_lookup_color_texture(&color, state, tex, &coord)) {
		*result = state->normal;
		return(miFALSE);
	}
	val = alpha ? color.a : (color.r + color.g + color.b) / 3;

	if (*mi_eval_boolean(&paras->torus_u)) {
		coord_u.x -= (miScalar)floor(coord_u.x);
		coord_u.y -= (miScalar)floor(coord_u.y);
		coord_u.z -= (miScalar)floor(coord_u.z);
	}
	mi_flush_cache(state);
	val_u = mi_lookup_color_texture(&color, state, tex, &coord_u)
		? alpha ? color.a : (color.r + color.g + color.b) / 3 : val;

	if (*mi_eval_boolean(&paras->torus_v)) {
		coord_v.x -= (miScalar)floor(coord_v.x);
		coord_v.y -= (miScalar)floor(coord_v.y);
		coord_v.z -= (miScalar)floor(coord_v.z);
	}
	mi_flush_cache(state);
	val_v = mi_lookup_color_texture(&color, state, tex, &coord_v)
		? alpha ? color.a : (color.r + color.g + color.b) / 3 : val;

	val_u -= val;
	val_v -= val;
	if(!revert){
		state->normal.x += factor * (u.x * val_u + v.x * val_v);
		state->normal.y += factor * (u.y * val_u + v.y * val_v);
		state->normal.z += factor * (u.z * val_u + v.z * val_v);
	}else {
			state->normal.x -= factor * (u.x * val_u + v.x * val_v);
			state->normal.y -= factor * (u.y * val_u + v.y * val_v);
			state->normal.z -= factor * (u.z * val_u + v.z * val_v);
		}
	mi_vector_normalize(&state->normal);

	state->dot_nd = mi_vector_dot(&state->normal, &state->dir);
	*result = state->normal;
	return(miTRUE);
}

/*----------------------------------------- ccg_bump_map2 -------------------*/

struct ccg_bump_map2 {
    miScalar	factor;	    /* How strong the bumps are */
    miScalar	scale;	    /* How many time the bumps are repeated */
    miTag	tex;	    /* The Texture to lookup */
    miColor	color;	    /* The attach illumination branch (ex. Phong) */
	miBoolean	inverse;
};

extern "C" DLLEXPORT int ccg_bump_map2_version(void) {return(1);}

/*-----------------------------------------------------------------------------
 * The bump map will look at 3 texture samples from the texture, using the
 * texture space 0. Base on the lighting differences, this will indicate the 
 * bending of the normal.
 * The normal will be bend in the direction of the bump_vectors if they
 * exist, or in the direction of the derivatives.
 *
 * The modify normal is save in the "state", which then call the evaluation
 * of color. All attach shaders will then used this modify normal.
 *
 * The normal is set back to its original value at the end.
 *---------------------------------------------------------------------------*/
extern "C" DLLEXPORT miBoolean ccg_bump_map2(
    miColor		    *result,
    miState		    *state,
    struct ccg_bump_map2    *paras)
{
    miVector	temp = state->normal;
    miVector	normal = state->normal;
    miVector	coord_u, coord_v;
    miColor	color;
    float	val, val_u, val_v;
    miTag	tex	= *mi_eval_tag    (&paras->tex);
    miScalar	scale	= *mi_eval_scalar (&paras->scale);
    float	factor	= *mi_eval_scalar (&paras->factor);
	miBoolean revert = *mi_eval_boolean(&paras->inverse);
    miVector	*tangent;
    miVector	*binormal;
    int		num;

    miVector coord;
    coord.x = state->tex_list[0].x * scale;
    coord.y = state->tex_list[0].y * scale;
    coord.x -= (miScalar)floor(coord.x);
    coord.y -= (miScalar)floor(coord.y);
    coord_u = coord_v = coord;
    coord_u.x += 0.001f;
    coord_v.y += 0.001f;

    mi_lookup_color_texture(&color, state, tex, &coord);
    val = (color.r + color.g + color.b) / 3;

    mi_lookup_color_texture(&color, state, tex, &coord_u);
    val_u = (color.r + color.g + color.b) / 3;

    mi_lookup_color_texture(&color, state, tex, &coord_v);
    val_v = (color.r + color.g + color.b) / 3;

    val_u -= val;
    val_v -= val;

    /* First choose the bump vectors if they exist or take the derivatives */
    if (mi_query(miQ_NUM_BUMPS, state, 0, &num) && num) {
	tangent = &state->bump_x_list[0];
	binormal = &state->bump_y_list[0];
    } else {
	tangent = &state->derivs[0];
	binormal = &state->derivs[1];
    }


	if(!revert){
		normal.x += factor * (tangent->x  * val_u + 
			  binormal->x * val_v);
		normal.y += factor * (tangent->y  * val_u + 
			  binormal->y * val_v);
		normal.z += factor * (tangent->z  * val_u + 
			  binormal->z * val_v);
	}else {
			normal.x -= factor * (tangent->x  * val_u + 
			  binormal->x * val_v);
			normal.y -= factor * (tangent->y  * val_u + 
			  binormal->y * val_v);
			normal.z -= factor * (tangent->z  * val_u + 
			  binormal->z * val_v);
		}
    mi_vector_normalize(&normal);

    /* The evaluation of "color" will use the modify state normal */
    state->normal = normal;
    *result = *mi_eval_color(&paras->color);
    state->normal = temp;	/* return to original state */

    return miTRUE;
}


/*------------------------------------------ ccg_passthrough_bump_map -------*/

extern "C" DLLEXPORT int ccg_passthrough_bump_map_version(void) {return(2);}

extern "C" DLLEXPORT miBoolean ccg_passthrough_bump_map(
    miColor		*result,
    miState		*state,
struct ccg_bump_map *paras)
{
    miVector	dummy;

    return(ccg_bump_map(&dummy, state, paras));
}


/*------------------------------------------ ccg_bump_procedural -------------------*/

struct ccg_bump_procedural {
	miVector	u;
	miVector	v;
	miVector	step;
	miScalar	factor;
	miBoolean	torus_u;
	miBoolean	torus_v;
	miBoolean	alpha;
	miColor		color;
	miBoolean	clamp;
	miBoolean	inverse;
};

extern "C" DLLEXPORT int ccg_bump_procedural_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_bump_procedural(
	miVector	*result,
	miState		*state,
	struct ccg_bump_procedural *paras)
{
	miBoolean	alpha	= *mi_eval_boolean(&paras->alpha);
	miVector	step	= *mi_eval_vector (&paras->step);
	miVector	u	= *mi_eval_vector (&paras->u);
	miVector	v	= *mi_eval_vector (&paras->v);
	miScalar	factor	= *mi_eval_scalar (&paras->factor);
	miBoolean	clamp	= *mi_eval_boolean(&paras->clamp);
	miBoolean revert = *mi_eval_boolean(&paras->inverse);
	miVector	coord_u, coord_v;
	miScalar	val, val_u, val_v;
	miColor		color;

	miVector	orig_texCoord = state->tex_list[0];
	miVector	coord	= state->tex_list[0];

	coord_u.x = coord.x + (step.x ? step.x : 0.001f);
	coord_u.y = coord.y;
	coord_u.z = coord.z;
	coord_v.x = coord.x;
	coord_v.y = coord.y + (step.y ? step.y : 0.001f);
	coord_v.z = coord.z;
	if (clamp && (coord.x   < 0 || coord.x   >= 1 ||
		      coord.y   < 0 || coord.y   >= 1 ||
		      coord.z   < 0 || coord.z   >= 1 ||
		      coord_u.x < 0 || coord_u.x >= 1 ||
		      coord_v.y < 0 || coord_v.y >= 1)) {
		*result = state->normal;
		return(miTRUE);
	}

	state->tex_list[0] = coord;
	color = *mi_eval_color(&paras->color);
	val = alpha ? color.a : (color.r + color.g + color.b) / 3;

	if (*mi_eval_boolean(&paras->torus_u)) {
		coord_u.x -= (miScalar)floor(coord_u.x);
		coord_u.y -= (miScalar)floor(coord_u.y);
		coord_u.z -= (miScalar)floor(coord_u.z);
	}
	state->tex_list[0] = coord_u;
	mi_flush_cache(state);
	color = *mi_eval_color(&paras->color);
	val_u = alpha ? color.a : (color.r + color.g + color.b) / 3;

	if (*mi_eval_boolean(&paras->torus_v)) {
		coord_v.x -= (miScalar)floor(coord_v.x);
		coord_v.y -= (miScalar)floor(coord_v.y);
		coord_v.z -= (miScalar)floor(coord_v.z);
	}
	state->tex_list[0] = coord_v;
	mi_flush_cache(state);
	color = *mi_eval_color(&paras->color);
	val_v = alpha ? color.a : (color.r + color.g + color.b) / 3;

	val_u -= val;
	val_v -= val;
	if(!revert){
		state->normal.x += factor * (u.x * val_u + v.x * val_v);
		state->normal.y += factor * (u.y * val_u + v.y * val_v);
		state->normal.z += factor * (u.z * val_u + v.z * val_v);
	}else {
			state->normal.x -= factor * (u.x * val_u + v.x * val_v);
			state->normal.y -= factor * (u.y * val_u + v.y * val_v);
			state->normal.z -= factor * (u.z * val_u + v.z * val_v);
		}
	mi_vector_normalize(&state->normal);

	state->dot_nd = mi_vector_dot(&state->normal, &state->dir);
	*result = state->normal;

	//recover original tex vector
	state->tex_list[0] = orig_texCoord;

	return(miTRUE);
}