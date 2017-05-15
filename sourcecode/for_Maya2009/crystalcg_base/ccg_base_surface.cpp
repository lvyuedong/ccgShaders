#include <string.h>
#include "shader.h"
#include "mayaapi.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#pragma warning(disable : 4267)

struct ccg_base_surface {
	miColor		color;
	miColor		transparency;
	miColor		matteOpacity;
	miScalar	depthLimitMin;
	miScalar	depthLimitMax;
	miInteger	layer;
	miBoolean	passesInOnce;
	miTag		fbWriteString;
	miBoolean	disableShadowChain;
};

extern "C" DLLEXPORT void ccg_base_surface_init(      /* init shader */
    miState         *state,
    struct ccg_base_surface *paras,
    miBoolean       *inst_req)
{
	if (!paras) *inst_req = miTRUE;
	else {
			//render pass string
			struct ccg_passfbArray **fbarray;
			mi_query(miQ_FUNC_USERPTR, state, 0, &fbarray);
			*fbarray = (struct ccg_passfbArray *)mi_mem_allocate(sizeof(struct ccg_passfbArray));
			for(int i=0;i<=LAYER_NUM;i++)
			{
				(*fbarray)->passfbArray[i] = 0;
			}
			miTag fbc = *mi_eval_tag(&paras->fbWriteString);
			if(*mi_eval_boolean(&paras->passesInOnce))
			{
				if(!fbc) mi_error("Please set the fbWriteString parameter for ccg_base_surface shader.");
				else {
						char *fbstr = mi_mem_strdup((char*)mi_db_access(fbc));
						mi_db_unpin( fbc );
						ccg_pass_string( fbstr, *fbarray);
						mi_mem_release(fbstr);
					}
			}
		 }
}

extern "C" DLLEXPORT void ccg_base_surface_exit(      /* exit shader */
	miState         *state,
	struct ccg_base_surface *paras)
{
	if(paras)
	{
		struct ccg_passfbArray **fbarray;
		mi_query(miQ_FUNC_USERPTR, state, 0, &fbarray);
		mi_mem_release(*fbarray);
	}
}

extern "C" DLLEXPORT int ccg_base_surface_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_base_surface(
  miColor   *result,
  miState   *state,
  struct ccg_base_surface *paras)
{
	miColor		*color, *transp, *matte;
	int			whichlayer;
	miColor		tempColor;
	//copyright: miColor		copyright;
	miColor		passes[LAYER_NUM];
	miScalar	zmin, zmax, z;    /* limitation of depth */
	int			i;
	miBoolean	passesOnce;
	struct ccg_passfbArray **fbarray;
	struct ccg_raystate *mystate, my;

	miTag	parentShaderTag;
	char	*parentDeclName, *compareStr, *pch;

	miColor	ccg_solid_black = {0,0,0,1};
	miColor	ccg_trans_black = {0,0,0,0};

	transp	= mi_eval_color(&paras->transparency);

	if (state->type == miRAY_SHADOW)
	{
		if(!ccg_color_compare(transp, &ccg_trans_black))
		{
			if(*mi_eval_boolean(&paras->disableShadowChain)){
				result->r = transp->r;
				result->g = transp->g;
				result->b = transp->b;
				result->a = transp->a;
			}else {
						if(state->options->shadow=='s'){
							ccg_shadow_choose_volume(state);
							mi_trace_shadow_seg(result, state);
						}
						result->r *= transp->r;
						result->g *= transp->g;
						result->b *= transp->b;
						result->a *= transp->a;
				  }
			return(miTRUE);
		}else {
				result->r = result->g = result->b = result->a = 0;
				return(miFALSE);
			  }
	}

	if(state->type == miRAY_DISPLACE)
		return(miFALSE);

	miBoolean nonCcgshader = miFALSE;
	if(state->type!=miRAY_EYE && state->parent){
		parentShaderTag = state->parent->shader->function_decl;
		mi_query( miQ_DECL_NAME, NULL, parentShaderTag, &parentDeclName);
		compareStr = (char *)mi_mem_allocate(sizeof(char)*(int(strlen(parentDeclName))+1));
		strcpy(compareStr,parentDeclName);
		pch = strstr(compareStr, "ccg_");
		if(!pch) nonCcgshader = miTRUE;
		mi_mem_release(compareStr);
	}

