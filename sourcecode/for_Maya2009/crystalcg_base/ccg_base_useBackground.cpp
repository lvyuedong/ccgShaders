#include <stdlib.h>
#include <string.h>
#include "shader.h"
#include "geoshader.h"
#include "mayaapi.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#pragma warning(disable : 4267)

struct ccg_base_useBackground {
	miColor		specularColor;
	miScalar	reflectivity;
	miScalar	shadowMask;
	miBoolean	traceObject;
	miInteger	matteOpacityMode;
	miScalar	matteOpacity;
	miInteger	ibl_enable;
	miColor		bent;
	miInteger	bent_space;
	miInteger	ibl_shadow_mode;
	miScalar	min_dist;
	miScalar	max_dist;
	miColor		ibl_bright;
	miColor		ibl_dark;
	miBoolean	ibl_emit_diffuse;
	miScalar	ibl_angle;
	miScalar	depthLimitMin;
	miScalar	depthLimitMax;
	miInteger	layer;
	miBoolean	passesInOnce;
	miTag		fbWriteString;
	int			mode;			/* light mode: 0..2 */
	int			i_light;		/* index of first light */
	int			n_light;		/* number of lights */
	miTag		light[1];		/* list of lights */
};

extern "C" DLLEXPORT void ccg_base_useBackground_init(      /* init shader */
    miState         *state,
    struct ccg_base_useBackground *paras,
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
				if(!fbc) mi_error("Please set the fbWriteString parameter for ccg_base_blinn shader.");
				else {
						char *fbstr = mi_mem_strdup((char*)mi_db_access(fbc));
						mi_db_unpin( fbc );
						ccg_pass_string( fbstr, *fbarray);
						mi_mem_release(fbstr);
					}
			}

			//image based lighting
			if(*mi_eval_integer(&paras->ibl_enable)==2)
			{
				int n,j;
				int init = 1;
				int gLightNum,iblLightNum;
				mi_query(miQ_NUM_GLOBAL_LIGHTS, NULL, miNULLTAG, &gLightNum);
				miTag *iblTag;
				mi_query(miQ_GLOBAL_LIGHTS, NULL, miNULLTAG, &iblTag);
				for (n=0, j=0, iblLightNum=0; n < gLightNum; n++, iblTag++)
				{
					miTag realLight;
					mi_query( miQ_INST_ITEM,NULL,*iblTag,&realLight );
					char *lightname = (char *)mi_mem_strdup(mi_api_tag_lookup(realLight));
					char *tmp;
					tmp = strstr(lightname, "ccgLightArray");
					if(tmp!=NULL)
					{
						if(init)
						{
							int lightType;
							mi_query(miQ_LIGHT_TYPE, NULL, realLight, &lightType);
							(*fbarray)->light_type = lightType;
							char *tok;
							tok = strtok(lightname,"_");
							while (strcmp(tok, "ccgLightArray")!=0)
							{
								tok = strtok (NULL, "_");
							}
							tok = strtok (NULL, "_");
							tok = strtok (NULL, "_");
							iblLightNum = atoi(tok);
							(*fbarray)->lightTag = (miTag *)mi_mem_allocate(sizeof(miTag)*iblLightNum);
							init = 0;
						}
						*(((*fbarray)->lightTag)+j) = *iblTag;
						j++;
					}
					mi_mem_release(lightname);	
				}
				(*fbarray)->number = j;
				float angle = *mi_eval_scalar(&paras->ibl_angle);
				angle /= 2.0f;
				if(angle<0) angle = 0;
				else if(angle>=90) angle = 89.999999f;
				angle = cos(angle/90.0f*float(CCG_PI_2));
				(*fbarray)->angle = angle;
			}
		 }
}

extern "C" DLLEXPORT void ccg_base_useBackground_exit(      /* exit shader */
	miState         *state,
	struct ccg_base_useBackground *paras)
{
	if(paras)
	{
		struct ccg_passfbArray **fbarray;
		mi_query(miQ_FUNC_USERPTR, state, 0, &fbarray);
		if((*fbarray)->lightTag!=miNULLTAG) mi_mem_release((*fbarray)->lightTag);
		mi_mem_release(*fbarray);
	}
}

