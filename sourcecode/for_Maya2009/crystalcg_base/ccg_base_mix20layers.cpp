/**************************************************
Source code originally comes from ccg_base_mix20layers.cpp by Jan Sandstrom (Pixero)
modified by lvyuedong
***************************************************/

#include <math.h>
#include "shader.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#pragma warning(disable : 4267)
//#pragma warning (disable : 4244)

struct ccg_base_mix20layers {
  //global attributes
  miInteger g_layers_number;
  miBoolean g_inverse_alpha;

  //local attributes
  miBoolean Enable[20];
  miInteger BlendMode[20];
  miColor   Opacity[20];
  miTag		shader[20];
  miInteger AlphaMode[20];

  miScalar  mix[20];
  miBoolean alphaInvert[20];
  miBoolean useOpacity[20];

  //clip switch
  miBoolean ColorClip;

	miBoolean	passesInOnce;
	miTag		fbWriteString;
	miBoolean	disableShadowChain;
};

extern "C" DLLEXPORT void ccg_base_mix20layers_init(      /* init shader */
    miState         *state,
    struct ccg_base_mix20layers *paras,
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
				if(!fbc) mi_error("Please set the fbWriteString parameter for ccg_base_mix20layers shader.");
				else {
						char *fbstr = mi_mem_strdup((char*)mi_db_access(fbc));
						mi_db_unpin( fbc );
						ccg_pass_string( fbstr, *fbarray);
						mi_mem_release(fbstr);
					}
			}
		 }
}

extern "C" DLLEXPORT void ccg_base_mix20layers_exit(      /* exit shader */
	miState         *state,
	struct ccg_base_mix20layers *paras)
{
	if(paras)
	{
		struct ccg_passfbArray **fbarray;
		mi_query(miQ_FUNC_USERPTR, state, 0, &fbarray);
		mi_mem_release(*fbarray);
	}
}

extern "C" DLLEXPORT int ccg_base_mix20layers_version() { return 1;}

void ccg_base_mix20layers_colorInvert(miColor *i)
{
  i->r = 1 - i->r;
  i->g = 1 - i->g;
  i->b = 1 - i->b;
}

void ccg_base_mix20layers_framebuffer_read(miState *state, struct ccg_passfbArray **fbarray, struct ccg_multipasses_structure *layer)
{
	mi::shader::Access_fb framebuffers(state->camera->buffertag);
	char buffer_name[LAYER_string_max_length];
	size_t buffer_index;
	
	//combined
	if((*fbarray)->passfbArray[LAYER_combined] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_combined), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->combined);
	}
	//color
	if((*fbarray)->passfbArray[LAYER_col] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_col), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->col);
	}
	//diffuse
	if((*fbarray)->passfbArray[LAYER_diff] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_diff), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->diff);
	}
	//ambient
	if((*fbarray)->passfbArray[LAYER_ambi] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_ambi), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->ambi);
	}
	//specular
	if((*fbarray)->passfbArray[LAYER_spec] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_spec), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->spec);
	}
	//incandescence
	if((*fbarray)->passfbArray[LAYER_incan] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_incan), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->incan);
	}
	//reflection
	if((*fbarray)->passfbArray[LAYER_refl] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_refl), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->refl);
	}
	//refraction
	if((*fbarray)->passfbArray[LAYER_refr] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_refr), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->refr);
	}
	//depth
	if((*fbarray)->passfbArray[LAYER_z] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_z), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->z);
	}
	//normal
	if((*fbarray)->passfbArray[LAYER_nor] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_nor), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->nor);
	}
	//ambient occlusion
	if((*fbarray)->passfbArray[LAYER_ao] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_ao), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->ao);
	}
	//shadow
	if((*fbarray)->passfbArray[LAYER_shad] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_shad), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->shad);
	}
	//reflection occlusion
	if((*fbarray)->passfbArray[LAYER_reflao] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_reflao), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->reflao);
	}
	//global illumination
	if((*fbarray)->passfbArray[LAYER_globillum] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_globillum), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->globillum);
	}
	//subsuface scattering front
	if((*fbarray)->passfbArray[LAYER_sssfront] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_sssfront), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->sssfront);
	}
	//subsuface scattering middle
	if((*fbarray)->passfbArray[LAYER_sssmiddle] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_sssmiddle), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->sssmiddle);
	}
	//subsuface scattering back
	if((*fbarray)->passfbArray[LAYER_sssback] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_sssback), buffer_index))
	{
		mi_fb_get(state, buffer_index, &layer->sssback);
	}
}

