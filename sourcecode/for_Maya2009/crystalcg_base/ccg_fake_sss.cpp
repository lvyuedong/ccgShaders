#include <string.h>
#include "shader.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#pragma warning(disable : 4267)

struct ccg_fake_sss {
	miColor		color;
	miVector	dir;
	miScalar	depth;
	miBoolean	exclude_front;
};

extern "C" DLLEXPORT int ccg_fake_sss_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_fake_sss(
  miColor   *result,
  miState   *state,
  struct ccg_fake_sss *paras)
{
	miVector dir = *mi_eval_vector(&paras->dir);
	mi_vector_neg(&dir);
	miColor	col = *mi_eval_color(&paras->color);
	float d = 0;

	miVector org = state->point;
	if (mi_trace_probe(state, &dir, &org)){
		d = float(state->child->dist);
		miState grandchild;
		state->child->child = &grandchild;

		//miBoolean nonCcgshader = miFALSE;
		//miTag	childShaderTag;
		//char	*childDeclName, *compareStr, *pch;
		//childShaderTag = state->child->shader->function_decl;
		//mi_query( miQ_DECL_NAME, NULL, childShaderTag, &childDeclName);
		//compareStr = (char *)mi_mem_allocate(sizeof(char)*(int(strlen(childDeclName))+1));
		//strcpy(compareStr,childDeclName);
		//pch = strstr(compareStr, "ccg_fake_sss");
		//if(!pch) nonCcgshader = miTRUE;
		//mi_mem_release(compareStr);

		miColor	temp;
		float pre_d = 0;
		if(mi_call_material(&temp, state->child)){
			pre_d = temp.a;
		}
		d += pre_d;
	}else d = 0.0f;

	if(state->type == miRAY_EYE){
		miScalar dep = *mi_eval_scalar(&paras->depth);
		if(dep<=0) dep = 0.000001f;
		d = d/dep;
		if(d>1) d = 1.0f;
		int front = *mi_eval_boolean(&paras->exclude_front);
		if(!front) d = 1 - d;

		result->r = d * col.r;
		result->g = d * col.g;
		result->b = d * col.b;
	}
	result->a = d;

	return(miTRUE);
}
