
#include <math.h>
#include "shader.h"
#include "crystalcg.h"

struct ccg_simple_math_return {
	miColor		color_result;
	miVector	vector_result;
	miScalar	scalar_result;
};

struct ccg_simple_math {
	miInteger	operation_color;
	int			i_colors;
	int			n_colors;
	miColor		colors[1];
	miInteger	operation_vector;
	miBoolean	normalize_input;
	miBoolean	normalize_output;
	int			i_vectors;
	int			n_vectors;
	miVector	vectors[1];
	miInteger	operation_scalar;
	int			i_scalars;
	int			n_scalars;
	miScalar	scalars[1];
};

extern "C" DLLEXPORT int ccg_simple_math_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_simple_math(
	struct ccg_simple_math_return	*result,
	miState		*state,
	struct ccg_simple_math *paras)
{
	//color operation
	int op_c = *mi_eval_integer(&paras->operation_color);
	int i_c = *mi_eval_integer(&paras->i_colors);
	int n_c = *mi_eval_integer(&paras->n_colors);
	miColor color;
	int i;
	for(i=0;i<n_c;i++){
		color = *mi_eval_color(&paras->colors + i_c + i);
		switch(op_c)
		{
			case 0: ccg_color_add(&(result->color_result), &color, &(result->color_result));
					break;
			case 1: if(i==0) ccg_color_sub(&color, &(result->color_result), &(result->color_result));
					else ccg_color_sub(&(result->color_result), &color, &(result->color_result));
					break;
			case 2: if(i==0) ccg_color_assign(&(result->color_result), &color);
					else ccg_color_multiply(&(result->color_result), &color, &(result->color_result));
					break;
			case 3: if(i==0) ccg_color_assign(&(result->color_result), &color);
					else ccg_color_divide(&(result->color_result), &color, &(result->color_result));
					break;
			case 4: if(i==0) ccg_color_assign(&(result->color_result), &color);
					else {
						result->color_result.r = pow(result->color_result.r, color.r);
						result->color_result.g = pow(result->color_result.g, color.g);
						result->color_result.b = pow(result->color_result.b, color.b);
					}
					break;
			case 5: if(i==0) ccg_color_assign(&(result->color_result), &color);
					else ccg_screen_comp(&(result->color_result), &color, &(result->color_result));
					break;
			case 6: if(i==0) ccg_color_assign(&(result->color_result), &color);
					else ccg_over_comp(&(result->color_result), &color, &(result->color_result));
					break;
		}
	}

	//vector operation
	int op_v = *mi_eval_integer(&paras->operation_vector);
	int i_v = *mi_eval_integer(&paras->i_vectors);
	int n_v = *mi_eval_integer(&paras->n_vectors);
	miVector vec;
	int norm = *mi_eval_boolean(&paras->normalize_input);
	for(i=0;i<n_v;i++){
		vec = *mi_eval_vector(&paras->vectors + i_v + i);
		if(norm) mi_vector_normalize(&vec);
		switch(op_v)
		{
			case 0: if(i==0) ccg_vector_assign(&(result->vector_result), &vec);
					else mi_vector_add(&(result->vector_result), &vec, &(result->vector_result));
					break;
			case 1: if(i==0) ccg_vector_assign(&(result->vector_result), &vec);
					else mi_vector_sub(&(result->vector_result), &(result->vector_result), &vec);
					break;
			case 2: if(i==0) ccg_vector_assign(&(result->vector_result), &vec);
					else mi_vector_mul(&(result->vector_result), vec.x);
					break;
			case 3: if(i==0) ccg_vector_assign(&(result->vector_result), &vec);
					else mi_vector_div(&(result->vector_result), vec.x);
					break;
			case 4: if(i==0) ccg_vector_assign(&(result->vector_result), &vec);
					else ccg_vector_init(&(result->vector_result), mi_vector_dot(&(result->vector_result), &vec));
					break;
			case 5: if(i==0) ccg_vector_assign(&(result->vector_result), &vec);
					else mi_vector_prod(&(result->vector_result), &(result->vector_result), &vec);
					break;
			case 6: if(i==0) ccg_vector_assign(&(result->vector_result), &vec);
					else mi_vector_max(&(result->vector_result), &(result->vector_result), &vec);
					break;
			case 7: if(i==0) ccg_vector_assign(&(result->vector_result), &vec);
					else mi_vector_min(&(result->vector_result), &(result->vector_result), &vec);
					break;
		}
	}
	if(*mi_eval_boolean(&paras->normalize_output)) mi_vector_normalize(&(result->vector_result));

	//scalar operation
	int op_s = *mi_eval_integer(&paras->operation_scalar);
	int i_s = *mi_eval_integer(&paras->i_scalars);
	int n_s = *mi_eval_integer(&paras->n_scalars);
	miScalar scal;

	for(i=0;i<n_s;i++){
		scal = *mi_eval_scalar(&paras->scalars + i_s + i);
		switch(op_s)
		{
			case 0: if(i==0) result->scalar_result = scal;
					else result->scalar_result += scal;
					break;
			case 1: if(i==0) result->scalar_result = scal;
					else result->scalar_result -= scal;
					break;
			case 2: if(i==0) result->scalar_result = scal;
					else result->scalar_result *= scal;
					break;
			case 3: if(i==0) result->scalar_result = scal;
					else result->scalar_result = ( scal==0?0:(result->scalar_result/scal) );
					break;
			case 4: if(i==0) result->scalar_result = scal;
					else result->scalar_result = pow(result->scalar_result, scal);
					break;
		}
	}

	return(miTRUE);
}