void ccg_base_mix20layers_framebuffer_write(miState *state, struct ccg_passfbArray **fbarray, struct ccg_multipasses_structure *layer)
{
	mi::shader::Access_fb framebuffers(state->camera->buffertag);
	char buffer_name[LAYER_string_max_length];
	size_t buffer_index;
	
	//combined
	if((*fbarray)->passfbArray[LAYER_combined] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_combined), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->combined);
	}
	//color
	if((*fbarray)->passfbArray[LAYER_col] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_col), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->col);
	}
	//diffuse
	if((*fbarray)->passfbArray[LAYER_diff] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_diff), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->diff);
	}
	//ambient
	if((*fbarray)->passfbArray[LAYER_ambi] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_ambi), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->ambi);
	}
	//specular
	if((*fbarray)->passfbArray[LAYER_spec] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_spec), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->spec);
	}
	//incandescence
	if((*fbarray)->passfbArray[LAYER_incan] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_incan), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->incan);
	}
	//reflection
	if((*fbarray)->passfbArray[LAYER_refl] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_refl), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->refl);
	}
	//refraction
	if((*fbarray)->passfbArray[LAYER_refr] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_refr), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->refr);
	}
	//depth
	if((*fbarray)->passfbArray[LAYER_z] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_z), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->z);
	}
	//normal
	if((*fbarray)->passfbArray[LAYER_nor] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_nor), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->nor);
	}
	//ambient occlusion
	if((*fbarray)->passfbArray[LAYER_ao] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_ao), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->ao);
	}
	//shadow
	if((*fbarray)->passfbArray[LAYER_shad] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_shad), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->shad);
	}
	//reflection occlusion
	if((*fbarray)->passfbArray[LAYER_reflao] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_reflao), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->reflao);
	}
	//global illumination
	if((*fbarray)->passfbArray[LAYER_globillum] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_globillum), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->globillum);
	}
	//subsuface scattering front
	if((*fbarray)->passfbArray[LAYER_sssfront] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_sssfront), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->sssfront);
	}
	//subsuface scattering middle
	if((*fbarray)->passfbArray[LAYER_sssmiddle] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_sssmiddle), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->sssmiddle);
	}
	//subsuface scattering back
	if((*fbarray)->passfbArray[LAYER_sssback] && framebuffers->get_index(ccg_get_pass_name(buffer_name, LAYER_sssback), buffer_index))
	{
		mi_fb_put(state, buffer_index, &layer->sssback);
	}
}

void ccg_base_mix20layers_initMultipasses(struct ccg_multipasses_structure *layer)
{
	miColor black = {0,0,0,0};
	layer->ambi = black;
	layer->ao = black;
	layer->col = black;
	layer->combined = black;
	layer->diff = black;
	layer->globillum = black;
	layer->incan = black;
	layer->nor = black;
	layer->refl = black;
	layer->reflao = black;
	layer->refr = black;
	layer->shad = black;
	layer->spec = black;
	layer->sssback = black;
	layer->sssfront = black;
	layer->sssmiddle = black;
	layer->z = black;
}

