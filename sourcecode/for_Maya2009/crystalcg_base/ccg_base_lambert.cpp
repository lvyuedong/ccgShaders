#include <stdlib.h>
#include <string.h>
#include "shader.h"
#include "geoshader.h"
#include "mayaapi.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#pragma warning(disable : 4267)

struct ccg_base_lambert {
	miColor		color;
	miColor		transparency;
	miColor		ambient;
	miColor		incandescence;
	miVector	normalMapping;
	miScalar	diffuse;
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
	miColor		ambientOcclusion;
	miColor		reflectOcclusion;
	miBoolean	add_to_combined;
	miScalar	depthLimitMin;
	miScalar	depthLimitMax;
	miInteger	layer;
	miBoolean	enableTransPasses;
	miBoolean	passesInOnce;
	miBoolean	diffuseOpacity;
	miTag		fbWriteString;
	miBoolean	disableShadowChain;
	int			mode;			/* light mode: 0..2 */
	int			i_light;		/* index of first light */
	int			n_light;		/* number of lights */
	miTag		light[1];		/* list of lights */
};

extern "C" DLLEXPORT void ccg_base_lambert_init(      /* init shader */
    miState         *state,
    struct ccg_base_lambert *paras,
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
				if(!fbc) mi_error("Please set the fbWriteString parameter for ccg_base_lambert shader.");
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

			//float ***fresnelLUT;
			//int fresnelMode = *mi_eval_integer(&paras->fresnel_mode);
			//if(fresnelMode==3)
			//{
			//	mi_query(miQ_FUNC_USERPTR, state, 0, &fresnelLUT);
			//	ccg_fresnel_LUT(fresnelLUT);
			//}
		 }
}

extern "C" DLLEXPORT void ccg_base_lambert_exit(      /* exit shader */
	miState         *state,
	struct ccg_base_lambert *paras)
{
	if(paras)
	{
		struct ccg_passfbArray **fbarray;
		mi_query(miQ_FUNC_USERPTR, state, 0, &fbarray);
		if((*fbarray)->lightTag!=miNULLTAG) mi_mem_release((*fbarray)->lightTag);
		mi_mem_release(*fbarray);

		//float ***fresnelLUT;
		//mi_query(miQ_FUNC_USERPTR, state, 0, &fresnelLUT);
		//mi_mem_release(**fresnelLUT);
	}
}

extern "C" DLLEXPORT int ccg_base_lambert_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_base_lambert(
  miColor   *result,
  miState   *state,
  struct ccg_base_lambert *paras)
{
	miColor		*color, *transp, *incan;
	miColor		itransp, ambi;
	miScalar	diff;   /* the relationship between ambi, color and diff is, color*(dot_nl*diff+ambi)*/
	miTag		*light, *again_light;   /* tag of light instance */
	int			n_l;    /* number of light sources */
	int			i_l;    /* offset of light sources */
	int			m;    /* light mode: 0=all, 1=incl, 2=excl */
	int			n;    /* light counter */
	int			samples;  /* # of samples taken */
	int			whichlayer;
	miColor		lightColor;   /* color from light source */
	miColor		sum;    /* summed sample colors; summed sample speculars */
	miVector	dir;		/* direction towards light or reflection or refraction */
	miScalar	dot_nl;   /* dot prod of normal and light direction*/
	miColor		tempColor;
	//copyright: miColor		copyright;
	miColor		passes[LAYER_NUM], passesCombined[LAYER_NUM];
	miScalar	zmin, zmax, z;    /* limitation of depth */
	int			i;
	miVector	normalbend;
	miBoolean	enableTransPass, passesOnce, diffOpacity;
	struct ccg_passfbArray **fbarray;
	struct ccg_raystate *mystate, my;
	const miOptions *orig_option;
	miOptions option_copy;
	miColor	withShadow, withoutShadow;
	const miState	*orig_state;
	miState	state_copy;
	//char	shadow_state;

	miTag	parentShaderTag;
	char	*parentDeclName, *compareStr, *pch;
	
	miBoolean	*mayaStateDiffuse = NULL, *mayaStateSpecular = NULL;
	miBoolean	emitDiffuse = miTRUE;		/* default emits diffuse */
	miBoolean	emitSpecular = miTRUE;	/* default emits specular */

	miColor	ccg_solid_black = {0,0,0,1};
	miColor	ccg_trans_black = {0,0,0,0};

	transp	= mi_eval_color(&paras->transparency);
	itransp.r = 1 - transp->r;
	itransp.g = 1 - transp->g;
	itransp.b = 1 - transp->b;
	itransp.a = itransp.r * 0.3f + itransp.g * 0.59f + itransp.b * 0.11f;
	transp->a = 1 - itransp.a;

