
#include <math.h>
#include <string.h>
#include "shader.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#pragma warning(disable : 4267)

struct ccg_base_template {
	miInteger	blendMode;
	miColor		color;
	miColor		overallColor;
	miColor		transparency;
	miColor		ambient;
	miColor		incandescence;
	miTag		diffuse;
	miVector	normalMapping;
	miTag		specular;
	miColor		reflectivity;
	miTag		reflection;
	miScalar	refractiveIndex;
	miColor		refractedColor;
	miTag		refraction;
	miBoolean	useRefractionShader;
	miColor		sss_multiply;
	miTag		sss_front;
	miTag		sss_middle;
	miTag		sss_back;
	miColor		translucencyMultiply;
	miTag		translucency;
	miColor		globalIllumMultiply;
	miTag		globalIllum;
	miBoolean	useGlobalIllumShader;
	miColor		ambientOcclusion;
	miColor		reflectOcclusion;
	miBoolean	add_to_combined;
	miScalar	depthLimitMin;
	miScalar	depthLimitMax;
	miInteger	layer;
	miBoolean	enableTransPasses;
	miBoolean	passesInOnce;
	miBoolean	diffuseOpacity;
	miBoolean	optimal;
	miTag		fbWriteString;
};

extern "C" DLLEXPORT void ccg_base_template_init(      /* init shader */
    miState         *state,
    struct ccg_base_template *paras,
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
				if(!fbc) mi_error("Please set the fbWriteString parameter for ccg_base_template shader.");
				else {
						char *fbstr = mi_mem_strdup((char*)mi_db_access(fbc));
						mi_db_unpin( fbc );
						ccg_pass_string( fbstr, *fbarray);
						mi_mem_release(fbstr);
					}
			}
		 }
}

extern "C" DLLEXPORT void ccg_base_template_exit(      /* exit shader */
	miState         *state,
	struct ccg_base_template *paras)
{
	if(paras)
	{
		struct ccg_passfbArray **fbarray;
		mi_query(miQ_FUNC_USERPTR, state, 0, &fbarray);
		mi_mem_release(*fbarray);
	}
}


extern "C" DLLEXPORT int ccg_base_template_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_base_template(
  miColor   *result,
  miState   *state,
  struct ccg_base_template *paras)
{
	miColor		*color, *transp, *incan, *refracted, *overall, *sss_m;
	miColor		itransp, ambi;
	miScalar	refracti;
	int			whichlayer;
	miColor		tempColor;
	//copyright: miColor	copyright;
	miColor		passes[LAYER_NUM], passesCombined[LAYER_NUM];
	miScalar	zmin,zmax,z;    /* limitation of depth */
	miBoolean	enableTransPass, passesOnce, amb_add, diffOpacity;
	int			i;
	miVector	refr_dir, normalbend;
	struct ccg_passfbArray **fbarray;
	//float	***fresnelLUT;
	struct ccg_raystate *mystate, my;
	const miOptions *orig_option;
	miOptions option_copy;
	miColor	withShadow, withoutShadow;
	//const miState	*orig_state;
	//miState	state_copy;
	//char	shadow_state;

	miTag	parentShaderTag;
	char	*parentDeclName, *compareStr, *pch;

	miColor	ccg_solid_black = {0,0,0,1};
	miColor	ccg_trans_black = {0,0,0,0};

	transp	= ccg_mi_eval_color(state, &paras->transparency);
	itransp.r = 1 - transp->r;
	itransp.g = 1 - transp->g;
	itransp.b = 1 - transp->b;
	itransp.a = itransp.r * 0.3f + itransp.g * 0.59f + itransp.b * 0.11f;
	transp->a = 1 - itransp.a;