void ccg_base_mix20layers_blend(miInteger Mode, miInteger AlphaMode, miColor *opac, miColor *LayerResult, miColor *A, miColor *B)
{
	switch(Mode) {
		case 0:   // Normal
				  LayerResult->r = opac->r * B->r + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * B->g + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * B->b + (1 - opac->b) * A->b;
				  break;
		case 1:   // Darken              
				  LayerResult->r = opac->r * ((A->r < B->r) ? A->r : B->r) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * ((A->g < B->g) ? A->g : B->g) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * ((A->b < B->b) ? A->b : B->b) + (1 - opac->b) * A->b;
				  break;
		case 2:   // Multiply
				  LayerResult->r = opac->r * (A->r * B->r) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (A->g * B->g) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (A->b * B->b) + (1 - opac->b) * A->b;         
				  break;
		case 3:   // Color burn
				  LayerResult->r = opac->r * (1 - (1 - A->r) / B->r) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (1 - (1 - A->g) / B->g) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (1 - (1 - A->b) / B->b) + (1 - opac->b) * A->b;
				  break;
		case 4:   // Inverse Color burn
				  LayerResult->r = opac->r * (1 - (1 - B->r) / A->r) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (1 - (1 - B->g) / A->g) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (1 - (1 - B->b) / A->b) + (1 - opac->b) * A->b;
				  break;
		case 5:   // Subtract
				  LayerResult->r = opac->r * (A->r + B->r - 1.0f) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (A->g + B->g - 1.0f) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (A->b + B->b - 1.0f) + (1 - opac->b) * A->b;      
				  break;
		case 6:   // Add
				  LayerResult->r = opac->r * (A->r + B->r) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (A->g + B->g) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (A->b + B->b) + (1 - opac->b) * A->b;       
				  break;
		case 7:   // Lighten       
				  LayerResult->r = opac->r * ((A->r > B->r) ? A->r : B->r) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * ((A->g > B->g) ? A->g : B->g) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * ((A->b > B->b) ? A->b : B->b) + (1 - opac->b) * A->b;     
				  break;
		case 8:   // Screen
				  LayerResult->r = opac->r * (1 - (1 - A->r) * (1-B->r)) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (1 - (1 - A->g) * (1-B->g)) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (1 - (1 - A->b) * (1-B->b)) + (1 - opac->b) * A->b;
				  break;
		case 9:   // Color Dodge
				  LayerResult->r = opac->r * (A->r / (1 - B->r)) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (A->g / (1 - B->g)) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (A->b / (1 - B->b)) + (1 - opac->b) * A->b;
				  break;
		case 10:  // Inverse Color Dodge
				  LayerResult->r = opac->r * (B->r / (1 - A->r)) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (B->g / (1 - A->g)) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (B->b / (1 - A->b)) + (1 - opac->b) * A->b;
				  break;
		case 11:  // Overlay
				  LayerResult->r = opac->r * ((A->r<0.5f)? LayerResult->r =
					A->r * 2.0f * B->r : 1.0f - 2.0f*(1.0f-A->r)*(1.0f-B->r)) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * ((A->g<0.5f)? LayerResult->g =
					A->g * 2.0f * B->g : 1.0f - 2.0f*(1.0f-A->g)*(1.0f-B->g)) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * ((A->b<0.5f)? LayerResult->b =
					A->b * 2.0f * B->b : 1.0f - 2.0f*(1.0f-A->b)*(1.0f-B->b)) + (1 - opac->b) * A->b;    
				  break;
		case 12:  // Soft Light
				  LayerResult->r = opac->r * ((B->r<0.5f)? LayerResult->r =
					2.0f*A->r*B->r+powf(A->r,2.0f)*(1.0f-2.0f*B->r):
					sqrtf(A->r)*(2.0f*B->r-1)+(2.0f*A->r)*(1.0f-B->r)) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * ((B->g<0.5f)? LayerResult->g =
					2.0f*A->g*B->g+powf(A->g,2.0f)*(1.0f-2.0f*B->g):
					sqrtf(A->g)*(2.0f*B->g-1)+(2.0f*A->g)*(1.0f-B->g)) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * ((B->b<0.5f)? LayerResult->b =
					2.0f*A->b*B->b+powf(A->b,2.0f)*(1.0f-2.0f*B->b):
					sqrtf(A->b)*(2.0f*B->b-1)+(2.0f*A->b)*(1.0f-B->b)) + (1 - opac->b) * A->b;
				  break;
		case 13:  // Hard Light
				  LayerResult->r = opac->r * ((A->r<0.5f)? LayerResult->r = A->r*2.0f*B->r:
					  1.0f - 2.0f*(1.0f-A->r)*(1.0f-B->r)) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * ((A->g<0.5f)? LayerResult->g = A->g*2.0f*B->g:
					  1.0f - 2.0f*(1.0f-A->g)*(1.0f-B->g)) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * ((A->b<0.5f)? LayerResult->b = A->b*2.0f*B->b:
					  1.0f - 2.0f*(1.0f-A->b)*(1.0f-B->b)) + (1 - opac->b) * A->b;
				  break;
		case 14:  // Reflect
				  LayerResult->r = opac->r * ((B->r==1.0f)? LayerResult->r = 1.0f: LayerResult->r =
					powf(A->r,2)/(1.0f-B->r)) + (1 - opac->r) * A->r;

				  LayerResult->g = opac->g * ((B->g==1.0f)? LayerResult->g = 1.0f: LayerResult->g =
					powf(A->g,2)/(1.0f-B->g)) + (1 - opac->g) * A->g;

				  LayerResult->b = opac->b * ((B->b==1.0f)? LayerResult->b = 1.0f: LayerResult->b =
					powf(A->b,2)/(1.0f-B->b)) + (1 - opac->b) * A->b;
				  break;
		case 15:  // Glow
				  LayerResult->r = opac->r * ((A->r==1.0f)? LayerResult->r = 1.0f: LayerResult->r =
					powf(B->r,2)/(1.0f-A->r)) + (1 - opac->r) * A->r;

				  LayerResult->g = opac->g * ((A->g==1.0f)? LayerResult->g = 1.0f: LayerResult->g =
					powf(B->g,2)/(1.0f-A->g)) + (1 - opac->g) * A->g;

				  LayerResult->b = opac->b * ((A->b==1.0f)? LayerResult->b = 1.0f: LayerResult->b =
					powf(B->b,2)/(1.0f-A->b)) + (1 - opac->b) * A->b;
				  break;
		case 16:  // Average
				  LayerResult->r = opac->r * ((A->r + B->r) / 2.0f) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * ((A->g + B->g) / 2.0f) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * ((A->b + B->b) / 2.0f) + (1 - opac->b) * A->b;          
				  break;
		case 17:  // Difference
				  LayerResult->r = opac->r * (fabsf(A->r - B->r)) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (fabsf(A->g - B->g)) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (fabsf(A->b - B->b)) + (1 - opac->b) * A->b;
				  break;
		case 18:  // Exclusion
				  LayerResult->r = opac->r * (A->r + B->r - (2 * A->r * B->r)) + (1 - opac->r) * A->r;
				  LayerResult->g = opac->g * (A->g + B->g - (2 * A->g * B->g)) + (1 - opac->g) * A->g;
				  LayerResult->b = opac->b * (A->b + B->b - (2 * A->b * B->b)) + (1 - opac->b) * A->b;     
				  break;
		default:  *LayerResult = *A;
	}//blend mode

	switch(AlphaMode) {
		case 0:		// normal
					LayerResult->a = opac->a * B->a + (1-opac->a) * A->a;
					break;
		case 1:		// Dont add to Alpha       
					LayerResult->a = A->a;       
					break;
		case 2:		// Use highest Alpha       
					LayerResult->a = opac->a * ((A->a > B->a) ? A->a : B->a) + (1 - opac->a) * A->a;       
					break;
		case 3:		// Add difference to highest Alpha       
					LayerResult->a = opac->a*((A->a > B->a) ? (A->a+(A->a-B->a)) : (B->a+(B->a-A->a))) + (1-opac->a)*A->a;
					break;
		case 4:		// Full Alpha      
					LayerResult->a = 1;       
					break;
		default:	LayerResult->a = A->a;
	}//alpha mode
}

