
#include <math.h>
#include "shader.h"

struct ccg_texture_floatAdjust {
	miColor		input;
	miScalar	level;
	miScalar	multiply;
	miScalar	power;
};

extern "C" DLLEXPORT int ccg_texture_floatAdjust_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_texture_floatAdjust(
  miColor   *result,
  miState   *state,
  struct ccg_texture_floatAdjust *paras)
{
	miColor color = *mi_eval_color(&paras->input);
	miScalar low = *mi_eval_scalar(&paras->level);
	miScalar mul = *mi_eval_scalar(&paras->multiply);
	miScalar power = *mi_eval_scalar(&paras->power);
	if(low<=0) low = 0.00001f;
	
	miScalar lumi = color.r * 0.3f + color.g * 0.59f + color.b * 0.11f;
	if(lumi<=low)
	{
		float f = 1 + pow((1-lumi/low), power)*(mul-1);
		result->r = color.r * f;
		result->g = color.g * f;
		result->b = color.b * f;
	}else *result = color;
	
	return(miTRUE);
}