	if(state->type == miRAY_EYE)
	{
		state->user = &my;
		my.raystate = LAYER_RAY;
		my.scope = CCG_SCOPE_NONE;
	}else if(nonCcgshader){
				state->user = &my;
				my.scope = CCG_SCOPE_DISABLE_PASS;
			}
	mystate = (struct ccg_raystate *)state->user;

	passesOnce = *mi_eval_boolean(&paras->passesInOnce);

	whichlayer = *mi_eval_integer(&paras->layer);
	if((state->type != miRAY_EYE && state->type != miRAY_TRANSPARENT && state->type != miRAY_REFRACT) || state->reflection_level>0){
		whichlayer = LAYER_combined;
	}

	/* There are three cases when we should enter scope of (!passesOnce) only if passesOnce is enabled
		1. from a reflection or finalgather ray
		2. from any ray which casted by non ccg shader
		3. from any ray which casted in previous scope of (!passesOnce) */
	//1. if the ray comes from a reflection or finalgather
	if(state->type == miRAY_REFLECT || state->type == miRAY_FINALGATHER)
	{
		passesOnce = 0;
		whichlayer = LAYER_combined;
	}
	//2. if the ray comes from non_CcgShader
	if(nonCcgshader){
			passesOnce = 0;
			whichlayer = LAYER_combined;
	}
	//3. if the ray comes from previous scope of (!passesOnce)
	if(mystate->scope==CCG_SCOPE_DISABLE_PASS){
		passesOnce = 0;
	}

	mi_query(miQ_FUNC_USERPTR, state, 0, &fbarray);
	ccg_color_init(&tempColor,0);