	if (state->type == miRAY_SHADOW)
	{
		if(state->options->shadow=='s')
  		{
			if(itransp.a!=1.0)
			{
				ccg_shadow_choose_volume(state);
				mi_trace_shadow_seg(result, state);
				result->r *= transp->r;
				result->g *= transp->g;
  				result->b *= transp->b;
				result->a *= transp->a;
				return(miTRUE);
			}else {
					result->r = result->g = result->b = result->a = 1;
					return(miFALSE);
				  }
		}
		if(itransp.a!=1.0)
  		{
			result->r *= transp->r;
			result->g *= transp->g;
  			result->b *= transp->b;
			result->a *= transp->a;
  			return(miTRUE);
  		}else {
				result->r = result->g = result->b = result->a = 1;
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
	enableTransPass = *mi_eval_boolean(&paras->enableTransPasses);
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
	//mi_query(miQ_FUNC_USERPTR, state, 0, &fresnelLUT);
	ccg_color_init(&tempColor, 0);
	ccg_color_init(&withoutShadow, 0);
	ccg_color_init(&withShadow, 0);

	/*	retrieve bumpMapping value which never need to be used, to bend normal before 
		any other calculation of illumination model, since there is no way to perform MR's shader list in Maya	*/
	normalbend = *mi_eval_vector(&paras->normalMapping);

	if(!passesOnce)
	{
		mystate->scope = CCG_SCOPE_DISABLE_PASS;

		if(whichlayer==LAYER_refr && (state->type==miRAY_REFRACT || state->type==miRAY_TRANSPARENT))
			whichlayer = LAYER_combined;

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
			if(zmin<0.0) zmin = 0.0;
			if(zmax<=zmin) zmax = zmin + 0.001f;
			z = (float(state->dist) - zmin)/(zmax - zmin);
			tempColor.r = tempColor.g = tempColor.b = 1.0f - ccg_clamp(z,0,1.0f);
			tempColor.a = 1;
			ccg_color_assign(result, &tempColor);
			return(miTRUE);
		}

		for(i=0;i<LAYER_NUM;i++){
			ccg_color_assign(&passes[i], &ccg_trans_black);
		}

		overall	= ccg_mi_eval_color(state, &paras->overallColor);
		sss_m	= mi_eval_color(&paras->sss_multiply);
		color	= ccg_mi_eval_color(state, &paras->color);
		ccg_color_assign(&passes[LAYER_col],color);
		ccg_color_multiply(&passes[LAYER_col], overall, &passes[LAYER_col]);
		ambi	=  *ccg_mi_eval_color(state, &paras->ambient);
		ccg_color_assign(&passes[LAYER_ambi],&ambi);
		incan	= ccg_mi_eval_color(state, &paras->incandescence);
		ccg_color_assign(&passes[LAYER_incan],incan);
		refracti = *mi_eval_scalar(&paras->refractiveIndex);
		refracted = ccg_mi_eval_color(state, &paras->refractedColor);

		//PASS: ambient
		if(whichlayer==LAYER_ambi)
		{
			ccg_color_assign(result, &passes[LAYER_ambi]);
			return(miTRUE);
		}

		//PASS: color
		if(whichlayer==LAYER_col)
		{
			if(itransp.a!=1.0){
				if(enableTransPass){
					if(refracti==1.0)
					{
						state->refraction_level--;
						if(!mi_trace_transparent(&tempColor, state)){
							tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						}
					}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
							mi_refraction_dir(&refr_dir, state, 1, refracti);
							if(!mi_trace_refraction(&tempColor, state, &refr_dir))
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							ccg_color_multiply(refracted, &tempColor, &tempColor);
						  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					passes[LAYER_col].r = itransp.r * passes[LAYER_col].r + transp->r * tempColor.r;
					passes[LAYER_col].g = itransp.g * passes[LAYER_col].g + transp->g * tempColor.g;
					passes[LAYER_col].b = itransp.b * passes[LAYER_col].b + transp->b * tempColor.b;
					passes[LAYER_col].a = itransp.a + transp->a * tempColor.a;
				}else {
						ccg_color_multiply(&passes[LAYER_col], &itransp, &passes[LAYER_col]);
						passes[LAYER_col].a = itransp.a;
					}
			} else passes[LAYER_col].a = 1;
			ccg_color_assign(result, &passes[LAYER_col]);
			return(miTRUE);
		}

		//PASS: incandescence
		if(whichlayer==LAYER_incan)
		{
			if(itransp.a!=1.0){
				if(enableTransPass){
					if(refracti==1.0)
					{
						state->refraction_level--;
						if(!mi_trace_transparent(&tempColor, state)){
							tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						}
					}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
							mi_refraction_dir(&refr_dir, state, 1, refracti);
							if(!mi_trace_refraction(&tempColor, state, &refr_dir))
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							ccg_color_multiply(refracted, &tempColor, &tempColor);
						  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					passes[LAYER_incan].r += transp->r * tempColor.r;
					passes[LAYER_incan].g += transp->g * tempColor.g;
					passes[LAYER_incan].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_incan]);
			return(miTRUE);
		}

		//PASS: subsurface scatering front
		if(whichlayer==LAYER_sssfront || whichlayer==LAYER_combined)
		{
			ccg_mi_call_shader(&passes[LAYER_sssfront], state, *mi_eval_tag(&paras->sss_front));
			ccg_color_multiply(&passes[LAYER_sssfront], overall, &passes[LAYER_sssfront]);
			ccg_color_multiply(&passes[LAYER_sssfront], sss_m, &passes[LAYER_sssfront]);
			if(whichlayer==LAYER_sssfront)
			{
				if(itransp.a!=1.0){
					if(enableTransPass){
						if(refracti==1.0)
						{
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
						}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
								mi_refraction_dir(&refr_dir, state, 1, refracti);
								if(!mi_trace_refraction(&tempColor, state, &refr_dir))
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								ccg_color_multiply(refracted, &tempColor, &tempColor);
							  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						passes[LAYER_sssfront].r = itransp.r * passes[LAYER_sssfront].r + transp->r * tempColor.r;
						passes[LAYER_sssfront].g = itransp.g * passes[LAYER_sssfront].g + transp->g * tempColor.g;
						passes[LAYER_sssfront].b = itransp.b * passes[LAYER_sssfront].b + transp->b * tempColor.b;
					}else {
							ccg_color_multiply(&passes[LAYER_sssfront], &itransp, &passes[LAYER_sssfront]);
							passes[LAYER_sssfront].a = itransp.a;
						}
				}
				ccg_color_assign(result, &passes[LAYER_sssfront]);
				return miTRUE;
			}
		}

		//PASS: subsurface scatering middle
		if(whichlayer==LAYER_sssmiddle || whichlayer==LAYER_combined)
		{
			ccg_mi_call_shader(&passes[LAYER_sssmiddle], state, *mi_eval_tag(&paras->sss_middle));
			ccg_color_multiply(&passes[LAYER_sssmiddle], overall, &passes[LAYER_sssmiddle]);
			ccg_color_multiply(&passes[LAYER_sssmiddle], sss_m, &passes[LAYER_sssmiddle]);
			if(whichlayer==LAYER_sssmiddle)
			{
				if(itransp.a!=1.0){
					if(enableTransPass){
						if(refracti==1.0)
						{
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
						}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
								mi_refraction_dir(&refr_dir, state, 1, refracti);
								if(!mi_trace_refraction(&tempColor, state, &refr_dir))
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								ccg_color_multiply(refracted, &tempColor, &tempColor);
							  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						passes[LAYER_sssmiddle].r += transp->r * tempColor.r;
						passes[LAYER_sssmiddle].g += transp->g * tempColor.g;
						passes[LAYER_sssmiddle].b += transp->b * tempColor.b;
					}else {
							ccg_color_multiply(&passes[LAYER_sssmiddle], &itransp, &passes[LAYER_sssmiddle]);
							passes[LAYER_sssmiddle].a = itransp.a;
						}
				}
				ccg_color_assign(result, &passes[LAYER_sssmiddle]);
				return miTRUE;
			}
		}

		//PASS: subsurface scatering back
		if(whichlayer==LAYER_sssback || whichlayer==LAYER_combined)
		{
			ccg_mi_call_shader(&passes[LAYER_sssback], state, *mi_eval_tag(&paras->sss_back));
			ccg_color_multiply(&passes[LAYER_sssback], overall, &passes[LAYER_sssback]);
			ccg_color_multiply(&passes[LAYER_sssback], sss_m, &passes[LAYER_sssback]);
			if(whichlayer==LAYER_sssback)
			{
				if(itransp.a!=1.0){
					if(enableTransPass){
						if(refracti==1.0)
						{
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
						}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
								mi_refraction_dir(&refr_dir, state, 1, refracti);
								if(!mi_trace_refraction(&tempColor, state, &refr_dir))
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								ccg_color_multiply(refracted, &tempColor, &tempColor);
							  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						passes[LAYER_sssback].r = itransp.r * passes[LAYER_sssback].r + transp->r * tempColor.r;
						passes[LAYER_sssback].g = itransp.g * passes[LAYER_sssback].g + transp->g * tempColor.g;
						passes[LAYER_sssback].b = itransp.b * passes[LAYER_sssback].b + transp->b * tempColor.b;
					}else {
							ccg_color_multiply(&passes[LAYER_sssback], &itransp, &passes[LAYER_sssback]);
							passes[LAYER_sssback].a = itransp.a;
						}
				}
				ccg_color_assign(result, &passes[LAYER_sssback]);
				return miTRUE;
			}
		}

		//diffuse and specular
		if(whichlayer==LAYER_combined || whichlayer==LAYER_diff || whichlayer==LAYER_spec){
			if (whichlayer==LAYER_combined || whichlayer==LAYER_diff)
			{
				ccg_mi_call_shader(&passes[LAYER_diff], state, *mi_eval_tag(&paras->diffuse));
			}
			if (whichlayer==LAYER_combined || whichlayer==LAYER_spec)
			{
				ccg_mi_call_shader(&passes[LAYER_spec], state, *mi_eval_tag(&paras->specular));
			}
		}

		//PASS: diffusion
		if(whichlayer==LAYER_diff)
		{
			diffOpacity = *mi_eval_boolean(&paras->diffuseOpacity);
			if(!diffOpacity)
			{
				if(itransp.a!=1.0){
					if(enableTransPass){
						if(refracti==1.0)
						{
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
						}else {
								mi_refraction_dir(&refr_dir, state, 1, refracti);
								if(!mi_trace_refraction(&tempColor, state, &refr_dir))
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							  }
						passes[LAYER_diff].r = itransp.r * passes[LAYER_diff].r + transp->r * tempColor.r;
						passes[LAYER_diff].g = itransp.g * passes[LAYER_diff].g + transp->g * tempColor.g;
						passes[LAYER_diff].b = itransp.b * passes[LAYER_diff].b + transp->b * tempColor.b;
						passes[LAYER_diff].a = itransp.a + transp->a * tempColor.a;
					}else passes[LAYER_diff].a = 1;
				} else passes[LAYER_diff].a = 1;
			}else passes[LAYER_diff].a = 1;
			ccg_color_assign(result, &passes[LAYER_diff]);
			return(miTRUE);
		}

		//PASS: specular
		if(whichlayer==LAYER_spec)
		{
			if(itransp.a!=1.0){
				if(enableTransPass){
					if(refracti==1.0)
					{
						state->refraction_level--;
						if(!mi_trace_transparent(&tempColor, state)){
							tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						}
					}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
							mi_refraction_dir(&refr_dir, state, 1, refracti);
							if(!mi_trace_refraction(&tempColor, state, &refr_dir))
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							ccg_color_multiply(refracted, &tempColor, &tempColor);
						  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					passes[LAYER_spec].r += transp->r * tempColor.r;
					passes[LAYER_spec].g += transp->g * tempColor.g;
					passes[LAYER_spec].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_spec]);
			return(miTRUE);
		}

		//PASS: reflection
		if(whichlayer==LAYER_refl || whichlayer==LAYER_combined)
		{
			miColor *reflecti =  ccg_mi_eval_color(state, &paras->reflectivity);
			if (!ccg_color_compare(reflecti, &ccg_solid_black))
			{
				ccg_mi_call_shader(&passes[LAYER_refl], state, *mi_eval_tag(&paras->reflection));
				ccg_color_multiply(&passes[LAYER_refl], reflecti, &passes[LAYER_refl]);
			}
			if(whichlayer==LAYER_refl)
			{
				if(itransp.a!=1.0){
					if(enableTransPass){
						if(refracti==1.0)
						{
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
						}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
								mi_refraction_dir(&refr_dir, state, 1, refracti);
								if(!mi_trace_refraction(&tempColor, state, &refr_dir))
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								ccg_color_multiply(refracted, &tempColor, &tempColor);
							  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						passes[LAYER_refl].r += transp->r * tempColor.r;
						passes[LAYER_refl].g += transp->g * tempColor.g;
						passes[LAYER_refl].b += transp->b * tempColor.b;
					}
				}
				ccg_color_assign(result, &passes[LAYER_refl]);
				return(miTRUE);
			}
		}

		//PASS: refraction
		miBoolean useRefraShader = *mi_eval_boolean(&paras->useRefractionShader);
		if(whichlayer==LAYER_refr || whichlayer==LAYER_combined)
		{
			if(useRefraShader){
				ccg_mi_call_shader(&passes[LAYER_refr], state, *mi_eval_tag(&paras->refraction));
				ccg_color_multiply(refracted, &passes[LAYER_refr], &passes[LAYER_refr]);
			}else if(itransp.a!=1.0){
						if(refracti==1.0)
						{
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
						}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
								mi_refraction_dir(&refr_dir, state, 1, refracti);
								if(!mi_trace_refraction(&tempColor, state, &refr_dir))
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								ccg_color_multiply(refracted, &tempColor, &tempColor);
							  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						passes[LAYER_refr].r = transp->r * tempColor.r;
						passes[LAYER_refr].g = transp->g * tempColor.g;
						passes[LAYER_refr].b = transp->b * tempColor.b;
						passes[LAYER_refr].a = transp->a * tempColor.a;
					}
			if(whichlayer==LAYER_refr){
				ccg_color_assign(result, &passes[LAYER_refr]);
				return(miTRUE);
			}
		}

		//PASS: translucency
		if(whichlayer==LAYER_translucent || whichlayer==LAYER_combined){
			tempColor = *mi_eval_color(&paras->translucencyMultiply);
			ccg_mi_call_shader(&passes[LAYER_translucent], state, *mi_eval_tag(&paras->translucency));
			ccg_color_multiply(&passes[LAYER_translucent], &tempColor, &passes[LAYER_translucent]);
			if(whichlayer==LAYER_translucent){
				if(itransp.a!=1.0){
					if(enableTransPass){
						if(refracti==1.0)
						{
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
						}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
								mi_refraction_dir(&refr_dir, state, 1, refracti);
								if(!mi_trace_refraction(&tempColor, state, &refr_dir))
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								ccg_color_multiply(refracted, &tempColor, &tempColor);
							  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						passes[LAYER_translucent].r += transp->r * tempColor.r;
						passes[LAYER_translucent].g += transp->g * tempColor.g;
						passes[LAYER_translucent].b += transp->b * tempColor.b;
					}
				}
				ccg_color_assign(result, &passes[LAYER_translucent]);
				return(miTRUE);
			}
		}

		//PASS: global illumination
		if(whichlayer==LAYER_globillum || whichlayer==LAYER_combined){
			miBoolean useGlobalShader = *mi_eval_boolean(&paras->useGlobalIllumShader);
			if(useGlobalShader){
				tempColor = *mi_eval_color(&paras->globalIllumMultiply);
				ccg_mi_call_shader(&passes[LAYER_globillum], state, *mi_eval_tag(&paras->globalIllum));
				ccg_color_multiply(&passes[LAYER_globillum], &tempColor, &passes[LAYER_globillum]);
			}else {
					mi_compute_avg_radiance(&passes[LAYER_globillum], state, 'f', NULL);
					tempColor = *mi_eval_color(&paras->globalIllumMultiply);
					passes[LAYER_globillum].r *= tempColor.r * color->r * itransp.r;
					passes[LAYER_globillum].g *= tempColor.g * color->g * itransp.g;
					passes[LAYER_globillum].b *= tempColor.b * color->b * itransp.b;
				}
			if(whichlayer==LAYER_globillum){
				ccg_color_assign(result, &passes[LAYER_globillum]);
				return(miTRUE);
			}
		}

		//PASS: combined
		passes[LAYER_combined].r = passes[LAYER_col].r * (passes[LAYER_diff].r + passes[LAYER_ambi].r);
		passes[LAYER_combined].g = passes[LAYER_col].g * (passes[LAYER_diff].g + passes[LAYER_ambi].g);
		passes[LAYER_combined].b = passes[LAYER_col].b * (passes[LAYER_diff].b + passes[LAYER_ambi].b);

		//PASS: ambient occlusion
		amb_add	= *mi_eval_boolean(&paras->add_to_combined);
		if(whichlayer == LAYER_ao || (amb_add && whichlayer == LAYER_combined))
		{
			passes[LAYER_ao] = *ccg_mi_eval_color(state, &paras->ambientOcclusion);
			if(amb_add)	ccg_color_multiply(&passes[LAYER_combined], &passes[LAYER_ao], &passes[LAYER_combined]);
			if(whichlayer==LAYER_ao){
				ccg_color_assign(result, &passes[LAYER_ao]);
				return(miTRUE);
			}
		}

		//PASS: reflection occlusion
		if(whichlayer == LAYER_reflao || (amb_add && whichlayer == LAYER_combined))
		{
			passes[LAYER_reflao] = *ccg_mi_eval_color(state, &paras->reflectOcclusion);
			if(amb_add)	ccg_color_multiply(&passes[LAYER_refl], &passes[LAYER_reflao], &passes[LAYER_refl]);
			if(whichlayer==LAYER_reflao){
				ccg_color_assign(result, &passes[LAYER_reflao]);
				return(miTRUE);
			}
		}

		miInteger *blend_mode = mi_eval_integer(&paras->blendMode);
		switch(*blend_mode)
		{
			case 0:	//r
					passes[LAYER_combined].r += passes[LAYER_sssfront].r + passes[LAYER_sssmiddle].r + passes[LAYER_sssback].r;
					passes[LAYER_combined].r *= itransp.r;
					passes[LAYER_combined].r += passes[LAYER_incan].r + passes[LAYER_globillum].r + passes[LAYER_spec].r + passes[LAYER_refl].r + passes[LAYER_refr].r + passes[LAYER_translucent].r;
					//g
					passes[LAYER_combined].g += passes[LAYER_sssfront].g + passes[LAYER_sssmiddle].g + passes[LAYER_sssback].g;
					passes[LAYER_combined].g *= itransp.g;
					passes[LAYER_combined].g += passes[LAYER_incan].g + passes[LAYER_globillum].g + passes[LAYER_spec].g + passes[LAYER_refl].g + passes[LAYER_refr].g + passes[LAYER_translucent].g;
					//b
					passes[LAYER_combined].b += passes[LAYER_sssfront].b + passes[LAYER_sssmiddle].b + passes[LAYER_sssback].b;
					passes[LAYER_combined].b *= itransp.b;
					passes[LAYER_combined].b += passes[LAYER_incan].b + passes[LAYER_globillum].b + passes[LAYER_spec].b + passes[LAYER_refl].b + passes[LAYER_refr].b + passes[LAYER_translucent].b;
					break;
			case 1: ccg_screen_comp(&passes[LAYER_combined], &passes[LAYER_sssfront], &passes[LAYER_combined]);
					ccg_screen_comp(&passes[LAYER_combined], &passes[LAYER_sssmiddle], &passes[LAYER_combined]);
					ccg_screen_comp(&passes[LAYER_combined], &passes[LAYER_sssback], &passes[LAYER_combined]);
					ccg_color_multiply(&passes[LAYER_combined], &itransp, &passes[LAYER_combined]);
					ccg_screen_comp(&passes[LAYER_combined], &passes[LAYER_incan], &passes[LAYER_combined]);
					ccg_screen_comp(&passes[LAYER_combined], &passes[LAYER_globillum], &passes[LAYER_combined]);
					ccg_screen_comp(&passes[LAYER_combined], &passes[LAYER_spec], &passes[LAYER_combined]);
					ccg_screen_comp(&passes[LAYER_combined], &passes[LAYER_refl], &passes[LAYER_combined]);
					ccg_screen_comp(&passes[LAYER_combined], &passes[LAYER_translucent], &passes[LAYER_combined]);
					ccg_color_add(&passes[LAYER_combined], &passes[LAYER_refr], &passes[LAYER_combined]);
					break;
		}
		passes[LAYER_combined].a = itransp.a + passes[LAYER_refr].a;

	}else {
				/* we need prevent from calling our ccg* shader which caused by
				the ray comes from those of non-ccg* shaders */
				if(nonCcgshader)
				{
					result->r = result->g = result->b = result->a = 0.0;
					return(miTRUE);
				}

				mystate->scope = CCG_SCOPE_ENABLE_PASS;

				for(i=0;i<LAYER_NUM;i++){
					ccg_color_assign(&passes[i], &ccg_trans_black);
					ccg_color_assign(&passesCombined[i], &ccg_trans_black);
				}

				mi::shader::Access_fb framebuffers(state->camera->buffertag);
				char buffer_name[LAYER_string_max_length];
				size_t buffer_index;

				//PASS: normal
				if(state->type == miRAY_EYE)
				{	
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
				if(state->type==miRAY_EYE)
				{
					zmin = *mi_eval_scalar(&paras->depthLimitMin);
					zmax = *mi_eval_scalar(&paras->depthLimitMax);
					if(zmin<0.0) zmin = 0.0;
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

				overall	= ccg_mi_eval_color(state, &paras->overallColor);
				sss_m	= mi_eval_color(&paras->sss_multiply);
				color	= ccg_mi_eval_color(state, &paras->color);
				ccg_color_assign(&passesCombined[LAYER_col],color);
				ccg_color_multiply(&passesCombined[LAYER_col], overall, &passesCombined[LAYER_col]);
				ccg_color_assign(&passes[LAYER_col], &passesCombined[LAYER_col]);
				ambi	=  *ccg_mi_eval_color(state, &paras->ambient);
				ccg_color_assign(&passesCombined[LAYER_ambi],&ambi);
				ccg_color_assign(&passes[LAYER_ambi],&ambi);
				incan	= ccg_mi_eval_color(state, &paras->incandescence);
				ccg_color_assign(&passesCombined[LAYER_incan],incan);
				ccg_color_assign(&passes[LAYER_incan],incan);
				refracti	=	*mi_eval_scalar(&paras->refractiveIndex);
				refracted	=	ccg_mi_eval_color(state, &paras->refractedColor);

				//PASS: ambient
				if(state->type==miRAY_EYE)
				{
					if((*fbarray)->passfbArray[LAYER_ambi] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_ambi), buffer_index))	
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_ambi]);
					}
				}

				//PASS: color
				if(mystate->raystate==LAYER_col || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0)
					{
						if(enableTransPass){
							mystate->raystate = LAYER_col;
							if(refracti==1.0)
							{
								state->refraction_level--;
								if(!mi_trace_transparent(&tempColor, state)){
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								}
							}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
									mi_refraction_dir(&refr_dir, state, 1, refracti);
									if(!mi_trace_refraction(&tempColor, state, &refr_dir))
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									ccg_color_multiply(refracted, &tempColor, &tempColor);
								  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							passes[LAYER_col].r = itransp.r * passes[LAYER_col].r + transp->r * tempColor.r;
							passes[LAYER_col].g = itransp.g * passes[LAYER_col].g + transp->g * tempColor.g;
							passes[LAYER_col].b = itransp.b * passes[LAYER_col].b + transp->b * tempColor.b;
							passes[LAYER_col].a = itransp.a + transp->a * tempColor.a;
							if(state->type==miRAY_EYE)//state->user_size = LAYER_RAY;
								mystate->raystate = LAYER_RAY;
						}else {
								ccg_color_multiply(&passes[LAYER_col], &itransp, &passes[LAYER_col]);
								passes[LAYER_col].a = itransp.a;
							}
					} else passes[LAYER_col].a = 1;
					if(state->type==miRAY_EYE)
					{
						if((*fbarray)->passfbArray[LAYER_col] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_col), buffer_index))
						{
							mi_fb_put(state, buffer_index, &passes[LAYER_col]);
						}
					}
					else{
							ccg_color_assign(result, &passes[LAYER_col]);
							return(miTRUE);
						}
				}

				//PASS: incandescence
				if(mystate->raystate==LAYER_incan || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0){
						if(enableTransPass){
							mystate->raystate = LAYER_incan;
							if(refracti==1.0)
							{
								state->refraction_level--;
								if(!mi_trace_transparent(&tempColor, state)){
									tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								}
							}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
									mi_refraction_dir(&refr_dir, state, 1, refracti);
									if(!mi_trace_refraction(&tempColor, state, &refr_dir))
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									ccg_color_multiply(refracted, &tempColor, &tempColor);
								  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							passes[LAYER_incan].r += transp->r * tempColor.r;
							passes[LAYER_incan].g += transp->g * tempColor.g;
							passes[LAYER_incan].b += transp->b * tempColor.b;
							if(state->type==miRAY_EYE)//state->user_size = LAYER_RAY;
								mystate->raystate = LAYER_RAY;
						}
					}
					if(state->type==miRAY_EYE)
					{
						if((*fbarray)->passfbArray[LAYER_incan] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_incan), buffer_index))
						{
							mi_fb_put(state, buffer_index, &passes[LAYER_incan]);
						}
					}
					else{ 
							ccg_color_assign(result, &passes[LAYER_incan]);
							return(miTRUE);
						}
				}

				//PASS: subsurface scatering front
				if(mystate->raystate==LAYER_sssfront || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_combined)
				{
					ccg_mi_call_shader(&passes[LAYER_sssfront], state, *mi_eval_tag(&paras->sss_front));
					ccg_color_multiply(&passes[LAYER_sssfront], overall, &passes[LAYER_sssfront]);
					ccg_color_multiply(&passes[LAYER_sssfront], sss_m, &passes[LAYER_sssfront]);
					ccg_color_assign(&passesCombined[LAYER_sssfront], &passes[LAYER_sssfront]);

					if(mystate->raystate==LAYER_sssfront || mystate->raystate==LAYER_RAY)
					{
						if(itransp.a!=1.0){
							if(enableTransPass){
								mystate->raystate = LAYER_sssfront;
								if(refracti==1.0)
								{
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state)){
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
								}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
										mi_refraction_dir(&refr_dir, state, 1, refracti);
										if(!mi_trace_refraction(&tempColor, state, &refr_dir))
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										ccg_color_multiply(refracted, &tempColor, &tempColor);
									  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								passes[LAYER_sssfront].r = passes[LAYER_sssfront].r * itransp.r + transp->r * tempColor.r;
								passes[LAYER_sssfront].g = passes[LAYER_sssfront].g * itransp.g + transp->g * tempColor.g;
								passes[LAYER_sssfront].b = passes[LAYER_sssfront].b * itransp.b + transp->b * tempColor.b;
								if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
							}else {
									ccg_color_multiply(&passes[LAYER_sssfront], &itransp, &passes[LAYER_sssfront]);
									passes[LAYER_sssfront].a = itransp.a;
								}
						}
						if(state->type==miRAY_EYE)
						{
							if((*fbarray)->passfbArray[LAYER_sssfront] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_sssfront), buffer_index))
							{
								mi_fb_put(state, buffer_index, &passes[LAYER_sssfront]);
							}
						}
						else{ 
								ccg_color_assign(result, &passes[LAYER_sssfront]);
								return(miTRUE);
							}
					}
				}

				//PASS: subsurface scatering middle
				if(mystate->raystate==LAYER_sssmiddle || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_combined)
				{
					ccg_mi_call_shader(&passes[LAYER_sssmiddle], state, *mi_eval_tag(&paras->sss_middle));
					ccg_color_multiply(&passes[LAYER_sssmiddle], overall, &passes[LAYER_sssmiddle]);
					ccg_color_multiply(&passes[LAYER_sssmiddle], sss_m, &passes[LAYER_sssmiddle]);
					ccg_color_assign(&passesCombined[LAYER_sssmiddle], &passes[LAYER_sssmiddle]);

					if(mystate->raystate==LAYER_sssmiddle || mystate->raystate==LAYER_RAY)
					{
						if(itransp.a!=1.0){
							if(enableTransPass){
								mystate->raystate = LAYER_sssmiddle;
								if(refracti==1.0)
								{
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state)){
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
								}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
										mi_refraction_dir(&refr_dir, state, 1, refracti);
										if(!mi_trace_refraction(&tempColor, state, &refr_dir))
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										ccg_color_multiply(refracted, &tempColor, &tempColor);
									  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								passes[LAYER_sssmiddle].r = passes[LAYER_sssmiddle].r * itransp.r + transp->r * tempColor.r;
								passes[LAYER_sssmiddle].g = passes[LAYER_sssmiddle].g * itransp.g + transp->g * tempColor.g;
								passes[LAYER_sssmiddle].b = passes[LAYER_sssmiddle].b * itransp.b + transp->b * tempColor.b;
								if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
							}else {
									ccg_color_multiply(&passes[LAYER_sssmiddle], &itransp, &passes[LAYER_sssmiddle]);
									passes[LAYER_sssmiddle].a = itransp.a;
								}
						}
						if(state->type==miRAY_EYE)
						{
							if((*fbarray)->passfbArray[LAYER_sssmiddle] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_sssmiddle), buffer_index))
							{
								mi_fb_put(state, buffer_index, &passes[LAYER_sssmiddle]);
							}
						}
						else{ 
								ccg_color_assign(result, &passes[LAYER_sssmiddle]);
								return(miTRUE);
							}
					}
				}

				//PASS: subsurface scatering back
				if(mystate->raystate==LAYER_sssback || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_combined)
				{
					ccg_mi_call_shader(&passes[LAYER_sssback], state, *mi_eval_tag(&paras->sss_back));
					ccg_color_multiply(&passes[LAYER_sssback], overall, &passes[LAYER_sssback]);
					ccg_color_multiply(&passes[LAYER_sssback], sss_m, &passes[LAYER_sssback]);
					ccg_color_assign(&passesCombined[LAYER_sssback], &passes[LAYER_sssback]);

					if(mystate->raystate==LAYER_sssback || mystate->raystate==LAYER_RAY)
					{
						if(itransp.a!=1.0){
							if(enableTransPass){
								mystate->raystate = LAYER_sssback;
								if(refracti==1.0)
								{
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state)){
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
								}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
										mi_refraction_dir(&refr_dir, state, 1, refracti);
										if(!mi_trace_refraction(&tempColor, state, &refr_dir))
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										ccg_color_multiply(refracted, &tempColor, &tempColor);
									  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								passes[LAYER_sssback].r = passes[LAYER_sssback].r * itransp.r + transp->r * tempColor.r;
								passes[LAYER_sssback].g = passes[LAYER_sssback].g * itransp.g + transp->g * tempColor.g;
								passes[LAYER_sssback].b = passes[LAYER_sssback].b * itransp.b + transp->b * tempColor.b;
								if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
							}else {
									ccg_color_multiply(&passes[LAYER_sssback], &itransp, &passes[LAYER_sssback]);
									passes[LAYER_sssback].a = itransp.a;
								}
						}
						if(state->type==miRAY_EYE)
						{
							if((*fbarray)->passfbArray[LAYER_sssback] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_sssback), buffer_index))
							{
								mi_fb_put(state, buffer_index, &passes[LAYER_sssback]);
							}
						}
						else{ 
								ccg_color_assign(result, &passes[LAYER_sssback]);
								return(miTRUE);
							}
					}
				}


				//shadow_state = state->options->shadow;
				if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_diff || mystate->raystate==LAYER_spec || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_withoutShadow)
				{
					miBoolean optim = *mi_eval_boolean(&paras->optimal);
					/*	without shadow *****************************************/
					if(mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_withoutShadow)
					{
							orig_option = state->options;
							option_copy = *orig_option;
							option_copy.shadow = 0;
							if(optim){
								option_copy.trace = miFALSE;
								option_copy.caustic = miFALSE;
								option_copy.globillum = miFALSE;
							}
							state->options = &option_copy;
							ccg_mi_call_shader(&passes[LAYER_diff], state, *mi_eval_tag(&paras->diffuse));
							ccg_color_assign(&withoutShadow, &passes[LAYER_diff]);
							state->options = (miOptions*)orig_option;
					}

					/*	with shadow *****************************************/
					if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_diff || mystate->raystate==LAYER_spec)
					{
							/*orig_option = state->options;
							option_copy = *orig_option;
							option_copy.shadow = shadow_state;
							state->options = &option_copy;*/
							if(mystate->raystate==LAYER_combined||mystate->raystate==LAYER_diff || mystate->raystate==LAYER_RAY)
							{
								ccg_mi_call_shader(&passesCombined[LAYER_diff], state, *mi_eval_tag(&paras->diffuse));
								ccg_color_assign(&withShadow, &passesCombined[LAYER_diff]);
							}
							if(mystate->raystate==LAYER_combined||mystate->raystate==LAYER_spec||mystate->raystate==LAYER_RAY)
							{
								ccg_mi_call_shader(&passesCombined[LAYER_spec], state, *mi_eval_tag(&paras->specular));
								ccg_color_assign(&passes[LAYER_spec], &passesCombined[LAYER_spec]);
							}
							/*state->options = (miOptions*)orig_option;*/
					}
				}

				//PASS: diffusion
				if(mystate->raystate==LAYER_diff || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_withoutShadow)
				{
					diffOpacity = *mi_eval_boolean(&paras->diffuseOpacity);
					if(!diffOpacity)
					{
						if(itransp.a!=1.0){
							if(enableTransPass){
								//without shadow
								if(mystate->raystate==LAYER_withoutShadow || mystate->raystate==LAYER_RAY)
								{
									orig_option = state->options;
									option_copy = *orig_option;
									option_copy.shadow = 0;
									state->options = &option_copy;
									mystate->raystate = LAYER_withoutShadow;
									if(refracti==1.0)
									{
										state->refraction_level--;
										if(!mi_trace_transparent(&tempColor, state)){
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										}
									}else {
											mi_refraction_dir(&refr_dir, state, 1, refracti);
											if(!mi_trace_refraction(&tempColor, state, &refr_dir))
												tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										  }
									withoutShadow.r = itransp.r * withoutShadow.r + transp->r * tempColor.r;
									withoutShadow.g = itransp.g * withoutShadow.g + transp->g * tempColor.g;
									withoutShadow.b = itransp.b * withoutShadow.b + transp->b * tempColor.b;
									withoutShadow.a = itransp.a + transp->a * tempColor.a;
									ccg_color_assign(&passes[LAYER_diff], &withoutShadow);
									if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
									state->options = (miOptions*)orig_option;
								}
								//with shadow
								if(mystate->raystate==LAYER_diff || mystate->raystate==LAYER_RAY)
								{
									/*orig_option = state->options;
									option_copy = *orig_option;
									option_copy.shadow = shadow_state;
									state->options = &option_copy;*/
									mystate->raystate = LAYER_diff;
									if(refracti==1.0)
									{
										state->refraction_level--;
										if(!mi_trace_transparent(&tempColor, state)){
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										}
									}else {
											mi_refraction_dir(&refr_dir, state, 1, refracti);
											if(!mi_trace_refraction(&tempColor, state, &refr_dir))
												tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										  }
									withShadow.r = itransp.r * withShadow.r + transp->r * tempColor.r;
									withShadow.g = itransp.g * withShadow.g + transp->g * tempColor.g;
									withShadow.b = itransp.b * withShadow.b + transp->b * tempColor.b;
									withShadow.a = itransp.a + transp->a * tempColor.a;
									if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
									/*state->options = (miOptions*)orig_option;*/
								}
							}else {passes[LAYER_diff].a = 1; passesCombined[LAYER_diff].a = 1; withShadow.a = 1;}
						}else {passes[LAYER_diff].a = 1; passesCombined[LAYER_diff].a = 1; withShadow.a = 1;}
					}else {passes[LAYER_diff].a = 1; passesCombined[LAYER_diff].a = 1; withShadow.a = 1;}

					if(state->type==miRAY_EYE)
					{
						if((*fbarray)->passfbArray[LAYER_diff] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_diff), buffer_index))
						{
							mi_fb_put(state, buffer_index, &passes[LAYER_diff]);
						}
					}
					else if(!diffOpacity){ 
							if(mystate->raystate==LAYER_withoutShadow)
								ccg_color_assign(result, &passes[LAYER_diff]);
							if(mystate->raystate==LAYER_diff)
								ccg_color_assign(result, &withShadow);
							return(miTRUE);
						}
				}

				//PASS: shadow
				if(state->type==miRAY_EYE)
				{
					ccg_color_assign(&passes[LAYER_shad], &withShadow);
					if((*fbarray)->passfbArray[LAYER_shad] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_shad), buffer_index))	
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_shad]);
					}
				}

				//PASS: specular
				if(mystate->raystate==LAYER_spec || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0){
						if(enableTransPass){
							mystate->raystate = LAYER_spec;
							if(refracti==1.0)
								{
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state)){
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
								}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
										mi_refraction_dir(&refr_dir, state, 1, refracti);
										if(!mi_trace_refraction(&tempColor, state, &refr_dir))
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										ccg_color_multiply(refracted, &tempColor, &tempColor);
									  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							passes[LAYER_spec].r += transp->r * tempColor.r;
							passes[LAYER_spec].g += transp->g * tempColor.g;
							passes[LAYER_spec].b += transp->b * tempColor.b;
							if(state->type==miRAY_EYE)	//state->user_size = LAYER_RAY;
								mystate->raystate = LAYER_RAY;
						}
					}
					if(state->type==miRAY_EYE)
					{
						if((*fbarray)->passfbArray[LAYER_spec] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_spec), buffer_index))
						{
							mi_fb_put(state, buffer_index, &passes[LAYER_spec]);
						}
					}
					else{ 
							ccg_color_assign(result, &passes[LAYER_spec]);
							return(miTRUE);
						}
				}

				//PASS: reflection
				if(mystate->raystate==LAYER_refl || mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY)
				{
					miColor *reflecti =  ccg_mi_eval_color(state, &paras->reflectivity);
					if (!ccg_color_compare(reflecti, &ccg_solid_black))
					{
						int tmpInt = mystate->raystate;
						mystate->raystate = LAYER_combined;
						ccg_mi_call_shader(&passesCombined[LAYER_refl], state, *mi_eval_tag(&paras->reflection));
						if(state->type==miRAY_EYE)	mystate->raystate = LAYER_RAY;
						else mystate->raystate = tmpInt;
						ccg_color_multiply(&passesCombined[LAYER_refl], reflecti, &passesCombined[LAYER_refl]);
						ccg_color_assign(&passes[LAYER_refl], &passesCombined[LAYER_refl]);
					}

					if(mystate->raystate==LAYER_refl || mystate->raystate==LAYER_RAY)
					{
						if(itransp.a!=1.0)
						{
							if(enableTransPass){
								mystate->raystate = LAYER_refl;
								if(refracti==1.0)
								{
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state)){
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
								}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
										mi_refraction_dir(&refr_dir, state, 1, refracti);
										if(!mi_trace_refraction(&tempColor, state, &refr_dir))
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										ccg_color_multiply(refracted, &tempColor, &tempColor);
									  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								passes[LAYER_refl].r += transp->r * tempColor.r;
								passes[LAYER_refl].g += transp->g * tempColor.g;
								passes[LAYER_refl].b += transp->b * tempColor.b;
								if(state->type==miRAY_EYE)
									mystate->raystate = LAYER_RAY;
							}
						}
						if(state->type==miRAY_EYE)
						{
							if((*fbarray)->passfbArray[LAYER_refl] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_refl), buffer_index))
							{
								mi_fb_put(state, buffer_index, &passes[LAYER_refl]);
							}
						}
						else{
								ccg_color_assign(result, &passes[LAYER_refl]);
								return(miTRUE);
							}
					}
				}

				//PASS: refraction
				miBoolean useRefraShader = *mi_eval_boolean(&paras->useRefractionShader);
				if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY)
				{
					if(useRefraShader){
						ccg_mi_call_shader(&passesCombined[LAYER_refr], state, *mi_eval_tag(&paras->refraction));
						ccg_color_multiply(&passesCombined[LAYER_refr], refracted, &passesCombined[LAYER_refr]);
					}else if(itransp.a!=1.0f){
								//mystate->raystate = LAYER_combined;
								mystate->scope = CCG_SCOPE_DISABLE_PASS;
								if(refracti==1.0f)
								{
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state)){
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
								}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
										mi_refraction_dir(&refr_dir, state, 1, refracti);
										if(!mi_trace_refraction(&tempColor, state, &refr_dir))
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										ccg_color_multiply(refracted, &tempColor, &tempColor);
									  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								passesCombined[LAYER_refr].r = transp->r * tempColor.r;
								passesCombined[LAYER_refr].g = transp->g * tempColor.g;
								passesCombined[LAYER_refr].b = transp->b * tempColor.b;
								passesCombined[LAYER_refr].a = transp->a * tempColor.a;
								//if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
								mystate->scope = CCG_SCOPE_ENABLE_PASS;
							}
					ccg_color_assign(&passes[LAYER_refr], &passesCombined[LAYER_refr]);

					if(state->type==miRAY_EYE)
					{
						if((*fbarray)->passfbArray[LAYER_refr] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_refr), buffer_index))
						{
							mi_fb_put(state, buffer_index, &passes[LAYER_refr]);
						}
					}
				}

				//PASS: translucency
				if(mystate->raystate==LAYER_translucent || mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY)
				{
					tempColor = *mi_eval_color(&paras->translucencyMultiply);
					ccg_mi_call_shader(&passesCombined[LAYER_translucent], state, *mi_eval_tag(&paras->translucency));
					ccg_color_multiply(&passesCombined[LAYER_translucent], &tempColor, &passesCombined[LAYER_translucent]);
					ccg_color_assign(&passes[LAYER_translucent], &passesCombined[LAYER_translucent]);

					if(mystate->raystate==LAYER_translucent || mystate->raystate==LAYER_RAY)
					{
						if(itransp.a!=1.0){
							if(enableTransPass){
								mystate->raystate = LAYER_translucent;
								if(refracti==1.0)
								{
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state)){
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
								}else if(!ccg_color_compare(refracted, &ccg_trans_black)){
										mi_refraction_dir(&refr_dir, state, 1, refracti);
										if(!mi_trace_refraction(&tempColor, state, &refr_dir))
											tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
										ccg_color_multiply(refracted, &tempColor, &tempColor);
									  }else tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
								passes[LAYER_translucent].r += transp->r * tempColor.r;
								passes[LAYER_translucent].g += transp->g * tempColor.g;
								passes[LAYER_translucent].b += transp->b * tempColor.b;
								if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
							}
						}
						if(state->type==miRAY_EYE)
						{
							if((*fbarray)->passfbArray[LAYER_translucent] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_translucent), buffer_index))
							{
								mi_fb_put(state, buffer_index, &passes[LAYER_translucent]);
							}
						}
					}
				}

				//PASS: global illumination
				if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY){
					miBoolean useGlobalShader = *mi_eval_boolean(&paras->useGlobalIllumShader);
					tempColor = *mi_eval_color(&paras->globalIllumMultiply);
					if(useGlobalShader){
						ccg_mi_call_shader(&passesCombined[LAYER_globillum], state, *mi_eval_tag(&paras->globalIllum));
						ccg_color_multiply(&passesCombined[LAYER_globillum], &tempColor, &passesCombined[LAYER_globillum]);
					}else {
							mi_compute_avg_radiance(&passesCombined[LAYER_globillum], state, 'f', NULL);
							passesCombined[LAYER_globillum].r *= tempColor.r * color->r * itransp.r;
							passesCombined[LAYER_globillum].g *= tempColor.g * color->g * itransp.g;
							passesCombined[LAYER_globillum].b *= tempColor.b * color->b * itransp.b;
						}
					ccg_color_assign(&passes[LAYER_globillum], &passesCombined[LAYER_globillum]);
					if(state->type==miRAY_EYE)
					{
						if((*fbarray)->passfbArray[LAYER_globillum] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_globillum), buffer_index))	
						{
							mi_fb_put(state, buffer_index, &passes[LAYER_globillum]);
						}
					}
				}

				//PASS: combined
				passesCombined[LAYER_combined].r = passesCombined[LAYER_col].r * (passesCombined[LAYER_diff].r + passesCombined[LAYER_ambi].r);
				passesCombined[LAYER_combined].g = passesCombined[LAYER_col].g * (passesCombined[LAYER_diff].g + passesCombined[LAYER_ambi].g);
				passesCombined[LAYER_combined].b = passesCombined[LAYER_col].b * (passesCombined[LAYER_diff].b + passesCombined[LAYER_ambi].b);

				//PASS: ambient and reflect occlusion
				if(state->type == miRAY_EYE)
				{
					amb_add = *mi_eval_boolean(&paras->add_to_combined);
					passes[LAYER_ao] = *ccg_mi_eval_color(state, &paras->ambientOcclusion);
					if((*fbarray)->passfbArray[LAYER_ao] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_ao), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_ao]);
					}
					if(amb_add)	ccg_color_multiply(&passesCombined[LAYER_combined], &passes[LAYER_ao], &passesCombined[LAYER_combined]);

					passes[LAYER_reflao] = *ccg_mi_eval_color(state, &paras->reflectOcclusion);
					if((*fbarray)->passfbArray[LAYER_reflao] && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_reflao), buffer_index))
					{
						mi_fb_put(state, buffer_index, &passes[LAYER_reflao]);
					}
					if(amb_add)	ccg_color_multiply(&passesCombined[LAYER_refl], &passes[LAYER_reflao], &passesCombined[LAYER_refl]);
				}

				miInteger *blend_mode = mi_eval_integer(&paras->blendMode);
				switch(*blend_mode)
				{
					case 0:	//r
							passesCombined[LAYER_combined].r += passesCombined[LAYER_sssfront].r + passesCombined[LAYER_sssmiddle].r + passesCombined[LAYER_sssback].r;
							passesCombined[LAYER_combined].r *= itransp.r;
							passesCombined[LAYER_combined].r += passesCombined[LAYER_incan].r + passesCombined[LAYER_globillum].r + passesCombined[LAYER_spec].r + passesCombined[LAYER_refl].r + passesCombined[LAYER_refr].r + passesCombined[LAYER_translucent].r;
							//g
							passesCombined[LAYER_combined].g += passesCombined[LAYER_sssfront].g + passesCombined[LAYER_sssmiddle].g + passesCombined[LAYER_sssback].g;
							passesCombined[LAYER_combined].g *= itransp.g;
							passesCombined[LAYER_combined].g += passesCombined[LAYER_incan].g + passesCombined[LAYER_globillum].g + passesCombined[LAYER_spec].g + passesCombined[LAYER_refl].g + passesCombined[LAYER_refr].g + passesCombined[LAYER_translucent].g;
							//b
							passesCombined[LAYER_combined].b += passesCombined[LAYER_sssfront].b + passesCombined[LAYER_sssmiddle].b + passesCombined[LAYER_sssback].b;
							passesCombined[LAYER_combined].b *= itransp.b;
							passesCombined[LAYER_combined].b += passesCombined[LAYER_incan].b + passesCombined[LAYER_globillum].b + passesCombined[LAYER_spec].b + passesCombined[LAYER_refl].b + passesCombined[LAYER_refr].b + passesCombined[LAYER_translucent].b;
							break;
					case 1: ccg_screen_comp(&passesCombined[LAYER_combined], &passesCombined[LAYER_sssfront], &passesCombined[LAYER_combined]);
							ccg_screen_comp(&passesCombined[LAYER_combined], &passesCombined[LAYER_sssmiddle], &passesCombined[LAYER_combined]);
							ccg_screen_comp(&passesCombined[LAYER_combined], &passesCombined[LAYER_sssback], &passesCombined[LAYER_combined]);
							ccg_color_multiply(&passesCombined[LAYER_combined], &itransp, &passesCombined[LAYER_combined]);
							ccg_screen_comp(&passesCombined[LAYER_combined], &passesCombined[LAYER_incan], &passesCombined[LAYER_combined]);
							ccg_screen_comp(&passesCombined[LAYER_combined], &passesCombined[LAYER_globillum], &passesCombined[LAYER_combined]);
							ccg_screen_comp(&passesCombined[LAYER_combined], &passesCombined[LAYER_spec], &passesCombined[LAYER_combined]);
							ccg_screen_comp(&passesCombined[LAYER_combined], &passesCombined[LAYER_refl], &passesCombined[LAYER_combined]);
							ccg_screen_comp(&passesCombined[LAYER_combined], &passesCombined[LAYER_translucent], &passesCombined[LAYER_combined]);
							ccg_color_add(&passesCombined[LAYER_combined], &passesCombined[LAYER_refr], &passesCombined[LAYER_combined]);
							break;
				}
				passesCombined[LAYER_combined].a = itransp.a + passesCombined[LAYER_refr].a;
				ccg_color_assign(&passes[LAYER_combined], &passesCombined[LAYER_combined]);

				if((*fbarray)->passfbArray[LAYER_combined] && state->type==miRAY_EYE && framebuffers->get_index(ccg_get_pass_name(buffer_name,LAYER_combined), buffer_index))
				{
					mi_fb_put(state, buffer_index, &passes[LAYER_combined]);
				}

				result->r = passes[LAYER_combined].r;
				result->g = passes[LAYER_combined].g;
				result->b = passes[LAYER_combined].b;
				result->a = passes[LAYER_combined].a;
				
				return(miTRUE);
		  }

	result->r = passes[whichlayer].r;
	result->g = passes[whichlayer].g;
	result->b = passes[whichlayer].b;
	result->a = passes[whichlayer].a;
	
	return(miTRUE);
}