void ccg_base_mix20layers_blendMultipasses(struct ccg_passfbArray **fbarray, miInteger Mode, miInteger AlphaMode, miColor *opac,
											struct ccg_multipasses_structure *LayerResult_passes, struct ccg_multipasses_structure *A_passes, struct ccg_multipasses_structure *B_passes)
{
	//combined
	if((*fbarray)->passfbArray[LAYER_combined])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->combined, &A_passes->combined, &B_passes->combined);
	//color
	if((*fbarray)->passfbArray[LAYER_col])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->col, &A_passes->col, &B_passes->col);
	//diffuse
	if((*fbarray)->passfbArray[LAYER_diff])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->diff, &A_passes->diff, &B_passes->diff);
	//ambient
	if((*fbarray)->passfbArray[LAYER_ambi])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->ambi, &A_passes->ambi, &B_passes->ambi);
	//specular
	if((*fbarray)->passfbArray[LAYER_spec])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->spec, &A_passes->spec, &B_passes->spec);
	//incandescence
	if((*fbarray)->passfbArray[LAYER_incan])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->incan, &A_passes->incan, &B_passes->incan);
	//reflection
	if((*fbarray)->passfbArray[LAYER_refl])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->refl, &A_passes->refl, &B_passes->refl);
	//refraction
	if((*fbarray)->passfbArray[LAYER_refr])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->refr, &A_passes->refr, &B_passes->refr);
	//depth
	if((*fbarray)->passfbArray[LAYER_z])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->z, &A_passes->z, &B_passes->z);
	//normal
	if((*fbarray)->passfbArray[LAYER_nor])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->nor, &A_passes->nor, &B_passes->nor);
	//ambient occlusion
	if((*fbarray)->passfbArray[LAYER_ao])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->ao, &A_passes->ao, &B_passes->ao);
	//shadow
	if((*fbarray)->passfbArray[LAYER_shad])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->shad, &A_passes->shad, &B_passes->shad);
	//reflection occlusion
	if((*fbarray)->passfbArray[LAYER_reflao])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->reflao, &A_passes->reflao, &B_passes->reflao);
	//global illumination
	if((*fbarray)->passfbArray[LAYER_globillum])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->globillum, &A_passes->globillum, &B_passes->globillum);
	//subsuface scattering front
	if((*fbarray)->passfbArray[LAYER_sssfront])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->sssfront, &A_passes->sssfront, &B_passes->sssfront);
	//subsuface scattering middle
	if((*fbarray)->passfbArray[LAYER_sssmiddle])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->sssmiddle, &A_passes->sssmiddle, &B_passes->sssmiddle);
	//subsuface scattering back
	if((*fbarray)->passfbArray[LAYER_sssback])
		ccg_base_mix20layers_blend(Mode, AlphaMode, opac, &LayerResult_passes->sssback, &A_passes->sssback, &B_passes->sssback);
}