	if(!passesOnce)
	{
		mystate->scope = CCG_SCOPE_DISABLE_PASS;

		//PASS: normal
		if(whichlayer==LAYER_nor) {
			tempColor.r = (state->normal.x + 1.0f)*0.5f;
			tempColor.g = (state->normal.y + 1.0f)*0.5f;
			tempColor.b = (state->normal.z + 1.0f)*0.5f;
			tempColor.a = 1;
			ccg_color_assign(result, &tempColor);
			return(miTRUE);
		}

		//PASS: depth
		if(whichlayer==LAYER_z) {
			zmin = *mi_eval_scalar(&paras->depthLimitMin);
			zmax = *mi_eval_scalar(&paras->depthLimitMax);
			if(zmin<0.0f) zmin = 0.0f;
			if(zmax<=zmin) zmax = zmin + 0.001f;
			z = (float(state->dist) - zmin)/(zmax - zmin);
			tempColor.r = tempColor.g = tempColor.b = 1.0f - ccg_clamp(z,0,1.0f);
			tempColor.a = 1;
			ccg_color_assign(result, &tempColor);
			return(miTRUE);
		}
		
		for(i=0;i<LAYER_NUM;i++)
			ccg_color_assign(&passes[i], &ccg_trans_black);

		color	= mi_eval_color(&paras->color);
		ccg_color_assign(&passes[LAYER_combined], color);
		matte	= mi_eval_color(&paras->matteOpacity);
		matte->a = matte->r * 0.3f + matte->g * 0.59f + matte->b * 0.11f;

		if(!ccg_color_compare(transp, &ccg_trans_black)){
			state->refraction_level--;
			if(!mi_trace_transparent(&tempColor, state)){
				tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
			}
			passes[LAYER_combined].r += transp->r * tempColor.r;
			passes[LAYER_combined].g += transp->g * tempColor.g;
			passes[LAYER_combined].b += transp->b * tempColor.b;
			passes[LAYER_combined].a =	matte->a + (1 - matte->a) * tempColor.a;
		}else passes[LAYER_combined].a = matte->a;

		ccg_color_assign(&passes[LAYER_col], &passes[LAYER_combined]);

	}else {
				/* we need prevent from calling our ccg* shader which caused by
				the ray comes from those of non-ccg* shaders */
				if(nonCcgshader)
				{
					result->r = result->g = result->b = result->a = 0.0;
					return(miTRUE);
				}

				mystate->scope = CCG_SCOPE_ENABLE_PASS;

				for(i=0;i<LAYER_NUM;i++)
					ccg_color_assign(&passes[i], &ccg_trans_black);

				mi::shader::Access_fb framebuffers(state->camera->buffertag);
				char buffer_name[LAYER_string_max_length];
				size_t buffer_index;

				//PASS: normal
				if(state->type == miRAY_EYE) {
					tempColor.r = (state->normal.x + 1.0f)*0.5f;
					tempColor.g = (state->normal.y + 1.0f)*0.5f;
					tempColor.b = (state->normal.z + 1.0f)*0.5f;
					tempColor.a = 1;
					ccg_color_assign(&passes[LAYER_nor], &tempColor);
					if((*fbarray)->passfbArray[LAYER_nor] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_nor), buffer_index))	
					{
						mi_fb_put(state, buffer_index, &tempColor);
					}
				}

				//PASS: depth
				if(state->type == miRAY_EYE) {
					zmin = *mi_eval_scalar(&paras->depthLimitMin);
					zmax = *mi_eval_scalar(&paras->depthLimitMax);
					if(zmin<0.0f) zmin = 0.0f;
					if(zmax<=zmin) zmax = zmin + 0.001f;
					z = (float(state->dist) - zmin)/(zmax - zmin);
					tempColor.r = tempColor.g = tempColor.b = 1.0f - ccg_clamp(z,0,1.0f);
					tempColor.a = 1;
					ccg_color_assign(&passes[LAYER_z], &tempColor);
					if((*fbarray)->passfbArray[LAYER_z] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_z), buffer_index))	
					{
						mi_fb_put(state, buffer_index, &tempColor);
					}
				}

				color	= mi_eval_color(&paras->color);
				//PASS: color
				if(mystate->raystate==LAYER_col || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_combined)
				{
					ccg_color_assign(&passes[LAYER_combined], color);
					matte	= mi_eval_color(&paras->matteOpacity);
					matte->a = matte->r * 0.3f + matte->g * 0.59f + matte->b * 0.11f;

					if(!ccg_color_compare(transp, &ccg_trans_black)){
						int tmpInt = mystate->raystate;
						mystate->raystate = LAYER_combined;
						state->refraction_level--;
						if(!mi_trace_transparent(&tempColor, state)){
							tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						}
						passes[LAYER_combined].r += transp->r * tempColor.r;
						passes[LAYER_combined].g += transp->g * tempColor.g;
						passes[LAYER_combined].b += transp->b * tempColor.b;
						passes[LAYER_combined].a =	matte->a + (1 - matte->a) * tempColor.a;
						if(state->type==miRAY_EYE)	mystate->raystate = LAYER_RAY;
						else mystate->raystate = tmpInt;
					} else passes[LAYER_combined].a = matte->a;
					ccg_color_assign(&passes[LAYER_col], &passes[LAYER_combined]);

					if(state->type==miRAY_EYE && (*fbarray)->passfbArray[LAYER_col] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_col), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_col]);
					}else if(mystate->raystate==LAYER_col)
						{
							ccg_color_assign(result, &passes[LAYER_col]);
							return(miTRUE);
						}
				}

				//PASS: diffuse
				if(mystate->raystate==LAYER_withoutShadow ||mystate->raystate==LAYER_RAY)
				{
					if(ccg_color_compare(color, &ccg_trans_black))
						ccg_color_init(&passes[LAYER_diff], 0);
					else ccg_color_init(&passes[LAYER_diff], 1);

					if(state->type==miRAY_EYE && (*fbarray)->passfbArray[LAYER_diff] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_diff), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_diff]);
					}else if(mystate->raystate==LAYER_withoutShadow)
						{
							ccg_color_assign(result, &passes[LAYER_diff]);
							return(miTRUE);
						}
				}

				//PASS: shadow
				if(mystate->raystate==LAYER_diff || mystate->raystate==LAYER_RAY)
				{
					if(ccg_color_compare(color, &ccg_trans_black))
						ccg_color_init(&passes[LAYER_shad], 0);
					else ccg_color_init(&passes[LAYER_shad], 1);

					if(state->type==miRAY_EYE && (*fbarray)->passfbArray[LAYER_shad] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_shad), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_shad]);
					}else if(mystate->raystate==LAYER_diff)
						{
							ccg_color_assign(result, &passes[LAYER_shad]);
							return(miTRUE);
						}
				}

				//PASS: combined
				if(state->type==miRAY_EYE && (*fbarray)->passfbArray[LAYER_combined] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_combined), buffer_index))
				{
					mi_fb_put(state, buffer_index, &passes[LAYER_combined]);
				}

				ccg_color_assign(result, &passes[LAYER_combined]);
				return(miTRUE);
		  }

	result->r = passes[whichlayer].r;
	result->g = passes[whichlayer].g;
	result->b = passes[whichlayer].b;
	result->a = passes[whichlayer].a;
  
	return(miTRUE);
}