extern "C" DLLEXPORT int ccg_base_useBackground_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_base_useBackground(
  miColor   *result,
  miState   *state,
  struct ccg_base_useBackground *paras)
{
	miColor		*specular;
	miScalar	*reflective, *shadow_mask, *matte_opacity;
	miInteger	*matteMode;
	int			whichlayer;
	miColor		tempColor, lightColor, c_shadow, c_withoutShadow;
	//copyright: miColor		copyright;
	miColor		passes[LAYER_NUM];
	miScalar	zmin, zmax, z;    /* limitation of depth */
	int			i;
	miBoolean	passesOnce;
	struct ccg_passfbArray **fbarray;
	struct ccg_raystate *mystate, my;
	miTag	*light, *orig_light;
	miScalar	dot_nl;
	miVector	dir;
	int		n_l, i_l, m, n, samples;
	const miOptions *orig_option;
	miOptions	option_copy;
	const miState	*orig_state;
	miState	state_copy;
	miBoolean	tObj;

	miTag	parentShaderTag;
	char	*parentDeclName, *compareStr, *pch;

	miColor	ccg_solid_black = {0,0,0,1};
	miColor	ccg_trans_black = {0,0,0,0};

	if (state->type == miRAY_SHADOW)
	{
		result->r = result->g = result->b = result->a = 0;
		return(miFALSE);
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

		//PASS: shadow
		if(whichlayer==LAYER_combined)
		{
			m     = *mi_eval_integer(&paras->mode);
			n_l   = *mi_eval_integer(&paras->n_light);
			i_l   = *mi_eval_integer(&paras->i_light);
			light =  mi_eval_tag(paras->light) + i_l;
			if (m == 1)
				mi_inclusive_lightlist(&n_l, &light, state);
			else if (m == 2)
				mi_exclusive_lightlist(&n_l, &light, state);
			else if(m == 4){
				n_l = 0;
				light = 0;
			}

			ccg_color_init(&c_shadow, 0);
			ccg_color_init(&c_withoutShadow, 0);

			orig_light = light;
			if (m == 4 || n_l){
				for (mi::shader::LightIterator iter(state, light, n_l);!iter.at_end(); ++iter)
				{
					ccg_color_init(&tempColor, 0);
					while (iter->sample())
					{
						iter->get_contribution(&lightColor);
						dot_nl = iter->get_dot_nl();
						tempColor.r += dot_nl * lightColor.r;
						tempColor.g += dot_nl * lightColor.g;
						tempColor.b += dot_nl * lightColor.b;
					}
					samples = iter->get_number_of_samples();
					if (samples) {
						c_shadow.r += tempColor.r / samples;
						c_shadow.g += tempColor.g / samples;
						c_shadow.b += tempColor.b / samples;
					}
				}
			}
			
			light = orig_light;
			orig_option = state->options;
			option_copy = *orig_option;
			option_copy.shadow = 0;
			state->options = &option_copy;
			if (m == 4 || n_l){
				for (mi::shader::LightIterator iter(state, light, n_l);!iter.at_end(); ++iter)
				{
					ccg_color_init(&tempColor, 0);
					while (iter->sample())
					{
						iter->get_contribution(&lightColor);
						dot_nl = iter->get_dot_nl();
						tempColor.r += dot_nl * lightColor.r;
						tempColor.g += dot_nl * lightColor.g;
						tempColor.b += dot_nl * lightColor.b;
					}
					samples = iter->get_number_of_samples();
					if(samples){
						c_withoutShadow.r += tempColor.r / samples;
						c_withoutShadow.g += tempColor.g / samples;
						c_withoutShadow.b += tempColor.b / samples;
					}
				}
			}
			state->options = orig_option;

			//start image based lighting
			int ibl_en		=	*mi_eval_integer(&paras->ibl_enable);
			miBoolean ibl_emit_diff =	*mi_eval_boolean(&paras->ibl_emit_diffuse);
			miInteger	shadow_mode	=	*mi_eval_integer(&paras->ibl_shadow_mode);
			if(ibl_en==3 && ibl_emit_diff && shadow_mode!=2)
			{
				orig_state = state;
				state_copy = *orig_state;
				state = &state_copy;

				miColor		bent_normal =	*mi_eval_color(&paras->bent);
				float		min			=	*mi_eval_scalar(&paras->min_dist);
				float		max			=	*mi_eval_scalar(&paras->max_dist);
				miColor		bright		=	*mi_eval_color(&paras->ibl_bright);
				miColor		dark		=	*mi_eval_color(&paras->ibl_dark);
				miScalar	occ_alpha	=	bent_normal.a;
				miVector	bent_vector, bent_i;

				ccg_colormap2Vector(&bent_normal, &bent_vector);
				ccg_bent_space_conversion(state, *mi_eval_integer(&paras->bent_space), &bent_i, &bent_vector);

				double self;
				self = state->shadow_tol;
				mi_ray_offset(state, &self);
				float factor;
				if(shadow_mode==0)
				{
					if(max!=min && max>min) factor = (occ_alpha-min)/(max-min);
					else factor = 1;
					if(factor<0) factor = 0;
					if(factor>1) factor = 1;
				}else if(shadow_mode==1){
						if(mi_trace_probe(state, &bent_i, &state->point))
						{
							if(max!=min && max>min) factor = (float(state->child->dist)-min)/(max-min);
							else factor = 1;
							if(factor<0) factor = 0;
							if(factor>1) factor = 1;
						}else factor = 1;
					  }

				if(mi_trace_environment(&lightColor, state, &bent_i)) {
					dot_nl = mi_vector_dot(&state->normal, &bent_i);
					if(dot_nl>0)
					{
						miColor f;
						f.r = dark.r + factor*(bright.r - dark.r);
						f.g = dark.g + factor*(bright.g - dark.g);
						f.b = dark.b + factor*(bright.b - dark.b);
						f.a = 0;
						ccg_color_init(&tempColor, 0);
						tempColor.r = dot_nl * lightColor.r;
						tempColor.g = dot_nl * lightColor.g;
						tempColor.b = dot_nl * lightColor.b;
						ccg_color_add(&tempColor, &c_withoutShadow, &c_withoutShadow);
						ccg_color_multiply(&tempColor, &f, &tempColor);
						ccg_color_add(&tempColor, &c_shadow, &c_shadow);
					}
				}
				state = (miState*)orig_state;
			}else if(ibl_en==2)
				{
					if((*fbarray)->number>0 && (*fbarray)->angle<1.0f)
					{
						miTag *iblTag = (*fbarray)->lightTag;
						int n_ibl = (*fbarray)->number;
						miTag realLight;
						miVector lightDir;
						miVector lightPos;
						int rightPosition = 0;
						for (n=0; n < n_ibl; n++, iblTag++){
							mi_query(miQ_INST_ITEM, NULL, *iblTag, &realLight );
							mi_query(miQ_LIGHT_DIRECTION, state, realLight, &lightDir);
							mi_vector_neg(&lightDir);
							if((*fbarray)->light_type==2) {
								mi_query(miQ_LIGHT_ORIGIN, state, realLight, &lightPos);
								rightPosition = ccg_on_plane(&state->normal, &state->point, &lightPos);
							}else rightPosition = 1;
							if(rightPosition && mi_vector_dot(&state->normal, &lightDir) >= (*fbarray)->angle)
							{
								ccg_color_init(&tempColor, 0);
								samples = 0;
								while (mi_sample_light(&lightColor, &dir, &dot_nl, state, *iblTag, &samples)) {
										tempColor.r += dot_nl * lightColor.r;
										tempColor.g += dot_nl * lightColor.g;
										tempColor.b += dot_nl * lightColor.b;
								}
								if (samples){
									tempColor.r = tempColor.r / samples;
									tempColor.g = tempColor.g / samples;
									tempColor.b = tempColor.b / samples;
									ccg_color_add(&c_shadow, &tempColor, &c_shadow);
								}
							}
						}

						orig_option = state->options;
						option_copy = *orig_option;
						option_copy.shadow = 0;
						state->options = &option_copy;
						iblTag = (*fbarray)->lightTag;
						for (n=0; n < n_ibl; n++, iblTag++){
							mi_query(miQ_INST_ITEM, NULL, *iblTag, &realLight );
							mi_query(miQ_LIGHT_DIRECTION, state, realLight, &lightDir);
							mi_vector_neg(&lightDir);
							if((*fbarray)->light_type==2) {
								mi_query(miQ_LIGHT_ORIGIN, state, realLight, &lightPos);
								rightPosition = ccg_on_plane(&state->normal, &state->point, &lightPos);
							}else rightPosition = 1;
							if(rightPosition && mi_vector_dot(&state->normal, &lightDir) >= (*fbarray)->angle)
							{
								ccg_color_init(&tempColor, 0);
								samples = 0;
								while (mi_sample_light(&lightColor, &dir, &dot_nl, state, *iblTag, &samples)) {
										tempColor.r += dot_nl * lightColor.r;
										tempColor.g += dot_nl * lightColor.g;
										tempColor.b += dot_nl * lightColor.b;
								}
								if (samples){
									tempColor.r = tempColor.r / samples;
									tempColor.g = tempColor.g / samples;
									tempColor.b = tempColor.b / samples;
									ccg_color_add(&c_withoutShadow, &tempColor, &c_withoutShadow);
								}
							}
						}
						state->options = orig_option;
					}
				}

			if(c_shadow.r!=c_withoutShadow.r||c_shadow.g!=c_withoutShadow.g||c_shadow.b!=c_withoutShadow.b)
			{
				ccg_color_divide(&c_shadow, &c_withoutShadow, &passes[LAYER_shad]);
				shadow_mask	= mi_eval_scalar(&paras->shadowMask);
				passes[LAYER_shad].a = *shadow_mask;
			}else {
						ccg_color_init(&passes[LAYER_shad], 1);
						passes[LAYER_shad].a = 0;
					}
		}

		//PASS: color
		if(whichlayer==LAYER_col || whichlayer==LAYER_combined)
		{
			tObj = *mi_eval_boolean(&paras->traceObject);
			if(tObj)
			{
				if(!mi_trace_continue(&passes[LAYER_col],state))
					mi_trace_environment(&passes[LAYER_col], state, &state->dir);
			} else mi_trace_environment(&passes[LAYER_col], state, &state->dir);
			if(whichlayer==LAYER_col)
			{
				ccg_color_assign(result, &passes[LAYER_col]);
				return(miTRUE);
			}
		}

		//PASS: reflection
		if(whichlayer==LAYER_refl || whichlayer==LAYER_combined)
		{
			reflective	=	mi_eval_scalar(&paras->reflectivity);
			specular	=	mi_eval_color(&paras->specularColor);
			if(*reflective!=0.0f && !ccg_color_compare(specular, &ccg_solid_black))
			{
				mi_reflection_dir(&dir, state);
				if(mi_trace_reflection(&tempColor, state, &dir))
				{
					passes[LAYER_refl].r = tempColor.r * specular->r * (*reflective);
					passes[LAYER_refl].g = tempColor.g * specular->g * (*reflective);
					passes[LAYER_refl].b = tempColor.b * specular->b * (*reflective);
				}
			}
			if(whichlayer==LAYER_refl)
			{
				ccg_color_assign(result, &passes[LAYER_refl]);
				return(miTRUE);
			}
		}

		//PASS: combined
		ccg_color_multiply(&passes[LAYER_shad], &passes[LAYER_col], &passes[LAYER_combined]);
		ccg_color_add(&passes[LAYER_combined], &passes[LAYER_refl], &passes[LAYER_combined]);

		matteMode		=	mi_eval_integer(&paras->matteOpacityMode);
		matte_opacity	=	mi_eval_scalar(&paras->matteOpacity);
		if(*matteMode==0) passes[LAYER_combined].a = *matte_opacity;
		else if(*matteMode==1) passes[LAYER_combined].a = passes[LAYER_shad].a * (*matte_opacity);
		
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

				//PASS: diffuse
				if(mystate->raystate==LAYER_withoutShadow || mystate->raystate==LAYER_RAY)
				{
					ccg_color_init(&passes[LAYER_diff], 1);
					if(state->type==miRAY_EYE && (*fbarray)->passfbArray[LAYER_diff] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_diff), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_diff]);
					}else if(mystate->raystate==LAYER_withoutShadow)
						{
							ccg_color_assign(result, &passes[LAYER_diff]);
							return(miTRUE);
						}
				}

				matteMode		=	mi_eval_integer(&paras->matteOpacityMode);
				matte_opacity	=	mi_eval_scalar(&paras->matteOpacity);

				//PASS: shadow
				if(mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_combined || mystate->raystate==LAYER_diff)
				{
					m     = *mi_eval_integer(&paras->mode);
					n_l   = *mi_eval_integer(&paras->n_light);
					i_l   = *mi_eval_integer(&paras->i_light);
					light =  mi_eval_tag(paras->light) + i_l;
					if (m == 1)
						mi_inclusive_lightlist(&n_l, &light, state);
					else if (m == 2)
						mi_exclusive_lightlist(&n_l, &light, state);
					else if(m == 4){
						n_l = 0;
						light = 0;
					}

					ccg_color_init(&c_shadow, 0);
					ccg_color_init(&c_withoutShadow, 0);

					orig_light = light;
					if (m == 4 || n_l) {
						for (mi::shader::LightIterator iter(state, light, n_l);!iter.at_end(); ++iter)
						{
							ccg_color_init(&tempColor, 0);
							while (iter->sample())
							{
								iter->get_contribution(&lightColor);
								dot_nl = iter->get_dot_nl();
								tempColor.r += dot_nl * lightColor.r;
								tempColor.g += dot_nl * lightColor.g;
								tempColor.b += dot_nl * lightColor.b;
							}
							samples = iter->get_number_of_samples();
							if (samples) {
								c_shadow.r += tempColor.r / samples;
								c_shadow.g += tempColor.g / samples;
								c_shadow.b += tempColor.b / samples;
							}
						}
					}
					
					light = orig_light;
					orig_option = state->options;
					option_copy = *orig_option;
					option_copy.shadow = 0;
					state->options = &option_copy;
					if (m == 4 || n_l) {
						for (mi::shader::LightIterator iter(state, light, n_l);!iter.at_end(); ++iter)
						{
							ccg_color_init(&tempColor, 0);
							while (iter->sample())
							{
								iter->get_contribution(&lightColor);
								dot_nl = iter->get_dot_nl();
								tempColor.r += dot_nl * lightColor.r;
								tempColor.g += dot_nl * lightColor.g;
								tempColor.b += dot_nl * lightColor.b;
							}
							samples = iter->get_number_of_samples();
							if(samples){
								c_withoutShadow.r += tempColor.r / samples;
								c_withoutShadow.g += tempColor.g / samples;
								c_withoutShadow.b += tempColor.b / samples;
							}
						}
					}
					state->options = orig_option;

					//start image based lighting
					int ibl_en		=	*mi_eval_integer(&paras->ibl_enable);
					miBoolean ibl_emit_diff =	*mi_eval_boolean(&paras->ibl_emit_diffuse);
					miInteger	shadow_mode	=	*mi_eval_integer(&paras->ibl_shadow_mode);
					if(ibl_en==3 && ibl_emit_diff && shadow_mode!=2)
					{
						orig_state = state;
						state_copy = *orig_state;
						state = &state_copy;

						miColor		bent_normal =	*mi_eval_color(&paras->bent);
						float		min			=	*mi_eval_scalar(&paras->min_dist);
						float		max			=	*mi_eval_scalar(&paras->max_dist);
						miColor		bright		=	*mi_eval_color(&paras->ibl_bright);
						miColor		dark		=	*mi_eval_color(&paras->ibl_dark);
						miScalar	occ_alpha	=	bent_normal.a;
						miVector	bent_vector, bent_i;

						ccg_colormap2Vector(&bent_normal, &bent_vector);
						ccg_bent_space_conversion(state, *mi_eval_integer(&paras->bent_space), &bent_i, &bent_vector);

						double self;
						self = state->shadow_tol;
						mi_ray_offset(state, &self);
						float factor;
						if(shadow_mode==0)
						{
							if(max!=min && max>min) factor = (occ_alpha-min)/(max-min);
							else factor = 1;
							if(factor<0) factor = 0;
							if(factor>1) factor = 1;
						}else if(shadow_mode==1){
								if(mi_trace_probe(state, &bent_i, &state->point))
								{
									if(max!=min && max>min) factor = (float(state->child->dist)-min)/(max-min);
									else factor = 1;
									if(factor<0) factor = 0;
									if(factor>1) factor = 1;
								}else factor = 1;
							  }

						if(mi_trace_environment(&lightColor, state, &bent_i)) {
							dot_nl = mi_vector_dot(&state->normal, &bent_i);
							if(dot_nl>0)
							{
								miColor f;
								f.r = dark.r + factor*(bright.r - dark.r);
								f.g = dark.g + factor*(bright.g - dark.g);
								f.b = dark.b + factor*(bright.b - dark.b);
								f.a = 0;
								ccg_color_init(&tempColor, 0);
								tempColor.r = dot_nl * lightColor.r;
								tempColor.g = dot_nl * lightColor.g;
								tempColor.b = dot_nl * lightColor.b;
								ccg_color_add(&tempColor, &c_withoutShadow, &c_withoutShadow);
								ccg_color_multiply(&tempColor, &f, &tempColor);
								ccg_color_add(&tempColor, &c_shadow, &c_shadow);
							}
						}
						state = (miState*)orig_state;
					}else if(ibl_en==2)
						{
							if((*fbarray)->number>0 && (*fbarray)->angle<1.0f)
							{
								miTag *iblTag = (*fbarray)->lightTag;
								int n_ibl = (*fbarray)->number;
								miTag realLight;
								miVector lightDir;
								miVector lightPos;
								int rightPosition = 0;
								for (n=0; n < n_ibl; n++, iblTag++){
									mi_query(miQ_INST_ITEM, NULL, *iblTag, &realLight );
									mi_query(miQ_LIGHT_DIRECTION, state, realLight, &lightDir);
									mi_vector_neg(&lightDir);
									if((*fbarray)->light_type==2) {
										mi_query(miQ_LIGHT_ORIGIN, state, realLight, &lightPos);
										rightPosition = ccg_on_plane(&state->normal, &state->point, &lightPos);
									}else rightPosition = 1;
									if(rightPosition && mi_vector_dot(&state->normal, &lightDir) >= (*fbarray)->angle)
									{
										ccg_color_init(&tempColor, 0);
										samples = 0;
										while (mi_sample_light(&lightColor, &dir, &dot_nl, state, *iblTag, &samples)) {
												tempColor.r += dot_nl * lightColor.r;
												tempColor.g += dot_nl * lightColor.g;
												tempColor.b += dot_nl * lightColor.b;
										}
										if (samples){
											tempColor.r = tempColor.r / samples;
											tempColor.g = tempColor.g / samples;
											tempColor.b = tempColor.b / samples;
											ccg_color_add(&c_shadow, &tempColor, &c_shadow);
										}
									}
								}

								orig_option = state->options;
								option_copy = *orig_option;
								option_copy.shadow = 0;
								state->options = &option_copy;
								iblTag = (*fbarray)->lightTag;
								for (n=0; n < n_ibl; n++, iblTag++){
									mi_query(miQ_INST_ITEM, NULL, *iblTag, &realLight );
									mi_query(miQ_LIGHT_DIRECTION, state, realLight, &lightDir);
									mi_vector_neg(&lightDir);
									if((*fbarray)->light_type==2) {
										mi_query(miQ_LIGHT_ORIGIN, state, realLight, &lightPos);
										rightPosition = ccg_on_plane(&state->normal, &state->point, &lightPos);
									}else rightPosition = 1;
									if(rightPosition && mi_vector_dot(&state->normal, &lightDir) >= (*fbarray)->angle)
									{
										ccg_color_init(&tempColor, 0);
										samples = 0;
										while (mi_sample_light(&lightColor, &dir, &dot_nl, state, *iblTag, &samples)) {
												tempColor.r += dot_nl * lightColor.r;
												tempColor.g += dot_nl * lightColor.g;
												tempColor.b += dot_nl * lightColor.b;
										}
										if (samples){
											tempColor.r = tempColor.r / samples;
											tempColor.g = tempColor.g / samples;
											tempColor.b = tempColor.b / samples;
											ccg_color_add(&c_withoutShadow, &tempColor, &c_withoutShadow);
										}
									}
								}
								state->options = orig_option;
							}
						}

					if(c_shadow.r!=c_withoutShadow.r||c_shadow.g!=c_withoutShadow.g||c_shadow.b!=c_withoutShadow.b)
					{
						ccg_color_divide(&c_shadow, &c_withoutShadow, &passes[LAYER_shad]);
						shadow_mask	= mi_eval_scalar(&paras->shadowMask);
						passes[LAYER_shad].a = *shadow_mask;
					}else {
								ccg_color_init(&passes[LAYER_shad], 1);
								passes[LAYER_shad].a = 0;
							}

					if(*matteMode==0) passes[LAYER_shad].a = *matte_opacity;
					else if(*matteMode==1) passes[LAYER_shad].a = passes[LAYER_shad].a * (*matte_opacity);

					if(state->type==miRAY_EYE && (*fbarray)->passfbArray[LAYER_shad] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_shad), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_shad]);
					}else if(mystate->raystate==LAYER_diff)
						{
							ccg_color_assign(result, &passes[LAYER_shad]);
							return(miTRUE);
						}
				}

				//PASS: color
				if(mystate->raystate==LAYER_col || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_combined)
				{
					tObj = *mi_eval_boolean(&paras->traceObject);
					if(tObj)
					{
						if(!mi_trace_continue(&passes[LAYER_col],state))
							mi_trace_environment(&passes[LAYER_col], state, &state->dir);
					} else mi_trace_environment(&passes[LAYER_col], state, &state->dir);
					if((*fbarray)->passfbArray[LAYER_col] && state->type==miRAY_EYE && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_col), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_col]);
					}else if(mystate->raystate==LAYER_col)
						{
							ccg_color_assign(result, &passes[LAYER_col]);
							return(miTRUE);
						}
				}

				//PASS: reflection
				if(mystate->raystate==LAYER_refl || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_combined)
				{
					reflective	=	mi_eval_scalar(&paras->reflectivity);
					specular	=	mi_eval_color(&paras->specularColor);
					if(*reflective!=0.0f && !ccg_color_compare(specular, &ccg_solid_black))
					{
						int tmpInt = mystate->raystate;
						mi_reflection_dir(&dir, state);
						mystate->raystate = LAYER_combined;
						if(mi_trace_reflection(&tempColor, state, &dir))
						{
							passes[LAYER_refl].r = tempColor.r * specular->r * (*reflective);
							passes[LAYER_refl].g = tempColor.g * specular->g * (*reflective);
							passes[LAYER_refl].b = tempColor.b * specular->b * (*reflective);
						}
						if(state->type==miRAY_EYE)	mystate->raystate = LAYER_RAY;
						else mystate->raystate = tmpInt;
					}
					if(mystate->raystate==LAYER_refl)
					{
						ccg_color_assign(result, &passes[LAYER_refl]);
						return(miTRUE);
					}
					if((*fbarray)->passfbArray[LAYER_refl] && state->type==miRAY_EYE && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_refl), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_refl]);
					}
				}

				//PASS: combined
				ccg_color_multiply(&passes[LAYER_shad], &passes[LAYER_col], &passes[LAYER_combined]);
				ccg_color_add(&passes[LAYER_combined], &passes[LAYER_refl], &passes[LAYER_combined]);
				passes[LAYER_combined].a = passes[LAYER_shad].a;

				if((*fbarray)->passfbArray[LAYER_combined] && state->type==miRAY_EYE && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_combined), buffer_index))
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