extern "C" DLLEXPORT miBoolean ccg_base_mix20layers(
  miColor   *result,
  miState   *state,
  struct    ccg_base_mix20layers *paras)
{
	const miState	*orig_state;
	miState	copy_state;
	miInteger n_layers = *mi_eval_integer(&paras->g_layers_number);

	if (state->type == miRAY_SHADOW)
	{
		if(n_layers>0){
			miColor	LayerResult_shadow, A_shadow, B_shadow;
			LayerResult_shadow.r = LayerResult_shadow.g = LayerResult_shadow.b = LayerResult_shadow.a = 0;
			for (int i = 0; i < n_layers; i++){
				if(*mi_eval_boolean(&paras->Enable[i])){
					A_shadow = LayerResult_shadow;
					//call shadow shaders
					orig_state = state;
					copy_state = *orig_state;
					state = &copy_state;
					mi_call_shader(&B_shadow, miSHADER_SHADOW, state, *mi_eval_tag(&paras->shader[i]));
					state = (miState*)orig_state;
					//get layer alpha
					miColor opac;
					if(*mi_eval_boolean(&paras->useOpacity[i])){
						opac = *mi_eval_color(&paras->Opacity[i]);
						opac.a = 1 - (opac.r*0.299f + opac.g*0.587f + opac.b*0.114f);
						ccg_base_mix20layers_colorInvert(&opac); //invert transparency for maya
					} else opac.r = opac.g = opac.b = opac.a = 1;
					opac.r = opac.r>1?1:(opac.r<0?0:opac.r);  //clip alpha
					opac.g = opac.g>1?1:(opac.g<0?0:opac.g);
					opac.b = opac.b>1?1:(opac.b<0?0:opac.b);
					opac.a = opac.a>1?1:(opac.a<0?0:opac.a);
					//inverse alpha?
					if(*mi_eval_boolean(&paras->alphaInvert[i])) {
					  ccg_base_mix20layers_colorInvert(&opac);
					  opac.a = 1 - opac.a;
					}
					//mix between two layers
					miScalar mix_blend = *mi_eval_scalar(&paras->mix[i]);
					if(mix_blend!=1) {
					  opac.r *= mix_blend;
					  opac.g *= mix_blend;
					  opac.b *= mix_blend;
					  opac.a *= mix_blend;
					}
					//calculate shadow
					LayerResult_shadow.r = opac.r * B_shadow.r + (1-opac.r)*A_shadow.r;
					LayerResult_shadow.g = opac.g * B_shadow.g + (1-opac.g)*A_shadow.g;
					LayerResult_shadow.b = opac.b * B_shadow.b + (1-opac.b)*A_shadow.b;
					LayerResult_shadow.a = opac.a * B_shadow.a + (1-opac.a)*A_shadow.a;
				}// if layer enabled
			}// for loop

			if(*mi_eval_boolean(&paras->disableShadowChain)){
				result->r = LayerResult_shadow.r;
				result->g = LayerResult_shadow.g;
				result->b = LayerResult_shadow.b;
				result->a = LayerResult_shadow.a;
			}else {
						if(state->options->shadow=='s'){
							ccg_shadow_choose_volume(state);
							mi_trace_shadow_seg(result, state);
						}
						result->r *= LayerResult_shadow.r;
						result->g *= LayerResult_shadow.g;
						result->b *= LayerResult_shadow.b;
						result->a *= LayerResult_shadow.a;
				  }
			return(miTRUE);
		}else {
				result->r = result->g = result->b = result->a = 0;
				return miFALSE;
			}

		return miFALSE;
	}

