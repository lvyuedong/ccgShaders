
#include <string.h>
#include <math.h>
#include "shader.h"
#include "crystalcg.h"

#define SHADER_VERSION 1

struct ccg_output_bezier_clamp{
     miBoolean	enable;
	 miScalar	clamp_min;
	 miScalar	clamp_max;
	 miScalar	extract_range;
	 miScalar	curvature;
	 miScalar	power;
	 miTag		extractPasses;
};

struct ccg_bezier_par{
	float h1;
	float h2;
	float k;
	float range;
	float power;
	float p2x;
	float p3x;
	float p2y;
    float p3y;
    float slope;
	float p2x2;
};

void ccg_output_bezier_clamp_calculate(miColor *input, struct ccg_bezier_par *par)
{
	float t_r = (par->p2x-fabs(sqrt(par->p2x2-(par->p2x+par->p2x-par->p3x)*(input->r-par->h1)))) / (par->p2x+par->p2x-par->p3x);
	float t_g = (par->p2x-fabs(sqrt(par->p2x2-(par->p2x+par->p2x-par->p3x)*(input->g-par->h1)))) / (par->p2x+par->p2x-par->p3x);
	float t_b = (par->p2x-fabs(sqrt(par->p2x2-(par->p2x+par->p2x-par->p3x)*(input->b-par->h1)))) / (par->p2x+par->p2x-par->p3x);
	if(par->power!=1.0f){
		t_r = pow(t_r, par->power);
		t_g = pow(t_g, par->power);
		t_b = pow(t_b, par->power);
	}
	float t_r2 = t_r * t_r;
	float t_g2 = t_g * t_g;
	float t_b2 = t_b * t_b;

	input->r = input->r>par->h1 ? ( input->r<=par->range ? ((t_r+t_r-t_r2-t_r2)*par->p2y+t_r2*par->p3y+par->h1) : (par->k==1.0f?par->h2:(par->h2+(input->r-par->range)*par->slope)) ) : input->r;
	input->g = input->g>par->h1 ? ( input->g<=par->range ? ((t_g+t_g-t_g2-t_g2)*par->p2y+t_g2*par->p3y+par->h1) : (par->k==1.0f?par->h2:(par->h2+(input->g-par->range)*par->slope)) ) : input->g;
	input->b = input->b>par->h1 ? ( input->b<=par->range ? ((t_b+t_b-t_b2-t_b2)*par->p2y+t_b2*par->p3y+par->h1) : (par->k==1.0f?par->h2:(par->h2+(input->b-par->range)*par->slope)) ) : input->b;
}

miBoolean ccg_output_bezier_clamp_extract(miState *s, miImg_image *img, struct ccg_bezier_par *par)
{
	int abort=0;
	int x,y;
	miColor color;

	for (y=0; y < s->camera->y_resolution; y++) {
		if (mi_par_aborted()){
			abort = 1;
			break;
		}
        for (x=0; x<s->camera->x_resolution; x++) {
            mi_img_get_color(img, &color, x, y);
			ccg_output_bezier_clamp_calculate(&color, par);
            mi_img_put_color(img, &color, x, y);
        }
    }

	if(abort) return(miFALSE);
	return(miTRUE);
}

extern "C" DLLEXPORT int ccg_output_bezier_clamp_version(void) {return(SHADER_VERSION);}

extern "C" DLLEXPORT miBoolean ccg_output_bezier_clamp(
        void					*result,
        register miState		*state,
        struct ccg_output_bezier_clamp	*paras)
{
	miBoolean enabled = *mi_eval_boolean(&paras->enable);
	float min = *mi_eval_scalar(&paras->clamp_min);
	float max = *mi_eval_scalar(&paras->clamp_max);
	float range = *mi_eval_scalar(&paras->extract_range);
	if(!enabled) return(miTRUE);
	if(max<=min) return(miTRUE);
	if(range<=max) return(miTRUE);

	miImg_image *image;
	int extrFb[LAYER_NUM], extrNo;
	int i;
	for(i=0;i<LAYER_NUM;i++)
		extrFb[i] = 0;

	//tokenize passes name
	/* extractPasses: string passes' names should be separated by semicolon */
	char *pass, *tmp;
	int extrPass;
	miTag fbc = *mi_eval_tag(&paras->extractPasses);
	if(!fbc) extrPass = 0;
	else {
			pass = mi_mem_strdup((char*)mi_db_access(fbc));
			mi_db_unpin( fbc );
		}
	tmp = strtok(pass,";");
	char *token = (char *)mi_mem_allocate(sizeof(char)*LAYER_string_max_length);
	mi::shader::Access_fb framebuffers(state->camera->buffertag);
	size_t buffer_index;
	extrNo=0;
	while (tmp != NULL)
	{
		strcpy(token, tmp);
		if(framebuffers->get_index(token, buffer_index)){
			extrFb[extrNo] = (int)buffer_index;
			extrNo++;
		}
		tmp = strtok (NULL, ";");
	}
	mi_mem_release(pass);
	mi_mem_release(token);

	float curve = *mi_eval_scalar(&paras->curvature);
	float pows = *mi_eval_scalar(&paras->power);
	struct ccg_bezier_par par;
	par.h1 = min;
	par.h2 = max;
	par.range = range;
	par.power = pows;
	par.k = 1 - curve;
	par.p2x = (par.h2 - par.h1)*par.k;
	par.p3x = par.range - par.h1;
	par.p2y = par.p2x;
	par.p3y = par.h2 - par.h1;
    par.slope = (par.p3y-par.p2y)/(par.p3x-par.p2x);
	par.p2x2 = par.p2x * par.p2x;

	//extract primary frame buffer
	image = mi_output_image_open(state, miRC_IMAGE_RGBA);
	ccg_output_bezier_clamp_extract(state, image, &par);
	mi_output_image_close(state, miRC_IMAGE_RGBA);

	//extract user frame buffer
	int abort = 0;
	if(extrNo){
		for(i=0;i<extrNo;i++){
			//mi_warning("user buffer: %d", miRC_IMAGE_USER+extrFb[i]);
			image = mi_output_image_open(state, miRC_IMAGE_USER+extrFb[i]);
			if(!ccg_output_bezier_clamp_extract(state, image, &par))
				abort = 1;
			mi_output_image_close(state, miRC_IMAGE_USER+extrFb[i]);
			if(abort) break;
		}
	}

	return(miTRUE);
}