	if (state->type == miRAY_SHADOW)
	{
		if(itransp.a!=1.0)
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
	ccg_color_init(&tempColor,0);
	ccg_color_init(&withoutShadow, 0);
	ccg_color_init(&withShadow, 0);
	//copyright: ccg_color_init(&copyright, 0);
	//copyright: ccg_illegal_copyright(state, &copyright);

	/*	retrieve bumpMapping value, which never need to be used, to bend normal before 
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

		for(i=0;i<LAYER_NUM;i++)
			ccg_color_assign(&passes[i], &ccg_trans_black);
		
		miColor		bent_normal;
		bent_normal.a = -1;	//-1 used to decide if alpha contain occlusion info
		ambi	=  *mi_eval_color(&paras->ambient);
		int ibl_en = *mi_eval_integer(&paras->ibl_enable);
		if(ibl_en==1){
			orig_state = state;
			state_copy = *orig_state;
			state = &state_copy;
			bent_normal = *mi_eval_color(&paras->bent);
			miVector	bent_vector, bent_i;
			ccg_colormap2Vector(&bent_normal, &bent_vector);
			ccg_bent_space_conversion(state, *mi_eval_integer(&paras->bent_space), &bent_i, &bent_vector);
			if(mi_trace_environment(&lightColor, state, &bent_i)) {
				ccg_color_add(&lightColor, &ambi, &ambi);
			}
			state = (miState*)orig_state;
		}
		ccg_color_assign(&passes[LAYER_ambi],&ambi);

		color	= mi_eval_color(&paras->color);
		ccg_color_assign(&passes[LAYER_col],color);
		incan	= mi_eval_color(&paras->incandescence);
		ccg_color_assign(&passes[LAYER_incan],incan);
		diff	=  *mi_eval_scalar(&paras->diffuse);
		m     = *mi_eval_integer(&paras->mode);
		n_l   = *mi_eval_integer(&paras->n_light);
		i_l   = *mi_eval_integer(&paras->i_light);
		light =  mi_eval_tag(paras->light) + i_l;

		//PASS: ambient
		if(whichlayer==LAYER_ambi)
		{
			ccg_color_assign(result, &passes[LAYER_ambi]);
			return(miTRUE);
		}

		//PASS: color
		if(whichlayer==LAYER_col){
			if(itransp.a!=1.0){
				if(enableTransPass){
					state->refraction_level--;
					if(!mi_trace_transparent(&tempColor, state)){
						tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					}
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
		if(whichlayer==LAYER_incan){
			if(itransp.a!=1.0){
				if(enableTransPass){
					state->refraction_level--;
					if(!mi_trace_transparent(&tempColor, state)){
						tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					}
					passes[LAYER_incan].r += transp->r * tempColor.r;
					passes[LAYER_incan].g += transp->g * tempColor.g;
					passes[LAYER_incan].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_incan]);
			return(miTRUE);
		}

		//PASS: subsurface scatering front
		if(whichlayer==LAYER_sssfront)
		{
			if(itransp.a!=1.0){
				if(enableTransPass){
					state->refraction_level--;
					if(!mi_trace_transparent(&tempColor, state)){
						tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					}
					passes[LAYER_sssfront].r += transp->r * tempColor.r;
					passes[LAYER_sssfront].g += transp->g * tempColor.g;
					passes[LAYER_sssfront].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_sssfront]);
			return(miTRUE);
		}

		//PASS: subsurface scatering middle
		if(whichlayer==LAYER_sssmiddle)
		{
			if(itransp.a!=1.0){
				if(enableTransPass){
					state->refraction_level--;
					if(!mi_trace_transparent(&tempColor, state)){
						tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					}
					passes[LAYER_sssmiddle].r += transp->r * tempColor.r;
					passes[LAYER_sssmiddle].g += transp->g * tempColor.g;
					passes[LAYER_sssmiddle].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_sssmiddle]);
			return(miTRUE);
		}

		//PASS: subsurface scatering back
		if(whichlayer==LAYER_sssback)
		{
			if(itransp.a!=1.0){
				if(enableTransPass){
					state->refraction_level--;
					if(!mi_trace_transparent(&tempColor, state)){
						tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					}
					passes[LAYER_sssback].r += transp->r * tempColor.r;
					passes[LAYER_sssback].g += transp->g * tempColor.g;
					passes[LAYER_sssback].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_sssback]);
			return(miTRUE);
		}

		if (m == 1)   /* modify light list (inclusive mode) */
			mi_inclusive_lightlist(&n_l, &light, state);
		else if (m == 2)  /* modify light list (exclusive mode) */
			mi_exclusive_lightlist(&n_l, &light, state);
		else if(m == 4){
			n_l = 0;
			light = 0;
		}

		/* Loop over all light sources */
		if(whichlayer==LAYER_combined || whichlayer==LAYER_diff){
			if (m == 4 || n_l){
				for (mi::shader::LightIterator iter(state, light, n_l);!iter.at_end(); ++iter) {
					ccg_color_init(&sum, 0);
					while (iter->sample()) {
						//get custom maya light properties
						if (mayabase_stateitem_get(state,
								MBSI_LIGHTDIFFUSE, &mayaStateDiffuse,
								MBSI_LIGHTSPECULAR, &mayaStateSpecular,
								MBSI_NULL)) {
							emitDiffuse = *mayaStateDiffuse;
							emitSpecular = *mayaStateSpecular;
						}
						iter->get_contribution(&lightColor);
						dot_nl = iter->get_dot_nl();
						
						/* Lambert's cosine law */
						if(emitDiffuse && (whichlayer==LAYER_combined || whichlayer==LAYER_diff)) {
							sum.r += dot_nl * lightColor.r;
							sum.g += dot_nl * lightColor.g;
							sum.b += dot_nl * lightColor.b;
						}
					}
					samples = iter->get_number_of_samples();
					if (samples) {
						passes[LAYER_diff].r += sum.r / samples * diff;
						passes[LAYER_diff].g += sum.g / samples * diff;
						passes[LAYER_diff].b += sum.b / samples * diff;
					}
				}
			}

			//start image based lighting
			miBoolean ibl_emit_diff =	*mi_eval_boolean(&paras->ibl_emit_diffuse);
			if(ibl_en==3 && ibl_emit_diff)
			{
				orig_state = state;
				state_copy = *orig_state;
				state = &state_copy;

				miColor		bent_normal =	*mi_eval_color(&paras->bent);
				miInteger	shadow_mode	=	*mi_eval_integer(&paras->ibl_shadow_mode);
				float		min			=	*mi_eval_scalar(&paras->min_dist);
				float		max			=	*mi_eval_scalar(&paras->max_dist);
				miColor		bright		=	*mi_eval_color(&paras->ibl_bright);
				miColor		dark		=	*mi_eval_color(&paras->ibl_dark);
				miScalar	occ_alpha	=	bent_normal.a;
				miVector	bent_vector, bent_i;

				bent_vector.x = (bent_normal.r * 2.0f) - 1.0f;
				bent_vector.y = (bent_normal.g * 2.0f) - 1.0f;
				bent_vector.z = (bent_normal.b * 2.0f) - 1.0f;
				mi_vector_normalize(&bent_vector);

				switch(*mi_eval_integer(&paras->bent_space)) {
					case 0: /* World */
						mi_normal_from_world(state, &bent_i, &bent_vector);
						break;
					case 1: /* Camera */
						mi_normal_from_camera(state, &bent_i, &bent_vector);
						break;
					case 2: /* Object */
						mi_normal_from_object(state, &bent_i, &bent_vector);
						break;
					case 3: /* No change */
						bent_i = bent_vector;
						break;
				}

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
					  }else if(shadow_mode==2) factor = 1;

				ccg_color_init(&sum, 0);
				if(mi_trace_environment(&lightColor, state, &bent_i)) {
					dot_nl = mi_vector_dot(&state->normal, &bent_i);
					if(dot_nl>0)
					{
						miColor f;
						f.r = dark.r + factor*(bright.r - dark.r);
						f.g = dark.g + factor*(bright.g - dark.g);
						f.b = dark.b + factor*(bright.b - dark.b);
						if(ibl_emit_diff) {
							sum.r = dot_nl * lightColor.r * diff * f.r;
							sum.g = dot_nl * lightColor.g * diff * f.g;
							sum.b = dot_nl * lightColor.b * diff * f.b;
							ccg_color_add(&passes[LAYER_diff], &sum, &passes[LAYER_diff]);
						}
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
								sum.r = sum.g = sum.b = 0;
								samples = 0;
								while (mi_sample_light(&lightColor, &dir, &dot_nl, state, *iblTag, &samples)) {
									if (mayabase_stateitem_get(state,
											MBSI_LIGHTDIFFUSE, &mayaStateDiffuse,
											MBSI_LIGHTSPECULAR, &mayaStateSpecular,
											MBSI_NULL)) {
										emitDiffuse = *mayaStateDiffuse;
										emitSpecular = *mayaStateSpecular;
									}
									/* Lambert's cosine law */
									if(emitDiffuse) {
										sum.r += dot_nl * lightColor.r;
										sum.g += dot_nl * lightColor.g;
										sum.b += dot_nl * lightColor.b;
									}
								}
								if (samples){
									sum.r = sum.r / samples * diff;
									sum.g = sum.g / samples * diff;
									sum.b = sum.b / samples * diff;
									ccg_color_add(&passes[LAYER_diff], &sum, &passes[LAYER_diff]);
								}
							}
						}
					}
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
						state->refraction_level--;
						if(!mi_trace_transparent(&tempColor, state))
						{
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
		//although lambert doesn't contribute specular, you cann't refuse the specular from other behind it which appear transparent.
		if(whichlayer==LAYER_spec){
			if(itransp.a!=1.0){
				if(enableTransPass){
					state->refraction_level--;
					if(!mi_trace_transparent(&tempColor, state)){
						tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					}
					passes[LAYER_spec].r += transp->r * tempColor.r;
					passes[LAYER_spec].g += transp->g * tempColor.g;
					passes[LAYER_spec].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_spec]);
			return(miTRUE);
		}

		//PASS: reflection
		if(whichlayer==LAYER_refl){
			if(itransp.a!=1.0){
				if(enableTransPass){
					state->refraction_level--;
					if(!mi_trace_transparent(&tempColor, state)){
						tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					}
					passes[LAYER_refl].r += transp->r * tempColor.r;
					passes[LAYER_refl].g += transp->g * tempColor.g;
					passes[LAYER_refl].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_refl]);
			return(miTRUE);
		}

		//PASS: refraction
		if(whichlayer==LAYER_refr || whichlayer==LAYER_combined)
		{
			if(itransp.a!=1.0){
				state->refraction_level--;
				if(!mi_trace_transparent(&tempColor, state)){
					tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
				}
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
		if(whichlayer==LAYER_translucent){
			if(itransp.a!=1.0){
				if(enableTransPass){
					state->refraction_level--;
					if(!mi_trace_transparent(&tempColor, state)){
						tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
					}
					passes[LAYER_translucent].r += transp->r * tempColor.r;
					passes[LAYER_translucent].g += transp->g * tempColor.g;
					passes[LAYER_translucent].b += transp->b * tempColor.b;
				}
			}
			ccg_color_assign(result, &passes[LAYER_translucent]);
			return(miTRUE);
		}

		//PASS: global illumination
		if(whichlayer==LAYER_globillum || whichlayer==LAYER_combined){
			mi_compute_avg_radiance(&passes[LAYER_globillum], state, 'f', NULL);
			passes[LAYER_globillum].r *= diff * color->r * itransp.r;
			passes[LAYER_globillum].g *= diff * color->g * itransp.g;
			passes[LAYER_globillum].b *= diff * color->b * itransp.b;
			if(whichlayer==LAYER_globillum){
				ccg_color_assign(result, &passes[LAYER_globillum]);
				return(miTRUE);
			}
		}

		//PASS: combined
		passes[LAYER_combined].r = passes[LAYER_col].r * (passes[LAYER_diff].r + passes[LAYER_ambi].r);
		passes[LAYER_combined].g = passes[LAYER_col].g * (passes[LAYER_diff].g + passes[LAYER_ambi].g);
		passes[LAYER_combined].b = passes[LAYER_col].b * (passes[LAYER_diff].b + passes[LAYER_ambi].b);

		/*
		//PASS: ambient occlusion
		miBoolean amb_add	= *mi_eval_boolean(&paras->add_to_combined);
		if(whichlayer == LAYER_ao || (amb_add && whichlayer == LAYER_combined))
		{
			if(bent_normal.a!=-1) ccg_color_init(&passes[LAYER_ao], bent_normal.a);
			else passes[LAYER_ao] = *ccg_mi_eval_color(state, &paras->ambientOcclusion);
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
		*/

		//r
		passes[LAYER_combined].r *= itransp.r;
		passes[LAYER_combined].r += passes[LAYER_incan].r + passes[LAYER_spec].r + passes[LAYER_refr].r + passes[LAYER_globillum].r;
		//g
		passes[LAYER_combined].g *= itransp.g;
		passes[LAYER_combined].g += passes[LAYER_incan].g + passes[LAYER_spec].g + passes[LAYER_refr].g + passes[LAYER_globillum].g;
		//b
		passes[LAYER_combined].b *= itransp.b;
		passes[LAYER_combined].b += passes[LAYER_incan].b + passes[LAYER_spec].b + passes[LAYER_refr].b + passes[LAYER_globillum].b;
		//a
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

				miColor		bent_normal;
				bent_normal.a = -1;	//-1 used to decide if alpha contain occlusion info
				ambi	=  *mi_eval_color(&paras->ambient);
				int ibl_en = *mi_eval_integer(&paras->ibl_enable);
				if(ibl_en==1){
					orig_state = state;
					state_copy = *orig_state;
					state = &state_copy;
					bent_normal = *mi_eval_color(&paras->bent);
					miVector	bent_vector, bent_i;
					ccg_colormap2Vector(&bent_normal, &bent_vector);
					ccg_bent_space_conversion(state, *mi_eval_integer(&paras->bent_space), &bent_i, &bent_vector);
					if(mi_trace_environment(&lightColor, state, &bent_i)) {
						ccg_color_add(&lightColor, &ambi, &ambi);
					}
					state = (miState*)orig_state;
				}
				ccg_color_assign(&passesCombined[LAYER_ambi],&ambi);
				ccg_color_assign(&passes[LAYER_ambi],&ambi);

				color	= mi_eval_color(&paras->color);
				ccg_color_assign(&passesCombined[LAYER_col],color);
				ccg_color_assign(&passes[LAYER_col],color);
				incan	= mi_eval_color(&paras->incandescence);
				ccg_color_assign(&passesCombined[LAYER_incan],incan);
				ccg_color_assign(&passes[LAYER_incan],incan);
				diff	=  *mi_eval_scalar(&paras->diffuse);
				m     = *mi_eval_integer(&paras->mode);
				n_l   = *mi_eval_integer(&paras->n_light);
				i_l   = *mi_eval_integer(&paras->i_light);
				light =  mi_eval_tag(paras->light) + i_l;

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
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
							passes[LAYER_col].r = itransp.r * passes[LAYER_col].r + transp->r * tempColor.r;
							passes[LAYER_col].g = itransp.g * passes[LAYER_col].g + transp->g * tempColor.g;
							passes[LAYER_col].b = itransp.b * passes[LAYER_col].b + transp->b * tempColor.b;
							passes[LAYER_col].a = itransp.a + transp->a * tempColor.a;
							if(state->type==miRAY_EYE)	mystate->raystate = LAYER_RAY;
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
					if(itransp.a!=1.0)
					{
						if(enableTransPass){
							mystate->raystate = LAYER_incan;
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
							passes[LAYER_incan].r += transp->r * tempColor.r;
							passes[LAYER_incan].g += transp->g * tempColor.g;
							passes[LAYER_incan].b += transp->b * tempColor.b;
							if(state->type==miRAY_EYE)	mystate->raystate = LAYER_RAY;
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
				if(mystate->raystate==LAYER_sssfront || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0){
						if(enableTransPass){
							mystate->raystate = LAYER_sssfront;
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
							passes[LAYER_sssfront].r += transp->r * tempColor.r;
							passes[LAYER_sssfront].g += transp->g * tempColor.g;
							passes[LAYER_sssfront].b += transp->b * tempColor.b;
							if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
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

				//PASS: subsurface scatering middle
				if(mystate->raystate==LAYER_sssmiddle || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0){
						if(enableTransPass){
							mystate->raystate = LAYER_sssmiddle;
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
							passes[LAYER_sssmiddle].r += transp->r * tempColor.r;
							passes[LAYER_sssmiddle].g += transp->g * tempColor.g;
							passes[LAYER_sssmiddle].b += transp->b * tempColor.b;
							if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
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

				//PASS: subsurface scatering back
				if(mystate->raystate==LAYER_sssback || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0){
						if(enableTransPass){
							mystate->raystate = LAYER_sssback;
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
							passes[LAYER_sssback].r += transp->r * tempColor.r;
							passes[LAYER_sssback].g += transp->g * tempColor.g;
							passes[LAYER_sssback].b += transp->b * tempColor.b;
							if(state->type==miRAY_EYE) mystate->raystate = LAYER_RAY;
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

				if (m == 1)   /* modify light list (inclusive mode) */
					mi_inclusive_lightlist(&n_l, &light, state);
				else if (m == 2)  /* modify light list (exclusive mode) */
					mi_exclusive_lightlist(&n_l, &light, state);
				else if(m == 4){
					n_l = 0;
					light = 0;
				}

				/* Loop over all light sources */
				if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_diff || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_withoutShadow)
				{
					again_light = light;
					/*	without shadow *****************************************/
					if(mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_withoutShadow)
					{
						orig_option = state->options;
						option_copy = *orig_option;
						option_copy.shadow = 0;
						state->options = &option_copy;
						if (m == 4 || n_l) {
							for (mi::shader::LightIterator iter(state, light, n_l);!iter.at_end(); ++iter){
								ccg_color_init(&sum, 0);
								while (iter->sample()) {
									if (mayabase_stateitem_get(state,
											MBSI_LIGHTDIFFUSE, &mayaStateDiffuse,
											MBSI_LIGHTSPECULAR, &mayaStateSpecular,
											MBSI_NULL)) {
										emitDiffuse = *mayaStateDiffuse;
										emitSpecular = *mayaStateSpecular;
									}
									iter->get_contribution(&lightColor);
									dot_nl = iter->get_dot_nl();
									/* Lambert's cosine law */
									if(emitDiffuse)
									{
										sum.r += dot_nl * lightColor.r;
										sum.g += dot_nl * lightColor.g;
										sum.b += dot_nl * lightColor.b;
									}
								}
								samples = iter->get_number_of_samples();
								if (samples) {
									withoutShadow.r += sum.r / samples * diff;
									withoutShadow.g += sum.g / samples * diff;
									withoutShadow.b += sum.b / samples * diff;
									ccg_color_assign(&passes[LAYER_diff], &withoutShadow);
								}
							}
						}
						state->options = (miOptions*)orig_option;
					}

					light = again_light;
					/*	with shadow *****************************************/
					if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_diff)
					{
						/*orig_option = state->options;
						option_copy = *orig_option;
						option_copy.shadow = shadow_state;
						state->options = &option_copy;*/
						if (m == 4 || n_l) {
							for (mi::shader::LightIterator iter(state, light, n_l);!iter.at_end(); ++iter) {
								ccg_color_init(&sum, 0);
								while (iter->sample()) {
									//get custom maya light properties
									if (mayabase_stateitem_get(state,
											MBSI_LIGHTDIFFUSE, &mayaStateDiffuse,
											MBSI_LIGHTSPECULAR, &mayaStateSpecular,
											MBSI_NULL)) {
										emitDiffuse = *mayaStateDiffuse;
										emitSpecular = *mayaStateSpecular;
									}
									iter->get_contribution(&lightColor);
									dot_nl = iter->get_dot_nl();
									/* Lambert's cosine law */
									if(emitDiffuse)
									{
										sum.r += dot_nl * lightColor.r;
										sum.g += dot_nl * lightColor.g;
										sum.b += dot_nl * lightColor.b;
									}
								}
								samples = iter->get_number_of_samples();
								if (samples) {
									passesCombined[LAYER_diff].r += sum.r / samples * diff;
									passesCombined[LAYER_diff].g += sum.g / samples * diff;
									passesCombined[LAYER_diff].b += sum.b / samples * diff;
									ccg_color_assign(&withShadow, &passesCombined[LAYER_diff]);
								}
							}
						}
						/*state->options = (miOptions*)orig_option;*/
					}

					//start image based lighting
					miBoolean ibl_emit_diff =	*mi_eval_boolean(&paras->ibl_emit_diffuse);
					if(ibl_en==3 && ibl_emit_diff)
					{
						orig_state = state;
						state_copy = *orig_state;
						state = &state_copy;

						miColor		bent_normal =	*mi_eval_color(&paras->bent);
						miInteger	shadow_mode	=	*mi_eval_integer(&paras->ibl_shadow_mode);
						float		min			=	*mi_eval_scalar(&paras->min_dist);
						float		max			=	*mi_eval_scalar(&paras->max_dist);
						miColor		bright		=	*mi_eval_color(&paras->ibl_bright);
						miColor		dark		=	*mi_eval_color(&paras->ibl_dark);
						miScalar	occ_alpha	=	bent_normal.a;
						miVector	bent_vector, bent_i;

						bent_vector.x = (bent_normal.r * 2.0f) - 1.0f;
						bent_vector.y = (bent_normal.g * 2.0f) - 1.0f;
						bent_vector.z = (bent_normal.b * 2.0f) - 1.0f;
						mi_vector_normalize(&bent_vector);

						switch(*mi_eval_integer(&paras->bent_space)) {
							case 0: /* World */
								mi_normal_from_world(state, &bent_i, &bent_vector);
								break;
							case 1: /* Camera */
								mi_normal_from_camera(state, &bent_i, &bent_vector);
								break;
							case 2: /* Object */
								mi_normal_from_object(state, &bent_i, &bent_vector);
								break;
							case 3: /* No change */
								bent_i = bent_vector;
								break;
						}

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
							  }else if(shadow_mode==2) factor = 1;

						ccg_color_init(&sum, 0);		//here sum as diffuse with shadow now
						ccg_color_init(&tempColor, 0); //here tempColor as diffuse without shadow now
						if(mi_trace_environment(&lightColor, state, &bent_i)) {
							dot_nl = mi_vector_dot(&state->normal, &bent_i);
							if(dot_nl>0)
							{
								miColor f;
								f.r = dark.r + factor*(bright.r - dark.r);
								f.g = dark.g + factor*(bright.g - dark.g);
								f.b = dark.b + factor*(bright.b - dark.b);
								f.a = 0;
								if(ibl_emit_diff) {
									tempColor.r = dot_nl * lightColor.r * diff;
									tempColor.g = dot_nl * lightColor.g * diff;
									tempColor.b = dot_nl * lightColor.b * diff;
									if(mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_withoutShadow)
									{
										ccg_color_add(&withoutShadow, &tempColor, &withoutShadow);
										ccg_color_assign(&passes[LAYER_diff], &withoutShadow);
									}
									if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_diff)
									{
										ccg_color_multiply(&tempColor, &f, &sum);
										ccg_color_add(&passesCombined[LAYER_diff], &sum, &passesCombined[LAYER_diff]);
										ccg_color_assign(&withShadow, &passesCombined[LAYER_diff]);
									}
								}
							}
						}

						state = (miState*)orig_state;
					}else if(ibl_en==2)
						{
							if((*fbarray)->number>0 && (*fbarray)->angle<1.0f)
							{
								miTag *iblTag = (*fbarray)->lightTag;
								int n_ibl = (*fbarray)->number;
								/* without shadow */
								if(mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_withoutShadow)
								{
									orig_option = state->options;
									option_copy = *orig_option;
									option_copy.shadow = 0;
									state->options = &option_copy;
									miTag realLight;
									miVector lightDir;
									miVector lightPos;
									int rightPosition = 0;
									for (n=0; n < n_ibl; n++, iblTag++){
										mi_query(miQ_INST_ITEM,NULL, *iblTag, &realLight );
										mi_query(miQ_LIGHT_DIRECTION, state, realLight, &lightDir);
										mi_vector_neg(&lightDir);
										if((*fbarray)->light_type==2) {
											mi_query(miQ_LIGHT_ORIGIN, state, realLight, &lightPos);
											rightPosition = ccg_on_plane(&state->normal, &state->point, &lightPos);
										}else rightPosition = 1;
										if(mi_vector_dot(&state->normal, &lightDir) >= (*fbarray)->angle)
										{
											sum.r = sum.g = sum.b = 0;
											samples = 0;
											while (mi_sample_light(&lightColor, &dir, &dot_nl, state, *iblTag, &samples)) {
												if (mayabase_stateitem_get(state,
														MBSI_LIGHTDIFFUSE, &mayaStateDiffuse,
														MBSI_LIGHTSPECULAR, &mayaStateSpecular,
														MBSI_NULL)) {
													emitDiffuse = *mayaStateDiffuse;
													emitSpecular = *mayaStateSpecular;
												}
												/* Lambert's cosine law */
												if(emitDiffuse) {
													sum.r += dot_nl * lightColor.r;
													sum.g += dot_nl * lightColor.g;
													sum.b += dot_nl * lightColor.b;
												}
											}
											if (samples){
												sum.r = sum.r / samples * diff;
												sum.g = sum.g / samples * diff;
												sum.b = sum.b / samples * diff;
												ccg_color_add(&withoutShadow, &sum, &withoutShadow);
												ccg_color_add(&passes[LAYER_diff], &sum, &passes[LAYER_diff]);
											}
										}
									}
									state->options = (miOptions*)orig_option;
								}

								/* with shadow */
								iblTag = (*fbarray)->lightTag;
								if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY || mystate->raystate==LAYER_diff)
								{
									/*orig_option = state->options;
									option_copy = *orig_option;
									option_copy.shadow = shadow_state;
									state->options = &option_copy;*/
									miTag realLight;
									miVector lightDir;
									miVector lightPos;
									int rightPosition = 0;
									for (n=0; n < n_ibl; n++, iblTag++){
										mi_query( miQ_INST_ITEM,NULL, *iblTag, &realLight );
										mi_query(miQ_LIGHT_DIRECTION, state, realLight, &lightDir);
										mi_vector_neg(&lightDir);
										if((*fbarray)->light_type==2) {
											mi_query(miQ_LIGHT_ORIGIN, state, realLight, &lightPos);
											rightPosition = ccg_on_plane(&state->normal, &state->point, &lightPos);
										}else rightPosition = 1;
										if(mi_vector_dot(&state->normal, &lightDir) >= (*fbarray)->angle)
										{
											ccg_color_init(&sum, 0);
											samples = 0;
											while (mi_sample_light(&lightColor, &dir, &dot_nl, state, *iblTag, &samples)) {
												//get custom maya light properties
												if (mayabase_stateitem_get(state,
														MBSI_LIGHTDIFFUSE, &mayaStateDiffuse,
														MBSI_LIGHTSPECULAR, &mayaStateSpecular,
														MBSI_NULL)) {
													emitDiffuse = *mayaStateDiffuse;
													emitSpecular = *mayaStateSpecular;
												}
												/* Lambert's cosine law */
												if(emitDiffuse) {
													sum.r += dot_nl * lightColor.r;
													sum.g += dot_nl * lightColor.g;
													sum.b += dot_nl * lightColor.b;
												}
											}
											if (samples) {
												sum.r = sum.r / samples * diff;
												sum.g = sum.g / samples * diff;
												sum.b = sum.b / samples * diff;
												ccg_color_add(&passesCombined[LAYER_diff], &sum, &passesCombined[LAYER_diff]);
												ccg_color_assign(&withShadow, &passesCombined[LAYER_diff]);
											}
										}
									}
									/*state->options = (miOptions*)orig_option;*/
								}
							}
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
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state))
									{
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
									withoutShadow.r = itransp.r * withoutShadow.r + transp->r * tempColor.r;
									withoutShadow.g = itransp.g * withoutShadow.g + transp->g * tempColor.g;
									withoutShadow.b = itransp.b * withoutShadow.b + transp->b * tempColor.b;
									withoutShadow.a = itransp.a + transp->a * tempColor.a;
									ccg_color_assign(&passes[LAYER_diff], &withoutShadow);
									if(state->type==miRAY_EYE){
										mystate->raystate = LAYER_RAY;}
									state->options = (miOptions*)orig_option;
								}
								//with shadow
								if(mystate->raystate==LAYER_diff || mystate->raystate==LAYER_RAY)
								{
									mystate->raystate = LAYER_diff;
									state->refraction_level--;
									if(!mi_trace_transparent(&tempColor, state))
									{
										tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
									}
									withShadow.r = itransp.r * withShadow.r + transp->r * tempColor.r;
									withShadow.g = itransp.g * withShadow.g + transp->g * tempColor.g;
									withShadow.b = itransp.b * withShadow.b + transp->b * tempColor.b;
									withShadow.a = itransp.a + transp->a * tempColor.a;
									if(state->type==miRAY_EYE){
										mystate->raystate = LAYER_RAY;}
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
				//although lambert doesn't contribute specular, you cann't refuse the specular from other behind it which appear transparent.
				if(mystate->raystate==LAYER_spec || mystate->raystate==LAYER_RAY){
					if(itransp.a!=1.0)
					{
						if(enableTransPass){
							mystate->raystate = LAYER_spec;
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
							passes[LAYER_spec].r += transp->r * tempColor.r;
							passes[LAYER_spec].g += transp->g * tempColor.g;
							passes[LAYER_spec].b += transp->b * tempColor.b;
							if(state->type==miRAY_EYE)	mystate->raystate = LAYER_RAY;
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
				if(mystate->raystate==LAYER_refl || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0)
					{
						if(enableTransPass){
							mystate->raystate = LAYER_refl;
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
							passes[LAYER_refl].r += transp->r * tempColor.r;
							passes[LAYER_refl].g += transp->g * tempColor.g;
							passes[LAYER_refl].b += transp->b * tempColor.b;
							if(state->type==miRAY_EYE)	mystate->raystate = LAYER_RAY;
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

				//PASS: refraction
				if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0f){
						//mystate->raystate = LAYER_combined;
						mystate->scope = CCG_SCOPE_DISABLE_PASS;
						state->refraction_level--;
						if(!mi_trace_transparent(&tempColor, state)){
							tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
						}
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
				if(mystate->raystate==LAYER_translucent || mystate->raystate==LAYER_RAY)
				{
					if(itransp.a!=1.0){
						if(enableTransPass){
							mystate->raystate = LAYER_translucent;
							state->refraction_level--;
							if(!mi_trace_transparent(&tempColor, state)){
								tempColor.r = tempColor.g = tempColor.b = tempColor.a = 0.0;
							}
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
					}else{ 
							ccg_color_assign(result, &passes[LAYER_translucent]);
							return(miTRUE);
						}
				}

				//PASS: global illumination
				if(mystate->raystate==LAYER_combined || mystate->raystate==LAYER_RAY){
					mi_compute_avg_radiance(&passesCombined[LAYER_globillum], state, 'f', NULL);
					passesCombined[LAYER_globillum].r *= diff * color->r * itransp.r;
					passesCombined[LAYER_globillum].g *= diff * color->g * itransp.g;
					passesCombined[LAYER_globillum].b *= diff * color->b * itransp.b;
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

				/*
				//PASS: ambient and reflect occlusion
				if(state->type == miRAY_EYE)
				{
					miBoolean amb_add = *mi_eval_boolean(&paras->add_to_combined);
					if(bent_normal.a!=-1) ccg_color_init(&passes[LAYER_ao], bent_normal.a);
					else passes[LAYER_ao] = *ccg_mi_eval_color(state, &paras->ambientOcclusion);
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
				*/

				//r
				passesCombined[LAYER_combined].r *= itransp.r;
				passesCombined[LAYER_combined].r += passesCombined[LAYER_incan].r + passesCombined[LAYER_spec].r + passesCombined[LAYER_refr].r + passesCombined[LAYER_globillum].r;
				//g
				passesCombined[LAYER_combined].g *= itransp.g;
				passesCombined[LAYER_combined].g += passesCombined[LAYER_incan].g + passesCombined[LAYER_spec].g + passesCombined[LAYER_refr].g + passesCombined[LAYER_globillum].g;
				//b
				passesCombined[LAYER_combined].b *= itransp.b;
				passesCombined[LAYER_combined].b += passesCombined[LAYER_incan].b + passesCombined[LAYER_spec].b + passesCombined[LAYER_refr].b + passesCombined[LAYER_globillum].b;
				//a
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

	//copyright: if(!ccg_color_compare(&copyright, &ccg_trans_black)) ccg_color_assign(&passes[whichlayer], &copyright);
	result->r = passes[whichlayer].r;
	result->g = passes[whichlayer].g;
	result->b = passes[whichlayer].b;
	result->a = passes[whichlayer].a;
  
	return(miTRUE);
}