	if(state->type == miRAY_DISPLACE)
		return(miFALSE);

	struct ccg_passfbArray **fbarray;
	mi_query(miQ_FUNC_USERPTR, state, 0, &fbarray);
	miBoolean passesOnce = *mi_eval_boolean(&paras->passesInOnce);

  miColor LayerResult;
  LayerResult.r = LayerResult.g = LayerResult.b = LayerResult.a = 0.0;
  miColor A, B;
  miTag	B_tag;
  struct ccg_multipasses_structure A_passes, B_passes, LayerResult_passes;
  ccg_base_mix20layers_initMultipasses(&A_passes);
  ccg_base_mix20layers_initMultipasses(&B_passes);
  ccg_base_mix20layers_initMultipasses(&LayerResult_passes);

  for (int i = 0; i < n_layers; i++)
  {
    if(*mi_eval_boolean(&paras->Enable[i]))
	{
		A = LayerResult;
		if(state->type==miRAY_EYE && passesOnce) A_passes = LayerResult_passes;
		B_tag = *mi_eval_tag(&paras->shader[i]);
		orig_state = state;
		copy_state = *orig_state;
		state = &copy_state;
		mi_call_shader(&B, miSHADER_MATERIAL, state, B_tag);
		state = (miState*)orig_state;
		if(state->type==miRAY_EYE && passesOnce){
			ccg_base_mix20layers_framebuffer_read(state, fbarray, &B_passes);
		}

		miColor opac;
		if(*mi_eval_boolean(&paras->useOpacity[i])){
			opac = *mi_eval_color(&paras->Opacity[i]);
			opac.a = 1 - (opac.r*0.299f + opac.g*0.587f + opac.b*0.114f);
			ccg_base_mix20layers_colorInvert(&opac); //invert transparency for maya
		} else opac.r = opac.g = opac.b = opac.a = B.a;
		opac.r = opac.r>1?1:(opac.r<0?0:opac.r);  //clip alpha
		opac.g = opac.g>1?1:(opac.g<0?0:opac.g);
		opac.b = opac.b>1?1:(opac.b<0?0:opac.b);
		opac.a = opac.a>1?1:(opac.a<0?0:opac.a);

		//inverse alpha?
		if(*mi_eval_boolean(&paras->alphaInvert[i])) {
		  ccg_base_mix20layers_colorInvert(&opac);
		  opac.a = 1 - opac.a;
		}

		//mix between two layers
		miScalar mix_blend = *mi_eval_scalar(&paras->mix[i]);
		if(mix_blend!=1) {
		  opac.r *= mix_blend;
		  opac.g *= mix_blend;
		  opac.b *= mix_blend;
		  opac.a *= mix_blend;
		}
	     
		miInteger Mode = *mi_eval_integer(&paras->BlendMode[i]);
		miInteger AlphaMode = *mi_eval_integer(&paras->AlphaMode[i]);

		ccg_base_mix20layers_blend(Mode, AlphaMode, &opac, &LayerResult, &A, &B);
		if(state->type==miRAY_EYE && passesOnce){
			ccg_base_mix20layers_blendMultipasses(fbarray, Mode, AlphaMode, &opac, &LayerResult_passes, &A_passes, &B_passes);
		}
     
	}//layer enabled
  }// for loop
 
  if(*mi_eval_boolean(&paras->g_inverse_alpha)){
	LayerResult.a = 1 - LayerResult.a;
	if(state->type==miRAY_EYE && passesOnce)
		LayerResult_passes.combined.a = 1 - LayerResult_passes.combined.a;
  }

  miBoolean ColorClip = *mi_eval_boolean(&paras->ColorClip);
  if (ColorClip)
  {
    if (LayerResult.r > 1.0) {LayerResult.r = 1.0;}
    else if (LayerResult.r < 0.0) {LayerResult.r = 0.0;}
    if (LayerResult.g > 1.0) {LayerResult.g = 1.0;}
    else if (LayerResult.g < 0.0) {LayerResult.g = 0.0;}
    if (LayerResult.b > 1.0) {LayerResult.b = 1.0;}
    else if (LayerResult.b < 0.0) {LayerResult.b = 0.0;}
    if (LayerResult.a > 1.0) {LayerResult.a = 1.0;}
    else if (LayerResult.a < 0.0) {LayerResult.a = 0.0;}
	
	if(state->type==miRAY_EYE && passesOnce)
	{
		if (LayerResult_passes.combined.r > 1.0) {LayerResult_passes.combined.r = 1.0;}
		else if (LayerResult_passes.combined.r < 0.0) {LayerResult_passes.combined.r = 0.0;}
		if (LayerResult_passes.combined.g > 1.0) {LayerResult_passes.combined.g = 1.0;}
		else if (LayerResult_passes.combined.g < 0.0) {LayerResult_passes.combined.g = 0.0;}
		if (LayerResult_passes.combined.b > 1.0) {LayerResult_passes.combined.b = 1.0;}
		else if (LayerResult_passes.combined.b < 0.0) {LayerResult_passes.combined.b = 0.0;}
		if (LayerResult_passes.combined.a > 1.0) {LayerResult_passes.combined.a = 1.0;}
		else if (LayerResult_passes.combined.a < 0.0) {LayerResult_passes.combined.a = 0.0;}
	}
  }

  if(state->type==miRAY_EYE && passesOnce)
	ccg_base_mix20layers_framebuffer_write(state, fbarray, &LayerResult_passes);

  *result = LayerResult;
  return miTRUE;
}
