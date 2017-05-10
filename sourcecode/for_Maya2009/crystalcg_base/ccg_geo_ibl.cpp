#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include "shader.h"
#include "geoshader.h"
#include "ccgFunction.h"
#include "crystalcg.h"

using namespace std;

#pragma warning(disable : 4996)

struct ccg_geo_ibl {
	//texture
	miTag		texture;
	miInteger	tex_changeUV;	//0 for none, 1 for u, 2 for v, 3 for u&v, 4 for swap
	miVector	rotate;
	miVector	tex_rotate;
	miScalar	gamma;
	miScalar	exposure;
	miScalar	offset;
	miInteger	contrastType;
	miScalar	contrastValue;
	miScalar	contrastCenter;
	miInteger	correctOrder;
	miColor		multiply;
	miScalar	clamp_low;
	miScalar	clamp_hi;
	//light array
	miInteger	light_mode;  //0 for uv method, 1 for point evenly distribute on sphere, 2 for points data file
	miTag		light_points_file;
	miInteger	half_sphere;	//0 for hole sphere, 1 for sky half sphere, 2 for ground half sphere
	miScalar	radius;
	miInteger	sampleU;
	miInteger	sampleV;
	miInteger	sampleNumber;
	miInteger	iteration;
	miInteger	seed;
	miScalar	intensity_sky;
	miScalar	intensity_ground;
	miInteger	shadow_mode;				//0 for no shadow, 1 for shadow all, 2 for shadow limit
	miInteger	half_shadow;		//0 for hole sphere, 1 for sky half sphere, 2 for ground half sphere
	miColor		shadow_color;
	miScalar	shadow_factor;
	miScalar	min_intensity;
	miScalar	max_intensity;
	miVector	center;
	//lights property
	miInteger	light_type;		//0 for maya_dir, 1 for maya_spot, 2 for mr_dir, 3 for mr_spot
	miBoolean	visible;
	miScalar	light_radius;
	//maya dir
	miBoolean	mayadir_useLightPosition;
	miScalar	mayadir_lightAngle;
	miBoolean	mayadir_useDepthMapShadows;
	miBoolean	mayadir_emitDiffuse;
	miBoolean	mayadir_emitSpecular;
	miBoolean	mayadir_useRayTraceShadows;
	miInteger	mayadir_shadowRays;
	miInteger	mayadir_rayDepthLimit;
	//maya spot
	miScalar	mayaspot_coneAngle;
	miScalar	mayaspot_penumbraAngle;
	miScalar	mayaspot_dropoff;
	miBoolean	mayaspot_barnDoors;
	miScalar	mayaspot_leftBarnDoor;
	miScalar	mayaspot_rightBarnDoor;
	miScalar	mayaspot_topBarnDoor;
	miScalar	mayaspot_bottomBarnDoor;
	miBoolean	mayaspot_useDecayRegions;
	miScalar	mayaspot_startDistance1;
	miScalar	mayaspot_endDistance1;
	miScalar	mayaspot_startDistance2;
	miScalar	mayaspot_endDistance2;
	miScalar	mayaspot_startDistance3;
	miScalar	mayaspot_endDistance3;
	miScalar	mayaspot_lightRadius;
	miInteger	mayaspot_decayRate;
	miBoolean	mayaspot_useDepthMapShadows;
	miBoolean	mayaspot_emitDiffuse;
	miBoolean	mayaspot_emitSpecular;
	miBoolean	mayaspot_useRayTraceShadows;
	miInteger	mayaspot_shadowRays;
	miInteger	mayaspot_rayDepthLimit;
	miScalar	mayaspot_fogSpread;
	miScalar	mayaspot_fogIntensity;
	//mr dir
	miBoolean	mrdir_useLightPosition;
	miBoolean	mrdir_useDepthMapShadows;
	miBoolean	mrdir_useRayTraceShadows;
	//mr spot
	miBoolean	mrspot_useDepthMapShadows;
	miBoolean	mrspot_useRayTraceShadows;
	miBoolean   mrspot_atten;
	miScalar    mrspot_start;
	miScalar    mrspot_stop;
	miScalar    mrspot_coneAngle;
	miScalar	mrspot_penumbraAngle;
	//shadow map parameter
	miInteger	shadowmap_resolution;
	miScalar	shadowmap_softness;
	miInteger	shadowmap_samples;
	miInteger	shadowmap_format;	//0 for regular, 1 for detail
	miScalar	shadowmap_bias;
	miScalar	shadowmapdetail_accuracy;
	miInteger	shadowmapdetail_type;	//0 for color, 1 for alpha
	int			i_shadowmap_file;
	int			n_shadowmap_file;
	miTag		shadowmap_file[1];
};

struct ccg_geo_ibl_pointsParam {
	int mode;
	int half;
	int u;
	int v;
	int n;
	int number;
	int iteration;
	int seed;
	miVector center;
};

struct ccg_geo_ibl_coloradjParam {
	miScalar	gamma;
	miScalar	exposure;
	miScalar	offset;
	miInteger	contrastType;
	miScalar	contrastValue;
	miScalar	contrastCenter;
	miColor		multiply;
	miScalar	clamp_low;
	miScalar	clamp_hi;
	miInteger	correctOrder;
};

struct ccg_geo_ibl_lightPropParam {
	//maya dir
	miBoolean		md_useLightPosition;
	miScalar		md_lightAngle;
	miBoolean		md_useDepthMapShadows;
	miBoolean		md_emitDiffuse;
	miBoolean		md_emitSpecular;
	miBoolean		md_useRayTraceShadows;
	miInteger		md_shadowRays;
	miInteger		md_rayDepthLimit;
	//maya spot
	miScalar		ms_coneAngle;
	miScalar		ms_spread;
	miScalar		ms_penumbraAngle;
	miScalar		ms_dropoff;
	miBoolean		ms_barnDoors;
	miScalar		ms_leftBarnDoor;
	miScalar		ms_rightBarnDoor;
	miScalar		ms_topBarnDoor;
	miScalar		ms_bottomBarnDoor;
	miBoolean		ms_useDecayRegions;
	miScalar		ms_startDistance1;
	miScalar		ms_endDistance1;
	miScalar		ms_startDistance2;
	miScalar		ms_endDistance2;
	miScalar		ms_startDistance3;
	miScalar		ms_endDistance3;
	miScalar		ms_lightRadius;
	miInteger		ms_decayRate;
	miBoolean		ms_useDepthMapShadows;
	miBoolean		ms_emitDiffuse;
	miBoolean		ms_emitSpecular;
	miBoolean		ms_useRayTraceShadows;
	miInteger		ms_shadowRays;
	miInteger		ms_rayDepthLimit;
	miScalar		ms_fogSpread;
	miScalar		ms_fogIntensity;
	//mr dir
	miBoolean		rd_useLightPosition;
	miBoolean 		rd_useDepthMapShadows;
	miBoolean		rd_useRayTraceShadows;
	//mr spot
	miBoolean 		rs_useDepthMapShadows;
	miBoolean		rs_useRayTraceShadows;
	miBoolean		rs_atten;
	miScalar		rs_start;
	miScalar		rs_stop;
	miScalar		rs_coneAngle;
	miScalar		rs_outCosine;
	miScalar		rs_penumbraAngle;
	miScalar		rs_innerCosine;
};

miBoolean ccg_geo_ibl_pointHalf(int sky, miVector *c, miVector *p, miVector *d)
{
	if(c->x!=0 || c->y!=0 || c->z!=0 )
	{
		mi_vector_sub(d, p, c);
		mi_vector_neg(d);
		mi_vector_normalize(d);
		if(d->x==0 && d->y==0 && d->z==0) d->y = 1;
	}else {
			*d = *p;
			mi_vector_neg(d);
			mi_vector_normalize(d);
		}

	if(sky==1)
	{
		if(d->y<=0) return miTRUE;
		else return miFALSE;
	}else if(sky==2)
		{
			if(d->y>=0) return miTRUE;
			else return miFALSE;
		}

	return miTRUE;
}

int ccg_geo_ibl_getFileLinesNumber(char *filename)
{
	//caution: use <iostream> <fstream> <string>
	string line;
	int number = 0;
	ifstream myfile (filename);
	if (myfile.is_open())
	{
		while (! myfile.eof() )
		{
		  getline (myfile,line);
				number++;
		}
		myfile.close();
		return number;
	}else return 0;
}

void ccg_geo_ibl_points_distribute(
	miVector *lights, miVector *direction, struct ccg_geo_ibl_pointsParam *data,
	char *filename, miVector *rotate)
{
	int i,j;
	switch(data->mode)
	{
		case 0:		if(data->number>0) {
						miVector dir;
						float fu, fv, dir_y_sin;
						i=0;
						for(int v=1; v<data->u; v++) {
							for(int u=1; u<data->v; u++) {
								fu = float(u)/float(data->u);
								fv = float(v)/float(data->v);
								dir.y = float(-cos(fu * CCG_PI));
								dir_y_sin = 1 - pow(dir.y, 2.0f);
								dir.x = float(cos(fv * CCG_PIx2)) * dir_y_sin;
								dir.z = float(sin(fv * CCG_PIx2)) * dir_y_sin;
								mi_vector_normalize(&dir);
								lights[i] = dir;
								i++;
							}
						}
					}
					break;
		case 1:		if(data->n>0)
						ccg_sphere_point_repulsion(lights, data->n, data->iteration, data->seed);
					break;
		case 2:		if(filename && data->number>0)
					{
						string line;
						char *cstr, *p;
						ifstream myfile (filename);
					  if (myfile.is_open())
					  {
					  	i=0;
					    while (! myfile.eof() )
					    {
					      getline (myfile,line);
								cstr = new char [line.size() + 1];
								strcpy(cstr, line.c_str());
								p=strtok (cstr," ");
							  lights[i].x = (float)atof(p);
							  p=strtok (NULL," ");
							  lights[i].y = (float)atof(p);
							  p=strtok (NULL," ");
							  lights[i].z = (float)atof(p);
							  i++;
							  delete[] cstr;  
					    }
					    myfile.close();
					  }else {
					  				mi_error("Unable to open file %s", filename); 
					  				data->number = 0;
					  			}
					}
					break;
	}
	
	if(data->number>0)
	{
		//transform light position
		miMatrix rotMatrix;
		if(!(rotate->x==0 && rotate->y==0 && rotate->z==0)){
			for(i=0;i<data->number;i++){
				mi_matrix_rotate(rotMatrix, rotate->x/360.0f*float(CCG_PIx2), rotate->y/360.0f*float(CCG_PIx2), rotate->z/360.0f*float(CCG_PIx2));
				mi_vector_transform(&lights[i], &lights[i], rotMatrix);
			}
		}

		miVector *tmp;
		miVector dir;
		tmp = (miVector *)mi_mem_allocate(data->number * sizeof(miVector));
		for(i=0,j=0;i<data->number;i++) {
			if(ccg_geo_ibl_pointHalf(data->half, &data->center, &lights[i], &dir)) {
				tmp[j] = lights[i];
				direction[j] = dir;
				j++;
			}
		}
		if(j < data->number) {
			for(i=0;i<j;i++)
				lights[i] = tmp[i];
			data->number = j;
		}
		if(tmp!=NULL) mi_mem_release(tmp);
	}
}

void ccg_geo_ibl_color_adj_contrast(int contrastType, miColor *input, float v, float c)
{
	float l,m;
	if(contrastType==0)
	{
		l = 0.3f*input->r + 0.59f*input->g + 0.11f*input->b;
		if(l!=0) {
			m = v - (v-1)*c/l;
			ccg_color_multiply_scalar(input, m, input);
		}
	}else {
				m = c*(v-1);
				input->r = input->r * v - m;
				input->g = input->g * v - m;
				input->b = input->b * v - m;
			}
}
			
void ccg_geo_ibl_color_adj(miColor *color, struct ccg_geo_ibl_coloradjParam *data)
{
	//gamma
	if(data->gamma!=1) ccg_color_gamma(color,data->gamma,color);
	//color correction by order
	switch(data->correctOrder)
	{
		case 0:	if(data->exposure!=1) ccg_color_multiply_scalar(color, data->exposure, color);
						if(data->offset!=0) ccg_color_add_scalar(color, data->offset, color);
						if(data->contrastValue!=1) ccg_geo_ibl_color_adj_contrast(data->contrastType, color, data->contrastValue, data->contrastCenter);
						break;
		case 1:	if(data->exposure!=1) ccg_color_multiply_scalar(color, data->exposure, color);
						if(data->contrastValue!=1) ccg_geo_ibl_color_adj_contrast(data->contrastType, color, data->contrastValue, data->contrastCenter);
						if(data->offset!=0) ccg_color_add_scalar(color, data->offset, color);
						break;
		case 2:	if(data->offset!=0) ccg_color_add_scalar(color, data->offset, color);
						if(data->exposure!=1) ccg_color_multiply_scalar(color, data->exposure, color);
						if(data->contrastValue!=1) ccg_geo_ibl_color_adj_contrast(data->contrastType, color, data->contrastValue, data->contrastCenter);
						break;
		case 3:	if(data->offset!=0) ccg_color_add_scalar(color, data->offset, color);
						if(data->contrastValue!=1) ccg_geo_ibl_color_adj_contrast(data->contrastType, color, data->contrastValue, data->contrastCenter);
						if(data->exposure!=1) ccg_color_multiply_scalar(color, data->exposure, color);
						break;
		case 4:	if(data->contrastValue!=1) ccg_geo_ibl_color_adj_contrast(data->contrastType, color, data->contrastValue, data->contrastCenter);
						if(data->exposure!=1) ccg_color_multiply_scalar(color, data->exposure, color);
						if(data->offset!=0) ccg_color_add_scalar(color, data->offset, color);
						break;
		case 5:	if(data->contrastValue!=1) ccg_geo_ibl_color_adj_contrast(data->contrastType, color, data->contrastValue, data->contrastCenter);	
						if(data->offset!=0) ccg_color_add_scalar(color, data->offset, color);
						if(data->exposure!=1) ccg_color_multiply_scalar(color, data->exposure, color);
						break;	
	}
	//multiply
	if(!(data->multiply.r==1 && data->multiply.g==1 && data->multiply.b==1))
		ccg_color_multiply(color, &data->multiply, color);
	//clamp
	if(data->clamp_low<data->clamp_hi)
	{
		color->r = color->r<=data->clamp_low?data->clamp_low:(color->r>=data->clamp_hi?data->clamp_hi:color->r);
		color->g = color->g<=data->clamp_low?data->clamp_low:(color->g>=data->clamp_hi?data->clamp_hi:color->g);
		color->b = color->b<=data->clamp_low?data->clamp_low:(color->b>=data->clamp_hi?data->clamp_hi:color->b);
	}
}

extern "C" DLLEXPORT int ccg_geo_ibl_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_geo_ibl(
	miTag						*result,
	miState					*state,
	struct ccg_geo_ibl *paras)
{
	miTag tex = *mi_eval_tag(&paras->texture);
	struct ccg_geo_ibl_pointsParam par;
	par.mode = *mi_eval_integer(&paras->light_mode);
			if(par.mode<0) par.mode=0;else if(par.mode>2) par.mode=2;
	miTag pointsfileTag = *mi_eval_tag(&paras->light_points_file);
	char *pointsfile = NULL;
	if(par.mode==2)
	{
		if(!pointsfileTag) 
		{
			mi_error("Please set the light_points_file name.");
			return miFALSE;
		}else {
				pointsfile = mi_mem_strdup((char*)mi_db_access(pointsfileTag));
				mi_db_unpin( pointsfileTag );
			}
	}
	par.half = *mi_eval_integer(&paras->half_sphere);
	float radiu = *mi_eval_scalar(&paras->radius);
	par.u = *mi_eval_integer(&paras->sampleU);
	par.v = *mi_eval_integer(&paras->sampleV);
	par.n = *mi_eval_integer(&paras->sampleNumber);
	par.number = par.mode?(par.n):((par.u-1)*(par.v-1));
		if(par.mode==2)	par.number = ccg_geo_ibl_getFileLinesNumber(pointsfile);
	par.iteration = *mi_eval_integer(&paras->iteration);
	par.seed = *mi_eval_integer(&paras->seed);
	par.center = *mi_eval_vector(&paras->center);
	
	if(tex==miNULLTAG || par.number<=0 ) return miTRUE;
	
	int		tex_change = *mi_eval_integer(&paras->tex_changeUV);
	miVector	tex_rot = *mi_eval_vector(&paras->tex_rotate);
	float	tex_rx	=	tex_rot.x;
	float	tex_ry	=	tex_rot.y;
	float	tex_rz	=	tex_rot.z;
	float in_sky = *mi_eval_scalar(&paras->intensity_sky);
	float in_ground = *mi_eval_scalar(&paras->intensity_ground);
	int shadow = *mi_eval_integer(&paras->shadow_mode);
	int half_shadow = *mi_eval_integer(&paras->half_shadow);
	miColor	shadowColor = *mi_eval_color(&paras->shadow_color);
	float	shadowFactor = *mi_eval_scalar(&paras->shadow_factor);
	ccg_color_multiply_scalar(&shadowColor, shadowFactor, &shadowColor);
	float min_in = *mi_eval_scalar(&paras->min_intensity);
	float max_in = *mi_eval_scalar(&paras->max_intensity);
	int		sm_resolution = *mi_eval_integer(&paras->shadowmap_resolution);
	float sm_softness	=		*mi_eval_scalar(&paras->shadowmap_softness);
	int 	sm_samples	=		*mi_eval_integer(&paras->shadowmap_samples);
	int		sm_format =		*mi_eval_integer(&paras->shadowmap_format);
	float	sm_bias =		*mi_eval_scalar(&paras->shadowmap_bias);
	float sm_accuracy =		*mi_eval_scalar(&paras->shadowmapdetail_accuracy);
	int 	sm_type =		*mi_eval_integer(&paras->shadowmapdetail_type);
	int		sm_n_shmap = *mi_eval_integer(&paras->n_shadowmap_file);
	int		sm_i_shmap = *mi_eval_integer(&paras->i_shadowmap_file);
	int i;
	
	//create points array
	miVector *points;
	miVector *direction;
	if(par.number>0)
	{
		points = (miVector *)mi_mem_allocate(par.number * sizeof(miVector));
		direction = (miVector *)mi_mem_allocate(par.number * sizeof(miVector));
		miVector tran_rot = *mi_eval_vector(&paras->rotate);
		ccg_geo_ibl_points_distribute(points, direction, &par, pointsfile, &tran_rot);
	}
	if(pointsfile!=NULL) mi_mem_release(pointsfile);
	//for(i=0;i<par.number;i++)
	//	mi_info("points %d: %f %f %f", i, points[i].x, points[i].y, points[i].z);
	mi_info("The total number of ccg_geo_ibl lights is %d", par.number );
		
	miInteger lightType = *mi_eval_integer(&paras->light_type);
	miBoolean	lightVisible = *mi_eval_boolean(&paras->visible);
	float	lightRadius = *mi_eval_scalar(&paras->light_radius);
	switch(lightType)
	{
		case 0:	if(!mi_api_name_lookup(mi_mem_strdup("maya_directionallight")))
				{
						miParameter* outColor = mi_api_parameter_decl(miTYPE_COLOR, NULL, NULL);
						miParameter* parm1 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("useLightPosition"), NULL);
						miParameter* parm2 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("lightAngle"), NULL);
						miParameter* parm3 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("useDepthMapShadows"), NULL);
						miParameter* parm4 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("emitDiffuse"), NULL);
						miParameter* parm5 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("emitSpecular"), NULL);
						miParameter* parm6 = mi_api_parameter_decl(miTYPE_COLOR, mi_mem_strdup("color"), NULL);
						miParameter* parm7 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("intensity"), NULL);
						miParameter* parm8 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("useRayTraceShadows"), NULL);
						miParameter* parm9 = mi_api_parameter_decl(miTYPE_COLOR, mi_mem_strdup("shadowColor"), NULL);
						miParameter* parm10 = mi_api_parameter_decl(miTYPE_INTEGER, mi_mem_strdup("shadowRays"), NULL);
						miParameter* parm11 = mi_api_parameter_decl(miTYPE_INTEGER, mi_mem_strdup("rayDepthLimit"), NULL);
						
						miParameter* append0 = mi_api_parameter_append( parm1, parm2 );
					      miParameter* append1 = mi_api_parameter_append( append0, parm3 );
					      miParameter* append2 = mi_api_parameter_append( append1, parm4 );
					      miParameter* append3 = mi_api_parameter_append( append2, parm5 );
					      miParameter* append4 = mi_api_parameter_append( append3, parm6 );
					      miParameter* append5 = mi_api_parameter_append( append4, parm7 );
					      miParameter* append6 = mi_api_parameter_append( append5, parm8 );
					      miParameter* append7 = mi_api_parameter_append( append6, parm9 );
					      miParameter* append8 = mi_api_parameter_append( append7, parm10 );
					      miParameter* appendList = mi_api_parameter_append( append8, parm11 );
					      
					      miFunction_decl * mayaLight = mi_api_funcdecl_begin(outColor, mi_mem_strdup("maya_directionallight"), appendList);  
					      mayaLight->version = 2;
					      mi_api_funcdecl_end();
						}
						break;
		case 1:	if(!mi_api_name_lookup(mi_mem_strdup("maya_spotlight")))
						{
								miParameter* outColor = mi_api_parameter_decl(miTYPE_COLOR, NULL, NULL);
								miParameter* parm1 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("coneAngle"), NULL);
								miParameter* parm2 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("penumbraAngle"), NULL);
								miParameter* parm3 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("dropoff"), NULL);
								miParameter* parm4 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("barnDoors"), NULL);
								miParameter* parm5 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("leftBarnDoor"), NULL);
								miParameter* parm6 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("rightBarnDoor"), NULL);
								miParameter* parm7 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("topBarnDoor"), NULL);
								miParameter* parm8 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("bottomBarnDoor"), NULL);
								miParameter* parm9 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("useDecayRegions"), NULL);
								miParameter* parm10 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("startDistance1"), NULL);
								miParameter* parm11 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("endDistance1"), NULL);
								miParameter* parm12 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("startDistance2"), NULL);
								miParameter* parm13 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("endDistance2"), NULL);
								miParameter* parm14 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("startDistance3"), NULL);
								miParameter* parm15 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("endDistance3"), NULL);
								miParameter* parm16 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("lightRadius"), NULL);
								miParameter* parm17 = mi_api_parameter_decl(miTYPE_INTEGER, mi_mem_strdup("decayRate"), NULL);
								miParameter* parm18 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("useDepthMapShadows"), NULL);
								miParameter* parm19 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("emitDiffuse"), NULL);
								miParameter* parm20 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("emitSpecular"), NULL);
								miParameter* parm21 = mi_api_parameter_decl(miTYPE_COLOR, mi_mem_strdup("color"), NULL);
								miParameter* parm22 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("intensity"), NULL);
								miParameter* parm23 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("useRayTraceShadows"), NULL);
								miParameter* parm24 = mi_api_parameter_decl(miTYPE_COLOR, mi_mem_strdup("shadowColor"), NULL);
								miParameter* parm25 = mi_api_parameter_decl(miTYPE_INTEGER, mi_mem_strdup("shadowRays"), NULL);
								miParameter* parm26 = mi_api_parameter_decl(miTYPE_INTEGER, mi_mem_strdup("rayDepthLimit"), NULL);
								miParameter* parm27 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("fogSpread"), NULL);
								miParameter* parm28 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("fogIntensity"), NULL);
								
								miParameter* append0 = mi_api_parameter_append( parm1, parm2 );
					      miParameter* append1 = mi_api_parameter_append( append0, parm3 );
					      miParameter* append2 = mi_api_parameter_append( append1, parm4 );
					      miParameter* append3 = mi_api_parameter_append( append2, parm5 );
					      miParameter* append4 = mi_api_parameter_append( append3, parm6 );
					      miParameter* append5 = mi_api_parameter_append( append4, parm7 );
					      miParameter* append6 = mi_api_parameter_append( append5, parm8 );
					      miParameter* append7 = mi_api_parameter_append( append6, parm9 );
					      miParameter* append8 = mi_api_parameter_append( append7, parm10 );
					      miParameter* append9 = mi_api_parameter_append( append8, parm11 );
					      miParameter* append10 = mi_api_parameter_append( append9, parm12 );
					      miParameter* append11 = mi_api_parameter_append( append10, parm13 );
					      miParameter* append12 = mi_api_parameter_append( append11, parm14 );
					      miParameter* append13 = mi_api_parameter_append( append12, parm15 );
					      miParameter* append14 = mi_api_parameter_append( append13, parm16 );
					      miParameter* append15 = mi_api_parameter_append( append14, parm17 );
					      miParameter* append16 = mi_api_parameter_append( append15, parm18 );
					      miParameter* append17 = mi_api_parameter_append( append16, parm19 );
					      miParameter* append18 = mi_api_parameter_append( append17, parm20 );
					      miParameter* append19 = mi_api_parameter_append( append18, parm21 );
					      miParameter* append20 = mi_api_parameter_append( append19, parm22 );
					      miParameter* append21 = mi_api_parameter_append( append20, parm23 );
					      miParameter* append22 = mi_api_parameter_append( append21, parm24 );
					      miParameter* append23 = mi_api_parameter_append( append22, parm25 );
					      miParameter* append24 = mi_api_parameter_append( append23, parm26 );
					      miParameter* append25 = mi_api_parameter_append( append24, parm27 );
					      miParameter* appendList = mi_api_parameter_append( append25, parm28 );
					      
					      miFunction_decl * mayaLight = mi_api_funcdecl_begin(outColor, mi_mem_strdup("maya_spotlight"), appendList);  
					      mayaLight->version = 4;
					      mi_api_funcdecl_end();
						}
						break;
		case 2: if(!mi_api_name_lookup(mi_mem_strdup("mib_light_infinite")))
						{
								miParameter* outColor = mi_api_parameter_decl(miTYPE_COLOR, NULL, NULL);
								miParameter* parm1 = mi_api_parameter_decl(miTYPE_COLOR, mi_mem_strdup("color"), NULL);
								miParameter* parm2 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("shadow"), NULL);
								miParameter* parm3 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("factor"), NULL);
	
								miParameter* append1 = mi_api_parameter_append( parm1, parm2 );
					      miParameter* appendList = mi_api_parameter_append( append1, parm3 );
					      
					      miFunction_decl * mayaLight = mi_api_funcdecl_begin(outColor, mi_mem_strdup("mib_light_infinite"), appendList);  
					      mayaLight->version = 1;
					      mi_api_funcdecl_end();
						}
						break;
		case 3: if(!mi_api_name_lookup(mi_mem_strdup("mib_light_spot")))
						{
								miParameter* outColor = mi_api_parameter_decl(miTYPE_COLOR, NULL, NULL);
								miParameter* parm1 = mi_api_parameter_decl(miTYPE_COLOR, mi_mem_strdup("color"), NULL);
								miParameter* parm2 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("shadow"), NULL);
								miParameter* parm3 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("factor"), NULL);
								miParameter* parm4 = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("atten"), NULL);
								miParameter* parm5 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("start"), NULL);
								miParameter* parm6 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("stop"), NULL);
								miParameter* parm7 = mi_api_parameter_decl(miTYPE_SCALAR, mi_mem_strdup("cone"), NULL);
	
								miParameter* append1 = mi_api_parameter_append( parm1, parm2 );
								miParameter* append2 = mi_api_parameter_append( append1, parm3 );
								miParameter* append3 = mi_api_parameter_append( append2, parm4 );
								miParameter* append4 = mi_api_parameter_append( append3, parm5 );
								miParameter* append5 = mi_api_parameter_append( append4, parm6 );
					      miParameter* appendList = mi_api_parameter_append( append5, parm7 );
					      
					      miFunction_decl * mayaLight = mi_api_funcdecl_begin(outColor, mi_mem_strdup("mib_light_spot"), appendList);  
					      mayaLight->version = 1;
					      mi_api_funcdecl_end();
						}
						break;
	}
	
	//get texture adjustment
	struct ccg_geo_ibl_coloradjParam coladj;
	coladj.gamma = *mi_eval_scalar(&paras->gamma);
	coladj.exposure = *mi_eval_scalar(&paras->exposure);
	coladj.offset = *mi_eval_scalar(&paras->offset);
	coladj.contrastType = *mi_eval_integer(&paras->contrastType);
	coladj.contrastValue = *mi_eval_scalar(&paras->contrastValue);
	coladj.contrastCenter = *mi_eval_scalar(&paras->contrastCenter);
	coladj.multiply = *mi_eval_color(&paras->multiply);
	coladj.clamp_low = *mi_eval_scalar(&paras->clamp_low);
	coladj.clamp_hi = *mi_eval_scalar(&paras->clamp_hi);
	coladj.correctOrder = *mi_eval_integer(&paras->correctOrder);
	
	//get lights properties
	struct ccg_geo_ibl_lightPropParam lightProp;
	switch(lightType)
	{
		case 0:	lightProp.md_emitDiffuse = *mi_eval_boolean(&paras->mayadir_emitDiffuse);
					lightProp.md_emitSpecular = *mi_eval_boolean(&paras->mayadir_emitSpecular);
					lightProp.md_lightAngle = *mi_eval_scalar(&paras->mayadir_lightAngle);
					lightProp.md_rayDepthLimit = *mi_eval_integer(&paras->mayadir_rayDepthLimit);
					lightProp.md_shadowRays = *mi_eval_integer(&paras->mayadir_shadowRays);
					lightProp.md_useDepthMapShadows = *mi_eval_boolean(&paras->mayadir_useDepthMapShadows);
					lightProp.md_useLightPosition = *mi_eval_boolean(&paras->mayadir_useLightPosition);
					lightProp.md_useRayTraceShadows = *mi_eval_boolean(&paras->mayadir_useRayTraceShadows);
					break;
		case 1:		lightProp.ms_barnDoors = *mi_eval_boolean(&paras->mayaspot_barnDoors);
					lightProp.ms_bottomBarnDoor = *mi_eval_scalar(&paras->mayaspot_bottomBarnDoor);
					lightProp.ms_coneAngle = (*mi_eval_scalar(&paras->mayaspot_coneAngle))/180.0f*float(CCG_PI);
					lightProp.ms_decayRate = *mi_eval_integer(&paras->mayaspot_decayRate);
					lightProp.ms_dropoff = *mi_eval_scalar(&paras->mayaspot_dropoff);
					lightProp.ms_emitDiffuse = *mi_eval_boolean(&paras->mayaspot_emitDiffuse);
					lightProp.ms_emitSpecular = *mi_eval_boolean(&paras->mayaspot_emitSpecular);
					lightProp.ms_endDistance1 = *mi_eval_scalar(&paras->mayaspot_endDistance1);
					lightProp.ms_endDistance2 = *mi_eval_scalar(&paras->mayaspot_endDistance2);
					lightProp.ms_endDistance3 = *mi_eval_scalar(&paras->mayaspot_endDistance3);
					lightProp.ms_fogIntensity = *mi_eval_scalar(&paras->mayaspot_fogIntensity);
					lightProp.ms_fogSpread = *mi_eval_scalar(&paras->mayaspot_fogSpread);
					lightProp.ms_leftBarnDoor = *mi_eval_scalar(&paras->mayaspot_leftBarnDoor);
					lightProp.ms_lightRadius = *mi_eval_scalar(&paras->mayaspot_lightRadius);
					lightProp.ms_penumbraAngle = (*mi_eval_scalar(&paras->mayaspot_penumbraAngle))/180.0f*float(CCG_PI);
					lightProp.ms_spread	= cos(lightProp.ms_coneAngle*0.5f+(lightProp.ms_penumbraAngle>0?lightProp.ms_penumbraAngle:0));
					if(lightProp.ms_spread<0) lightProp.ms_spread = 0;
					else if(lightProp.ms_spread>1) lightProp.ms_spread = 1;
					lightProp.ms_rayDepthLimit = *mi_eval_integer(&paras->mayaspot_rayDepthLimit);
					lightProp.ms_rightBarnDoor = *mi_eval_scalar(&paras->mayaspot_rightBarnDoor);
					lightProp.ms_shadowRays = *mi_eval_integer(&paras->mayaspot_shadowRays);
					lightProp.ms_startDistance1 = *mi_eval_scalar(&paras->mayaspot_startDistance1);
					lightProp.ms_startDistance2 = *mi_eval_scalar(&paras->mayaspot_startDistance2);
					lightProp.ms_startDistance3 = *mi_eval_scalar(&paras->mayaspot_startDistance3);
					lightProp.ms_topBarnDoor = *mi_eval_scalar(&paras->mayaspot_topBarnDoor);
					lightProp.ms_useDecayRegions = *mi_eval_boolean(&paras->mayaspot_useDecayRegions);
					lightProp.ms_useDepthMapShadows = *mi_eval_boolean(&paras->mayaspot_useDepthMapShadows);
					lightProp.ms_useRayTraceShadows = *mi_eval_boolean(&paras->mayaspot_useRayTraceShadows);
					break;
		case 2:		lightProp.rd_useLightPosition = *mi_eval_boolean(&paras->mrdir_useLightPosition);
					lightProp.rd_useDepthMapShadows = *mi_eval_boolean(&paras->mrdir_useDepthMapShadows);
					lightProp.rd_useRayTraceShadows = *mi_eval_boolean(&paras->mrdir_useRayTraceShadows);
					break;
		case 3:		lightProp.rs_useDepthMapShadows = *mi_eval_boolean(&paras->mrspot_useDepthMapShadows);
					lightProp.rs_useRayTraceShadows = *mi_eval_boolean(&paras->mrspot_useRayTraceShadows);
					lightProp.rs_atten = *mi_eval_boolean(&paras->mrspot_atten);
					lightProp.rs_start = *mi_eval_scalar(&paras->mrspot_start);
					lightProp.rs_stop = *mi_eval_scalar(&paras->mrspot_stop);
					lightProp.rs_coneAngle = (*mi_eval_scalar(&paras->mrspot_coneAngle))/180.0f*float(CCG_PI);
					lightProp.rs_penumbraAngle = (*mi_eval_scalar(&paras->mrspot_penumbraAngle))/180.0f*float(CCG_PI);
					lightProp.rs_outCosine = cos((lightProp.rs_coneAngle*0.5f+(lightProp.rs_penumbraAngle>0?lightProp.rs_penumbraAngle:0)));
					if(lightProp.rs_outCosine<0) lightProp.rs_outCosine=0;
					else if(lightProp.rs_outCosine>1) lightProp.rs_outCosine = 1;
					lightProp.rs_innerCosine = cos((lightProp.rs_coneAngle*0.5f+(lightProp.rs_penumbraAngle<0?lightProp.rs_penumbraAngle:0)));
					if(lightProp.rs_innerCosine<0) lightProp.rs_innerCosine=0;
					else if(lightProp.rs_outCosine>1) lightProp.rs_outCosine = 1;
					break;
	}
	
	//begin light instances
	mi_api_instgroup_begin(mi_mem_strdup("ccgLightArrayGrp"));
	for(i=0;i<par.number;i++)
	{
			char *lightname = (char *)mi_mem_allocate(sizeof(char)*24);
			char *lightinst	=	(char *)mi_mem_allocate(sizeof(char)*29);
			sprintf(lightname, "_ccgLightArray_%d_%d", i, par.number);
			sprintf(lightinst, "_ccgLightArray_Inst_%d_%d", i, par.number);
			
			//generate u and v for texture lookup
			miMatrix rotMatrix;
			miVector rotV;
			if(!(tex_rx==0 && tex_ry==0 && tex_rz==0))
			{
				mi_matrix_rotate(rotMatrix, tex_rx/360.0f*float(CCG_PIx2), tex_ry/360.0f*float(CCG_PIx2), tex_rz/360.0f*float(CCG_PIx2));
				mi_vector_transform_T(&rotV, &points[i], rotMatrix);
			}else ccg_vector_assign(&rotV, &points[i]);
			float fu = float(acos(-rotV.y)/CCG_PI);
			float fv;
			if(rotV.x==0 && rotV.z==0) fv = 0;
			else if(rotV.x==0)
						{
							if(rotV.z>0) fv = 0.25f;
							else fv = 0.75f;
						}else if(rotV.z==0)
									{
										if(rotV.x>0) fv = 0;
										else fv = 0.5f;
									}else {
											float atanV = float(atan2(rotV.z, rotV.x));
											if(atanV>=0) fv = atanV/float(CCG_PIx2);
											else fv = float(CCG_PIx2+atanV)/float(CCG_PIx2);
										}
			switch(tex_change)
			{
				case 0:	//no change
								break;
				case 1:	//reverse u
								fu = 1 - fu;
								break;
				case 2:	//reverse v
								fv = 1 - fv;
								break;
				case 3:	//reverse u&v
								fu = 1 - fu;
								fv = 1 - fv;
								break;
				case 4: //swap uv
								float swap = fv;
								fv = fu;
								fu = swap;
								break;
			}
			
			miInteger ivalue = 0;
			miBoolean bvalue = miFALSE;
			miBoolean bvalue_ray = miFALSE;
			miBoolean bvalue_map = miFALSE;
			float			fvalue = 0;
			miColor		texColor, lumi;
			ccg_color_init(&texColor, 0);
			ccg_color_init(&lumi, 0);
			miVector	coord, pointsDir, internalDir, objDir;
			ccg_vector_init(&coord, 0);
			ccg_vector_init(&pointsDir, 0);
			ccg_vector_init(&internalDir, 0);
			ccg_vector_init(&objDir, 0);
			miTag  lightshader = miNULLTAG;
			miLight *light = NULL;
			char *shmap_file = NULL;
			miTag sm_file = miNULLTAG;
			
			switch(lightType)
			{
				case 0:	light = mi_api_light_begin(mi_mem_strdup(lightname));
								light->type = miLIGHT_DIRECTION;
								light->area = miLIGHT_NONE;
								mi_api_function_call(mi_mem_strdup("maya_directionallight"));
								coord.x = fu; coord.y = fv; coord.z = 0;
								mi_lookup_color_texture(&texColor, state, tex, &coord);
								ccg_geo_ibl_color_adj(&texColor, &coladj);
								ccg_rgb2luminance(&texColor, &lumi);
								fvalue = texColor.r;
								mi_api_parameter_name(mi_mem_strdup("color"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = texColor.g;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = texColor.b;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								if(points[i].y>=0) fvalue = in_sky;
								else fvalue = in_ground;
								mi_api_parameter_name(mi_mem_strdup("intensity"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								bvalue = lightProp.md_emitDiffuse;
								mi_api_parameter_name(mi_mem_strdup("emitDiffuse"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								bvalue = lightProp.md_emitSpecular;
								mi_api_parameter_name(mi_mem_strdup("emitSpecular"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								if(shadow==0){
									bvalue = 0;
								}else if(shadow==1){
												if(half_shadow==0) bvalue = 1;
												else if(half_shadow==1) bvalue = direction[i].y<=0?1:0;
															else if(half_shadow==2) bvalue = direction[i].y>=0?1:0;
											}else if(shadow==2){
															if(lumi.a>=min_in && lumi.a<=max_in){
																if(half_shadow==0) bvalue = 1;
																else if(half_shadow==1) bvalue = direction[i].y<=0?1:0;
																			else if(half_shadow==2) bvalue = direction[i].y>=0?1:0;
															}else bvalue = 0;
														}
								if(bvalue)
								{
									bvalue_ray = lightProp.md_useRayTraceShadows;
									if(bvalue_ray) {bvalue_map = 0; lightProp.md_useDepthMapShadows = 0;}
									else {bvalue_map = 1; lightProp.md_useDepthMapShadows = 1;}
								}else {
												bvalue_ray = 0;
												bvalue_map = 0;
											}
								mi_api_parameter_name(mi_mem_strdup("useRayTraceShadows"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue_ray,0,0);
								mi_api_parameter_name(mi_mem_strdup("useDepthMapShadows"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue_map,0,0);
								bvalue = lightProp.md_useLightPosition;
								mi_api_parameter_name(mi_mem_strdup("useLightPosition"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								ivalue = lightProp.md_shadowRays;
								mi_api_parameter_name(mi_mem_strdup("shadowRays"));
								mi_api_parameter_value(miTYPE_INTEGER,&ivalue,0,0);
								ivalue = lightProp.md_rayDepthLimit;
								mi_api_parameter_name(mi_mem_strdup("rayDepthLimit"));
								mi_api_parameter_value(miTYPE_INTEGER,&ivalue,0,0);
								fvalue = lightProp.md_lightAngle;
								mi_api_parameter_name(mi_mem_strdup("lightAngle"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = shadowColor.r;
								mi_api_parameter_name(mi_mem_strdup("shadowColor"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = shadowColor.g;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = shadowColor.b;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								lightshader = mi_api_function_call_end(0);
								//light shader
								light->shader = lightshader;
								//light direction
								pointsDir = direction[i];
								mi_vector_from_world(state, &internalDir, &pointsDir);
								mi_vector_to_object(state, &objDir, &internalDir);
								light->direction = objDir;
								//light position
								if(lightProp.md_useLightPosition) {
									//light->origin.x = light->origin.y = light->origin.z = 0;
									pointsDir = points[i];
									mi_vector_mul(&pointsDir, radiu);
									mi_vector_from_world(state, &internalDir, &pointsDir);
									mi_vector_to_object(state, &objDir, &internalDir);
									light->origin = objDir;
									//infinite light with org?
									light->dirlight_has_org = (miUint1)lightProp.md_useLightPosition;
								}
								//light spread
								//shadow map setting
  								if(bvalue_map)
  								{
	  								light->shadowmap_flags = sm_format?(miSHADOWMAP_DETAIL):(miSHADOWMAP_MERGE);
	  								light->use_shadow_maps = miTRUE;
	  								if(i<sm_n_shmap) sm_file = *mi_eval_tag(paras->shadowmap_file + sm_i_shmap + i);
	  								if(sm_file) light->shadowmap_file = sm_file;
	  								sm_file = miNULLTAG;
	  								light->shadowmap_resolution = sm_resolution;
	  								light->shadowmap_softness = sm_softness;
	  								light->shadowmap_samples = sm_samples;
	  								light->shadowmap_bias = sm_bias;
	  								if(sm_format==1)
	  								{
		  								light->shmap.accuracy = sm_accuracy;
		  								light->shmap.samples = sm_samples;
		  								//light->shmap.filter = 'b';
		  								light->shmap.type = sm_type?'a':'c';
	  								}
  								}
								mi_api_light_end();
								break;
				case 1: light = mi_api_light_begin(mi_mem_strdup(lightname));
								light->type = miLIGHT_SPOT;
								if(!lightVisible) light->area = miLIGHT_NONE;
								else {
										light->area = miLIGHT_SPHERE;
										light->visible = miTRUE;
										light->primitive.sphere.radius = lightRadius;
										light->samples_u = 0;
										light->samples_v = 0;
									}
								mi_api_function_call(mi_mem_strdup("maya_spotlight"));
								coord.x = fu; coord.y = fv; coord.z = 0;
								mi_lookup_color_texture(&texColor, state, tex, &coord);
								ccg_geo_ibl_color_adj(&texColor, &coladj);
								ccg_rgb2luminance(&texColor, &lumi);
								fvalue = texColor.r;
								mi_api_parameter_name(mi_mem_strdup("color"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = texColor.g;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = texColor.b;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								if(points[i].y>=0) fvalue = in_sky;
								else fvalue = in_ground;
								mi_api_parameter_name(mi_mem_strdup("intensity"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								bvalue = lightProp.ms_emitDiffuse;
								mi_api_parameter_name(mi_mem_strdup("emitDiffuse"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								bvalue = lightProp.ms_emitSpecular;
								mi_api_parameter_name(mi_mem_strdup("emitSpecular"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								if(shadow==0){
									bvalue = 0;
								}else if(shadow==1){
												if(half_shadow==0) bvalue = 1;
												else if(half_shadow==1) bvalue = direction[i].y<=0?1:0;
															else if(half_shadow==2) bvalue = direction[i].y>=0?1:0;
											}else if(shadow==2){
															if(lumi.a>=min_in && lumi.a<=max_in){
																if(half_shadow==0) bvalue = 1;
																else if(half_shadow==1) bvalue = direction[i].y<=0?1:0;
																			else if(half_shadow==2) bvalue = direction[i].y>=0?1:0;
															}else bvalue = 0;
														}
								if(bvalue)
								{
									bvalue_ray = lightProp.ms_useRayTraceShadows;
									if(bvalue_ray) {bvalue_map = 0; lightProp.ms_useDepthMapShadows = 0;}
									else {bvalue_map = 1; lightProp.ms_useDepthMapShadows = 1;}
								}else {
												bvalue_ray = 0;
												bvalue_map = 0;
											}
								mi_api_parameter_name(mi_mem_strdup("useRayTraceShadows"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue_ray,0,0);
								mi_api_parameter_name(mi_mem_strdup("useDepthMapShadows"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue_map,0,0);
								ivalue = lightProp.ms_shadowRays;
								mi_api_parameter_name(mi_mem_strdup("shadowRays"));
								mi_api_parameter_value(miTYPE_INTEGER,&ivalue,0,0);
								ivalue = lightProp.ms_rayDepthLimit;
								mi_api_parameter_name(mi_mem_strdup("rayDepthLimit"));
								mi_api_parameter_value(miTYPE_INTEGER,&ivalue,0,0);
								fvalue = shadowColor.r;
								mi_api_parameter_name(mi_mem_strdup("shadowColor"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = shadowColor.g;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = shadowColor.b;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_coneAngle;
								mi_api_parameter_name(mi_mem_strdup("coneAngle"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_penumbraAngle;
								mi_api_parameter_name(mi_mem_strdup("penumbraAngle"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_dropoff;
								mi_api_parameter_name(mi_mem_strdup("dropoff"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								bvalue = lightProp.ms_barnDoors;
								mi_api_parameter_name(mi_mem_strdup("barnDoors"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								fvalue = lightProp.ms_leftBarnDoor;
								mi_api_parameter_name(mi_mem_strdup("leftBarnDoor"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_rightBarnDoor;
								mi_api_parameter_name(mi_mem_strdup("rightBarnDoor"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_topBarnDoor;
								mi_api_parameter_name(mi_mem_strdup("topBarnDoor"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_bottomBarnDoor;
								mi_api_parameter_name(mi_mem_strdup("bottomBarnDoor"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								bvalue = lightProp.ms_useDecayRegions;
								mi_api_parameter_name(mi_mem_strdup("useDecayRegions"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								fvalue = lightProp.ms_startDistance1;
								mi_api_parameter_name(mi_mem_strdup("startDistance1"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_endDistance1;
								mi_api_parameter_name(mi_mem_strdup("endDistance1"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_startDistance2;
								mi_api_parameter_name(mi_mem_strdup("startDistance2"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_endDistance2;
								mi_api_parameter_name(mi_mem_strdup("endDistance2"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_startDistance3;
								mi_api_parameter_name(mi_mem_strdup("startDistance3"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_endDistance3;
								mi_api_parameter_name(mi_mem_strdup("endDistance3"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_lightRadius;
								mi_api_parameter_name(mi_mem_strdup("lightRadius"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								ivalue = lightProp.ms_decayRate;
								mi_api_parameter_name(mi_mem_strdup("decayRate"));
								mi_api_parameter_value(miTYPE_INTEGER,&ivalue,0,0);
								fvalue = lightProp.ms_fogSpread;
								mi_api_parameter_name(mi_mem_strdup("fogSpread"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.ms_fogIntensity;
								mi_api_parameter_name(mi_mem_strdup("fogIntensity"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								lightshader = mi_api_function_call_end(0);
								//light shader
								light->shader = lightshader;
								//light direction
								pointsDir = direction[i];
								mi_vector_from_world(state, &internalDir, &pointsDir);
								mi_vector_to_object(state, &objDir, &internalDir);
								light->direction = objDir;
								//light->direction.x = light->direction.y = 0; light->direction.z = -1;
								//light position
								//light->origin.x = light->origin.y = light->origin.z = 0;
								pointsDir = points[i];
								mi_vector_mul(&pointsDir, radiu);
								mi_vector_from_world(state, &internalDir, &pointsDir);
								mi_vector_to_object(state, &objDir, &internalDir);
								light->origin = objDir;
								//light spread
								light->spread = lightProp.ms_spread;
								mi_info("spread is: %f", lightProp.ms_spread);
								//shadow map setting
  							if(bvalue_map)
  							{
	  							light->shadowmap_flags = sm_format?(miSHADOWMAP_DETAIL):(miSHADOWMAP_MERGE);
	  							light->use_shadow_maps = miTRUE;
	  							if(i<sm_n_shmap) sm_file = *mi_eval_tag(paras->shadowmap_file + sm_i_shmap + i);
	  							if(sm_file) light->shadowmap_file = sm_file;
	  							sm_file = miNULLTAG;
	  							light->shadowmap_resolution = sm_resolution;
	  							light->shadowmap_softness = sm_softness;
	  							light->shadowmap_samples = sm_samples;
	  							light->shadowmap_bias = sm_bias;
	  							if(sm_format==1)
	  							{
		  							light->shmap.accuracy = sm_accuracy;
		  							light->shmap.samples = sm_samples;
		  							//light->shmap.filter = 'b';
		  							light->shmap.type = sm_type?'a':'c';
	  							}
  							}
								mi_api_light_end();
								break;
				case 2: light = mi_api_light_begin(mi_mem_strdup(lightname));
								light->type = miLIGHT_DIRECTION;
								light->area = miLIGHT_NONE;
								mi_api_function_call(mi_mem_strdup("mib_light_infinite"));
								coord.x = fu; coord.y = fv; coord.z = 0;
								mi_lookup_color_texture(&texColor, state, tex, &coord);
								ccg_geo_ibl_color_adj(&texColor, &coladj);
								ccg_rgb2luminance(&texColor, &lumi);
								if(points[i].y>=0) fvalue = in_sky;
								else fvalue = in_ground;
								ccg_color_multiply_scalar(&texColor, fvalue, &texColor);
								fvalue = texColor.r;
								mi_api_parameter_name(mi_mem_strdup("color"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = texColor.g;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = texColor.b;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								if(shadow==0){
									bvalue = 0;
								}else if(shadow==1){
												if(half_shadow==0) bvalue = 1;
												else if(half_shadow==1) bvalue = direction[i].y<=0?1:0;
															else if(half_shadow==2) bvalue = direction[i].y>=0?1:0;
											}else if(shadow==2){
															if(lumi.a>=min_in && lumi.a<=max_in){
																if(half_shadow==0) bvalue = 1;
																else if(half_shadow==1) bvalue = direction[i].y<=0?1:0;
																			else if(half_shadow==2) bvalue = direction[i].y>=0?1:0;
															}else bvalue = 0;
														}
								if(bvalue)
								{
									bvalue_ray = lightProp.rd_useRayTraceShadows;
									if(bvalue_ray) {bvalue_map = 0; lightProp.rd_useDepthMapShadows = 0;}
									else {bvalue_map = 1; lightProp.rd_useDepthMapShadows = 1;}
								}else {
												bvalue_ray = 0;
												bvalue_map = 0;
											}
								mi_api_parameter_name(mi_mem_strdup("shadow"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								fvalue = shadowColor.r*0.3f + shadowColor.g*0.59f + shadowColor.b*0.11f;
								mi_api_parameter_name(mi_mem_strdup("factor"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								lightshader = mi_api_function_call_end(0);
								//light shader
								light->shader = lightshader;
								//light direction
								pointsDir = direction[i];
								mi_vector_from_world(state, &internalDir, &pointsDir);
								mi_vector_to_object(state, &objDir, &internalDir);
								light->direction = objDir;
								//light position
								if(lightProp.rd_useLightPosition) {
									//light->origin.x = light->origin.y = light->origin.z = 0;
									pointsDir = points[i];
									mi_vector_mul(&pointsDir, radiu);
									mi_vector_from_world(state, &internalDir, &pointsDir);
									mi_vector_to_object(state, &objDir, &internalDir);
									light->origin = objDir;
									//infinite light with org?
									light->dirlight_has_org = (miUint1)lightProp.rd_useLightPosition;
								}
								//light->origin.x = light->origin.y = light->origin.z = 0;
								//light spread
								//shadow map setting
  								if(bvalue_map)
  								{
	  								light->shadowmap_flags = sm_format?(miSHADOWMAP_DETAIL):(miSHADOWMAP_MERGE);
	  								light->use_shadow_maps = miTRUE;
	  								if(i<sm_n_shmap) sm_file = *mi_eval_tag(paras->shadowmap_file + sm_i_shmap + i);
	  								if(sm_file) light->shadowmap_file = sm_file;
	  								sm_file = miNULLTAG;
	  								light->shadowmap_resolution = sm_resolution;
	  								light->shadowmap_softness = sm_softness;
	  								light->shadowmap_samples = sm_samples;
	  								light->shadowmap_bias = sm_bias;
	  								if(sm_format==1)
	  								{
		  								light->shmap.accuracy = sm_accuracy;
		  								light->shmap.samples = sm_samples;
		  								//light->shmap.filter = 'b';
		  								light->shmap.type = sm_type?'a':'c';
	  								}
  								}
								mi_api_light_end();
								break;
				case 3: light = mi_api_light_begin(mi_mem_strdup(lightname));
								light->type = miLIGHT_SPOT;
								if(!lightVisible) light->area = miLIGHT_NONE;
								else {
										light->area = miLIGHT_SPHERE;
										light->visible = miTRUE;
										light->primitive.sphere.radius = lightRadius;
										light->samples_u = 0;
										light->samples_v = 0;
									}
								mi_api_function_call(mi_mem_strdup("mib_light_spot"));
								coord.x = fu; coord.y = fv; coord.z = 0;
								mi_lookup_color_texture(&texColor, state, tex, &coord);
								ccg_geo_ibl_color_adj(&texColor, &coladj);
								ccg_rgb2luminance(&texColor, &lumi);
								if(points[i].y>=0) fvalue = in_sky;
								else fvalue = in_ground;
								ccg_color_multiply_scalar(&texColor, fvalue, &texColor);
								fvalue = texColor.r;
								mi_api_parameter_name(mi_mem_strdup("color"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = texColor.g;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = texColor.b;
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								if(shadow==0){
									bvalue = 0;
								}else if(shadow==1){
												if(half_shadow==0) bvalue = 1;
												else if(half_shadow==1) bvalue = direction[i].y<=0?1:0;
															else if(half_shadow==2) bvalue = direction[i].y>=0?1:0;
											}else if(shadow==2){
															if(lumi.a>=min_in && lumi.a<=max_in){
																if(half_shadow==0) bvalue = 1;
																else if(half_shadow==1) bvalue = direction[i].y<=0?1:0;
																			else if(half_shadow==2) bvalue = direction[i].y>=0?1:0;
															}else bvalue = 0;
														}
								if(bvalue)
								{
									bvalue_ray = lightProp.rs_useRayTraceShadows;
									if(bvalue_ray) {bvalue_map = 0; lightProp.rs_useDepthMapShadows = 0;}
									else {bvalue_map = 1; lightProp.rs_useDepthMapShadows = 1;}
								}else {
												bvalue_ray = 0;
												bvalue_map = 0;
											}
								mi_api_parameter_name(mi_mem_strdup("shadow"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								fvalue = shadowColor.r*0.3f + shadowColor.g*0.59f + shadowColor.b*0.11f;
								mi_api_parameter_name(mi_mem_strdup("factor"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.rs_innerCosine;
								mi_api_parameter_name(mi_mem_strdup("cone"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								bvalue = lightProp.rs_atten;
								mi_api_parameter_name(mi_mem_strdup("atten"));
								mi_api_parameter_value(miTYPE_BOOLEAN,&bvalue,0,0);
								fvalue = lightProp.rs_start;
								mi_api_parameter_name(mi_mem_strdup("start"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								fvalue = lightProp.rs_stop;
								mi_api_parameter_name(mi_mem_strdup("stop"));
								mi_api_parameter_value(miTYPE_SCALAR,&fvalue,0,0);
								lightshader = mi_api_function_call_end(0);
								//light shader
								light->shader = lightshader;
								//light direction
								pointsDir = direction[i];
								mi_vector_from_world(state, &internalDir, &pointsDir);
								mi_vector_to_object(state, &objDir, &internalDir);
								light->direction = objDir;
								//light position
								//light->origin.x = light->origin.y = light->origin.z = 0;
								pointsDir = points[i];
								mi_vector_mul(&pointsDir, radiu);
								mi_vector_from_world(state, &internalDir, &pointsDir);
								mi_vector_to_object(state, &objDir, &internalDir);
								light->origin = objDir;
								//light spread
								light->spread = lightProp.rs_outCosine;
								//shadow map setting
  								if(bvalue_map)
  								{
	  								light->shadowmap_flags = sm_format?(miSHADOWMAP_DETAIL):(miSHADOWMAP_MERGE);
	  								light->use_shadow_maps = miTRUE;
	  								if(i<sm_n_shmap) sm_file = *mi_eval_tag(paras->shadowmap_file + sm_i_shmap + i);
	  								if(sm_file) light->shadowmap_file = sm_file;
	  								sm_file = miNULLTAG;
	  								light->shadowmap_resolution = sm_resolution;
	  								light->shadowmap_softness = sm_softness;
	  								light->shadowmap_samples = sm_samples;
	  								light->shadowmap_bias = sm_bias;
	  								if(sm_format==1)
	  								{
		  								light->shmap.accuracy = sm_accuracy;
		  								light->shmap.samples = sm_samples;
		  								//light->shmap.filter = 'b';
		  								light->shmap.type = sm_type?'a':'c';
	  								}
  								}
								mi_api_light_end();
								break;
			}
			
			//miInstance *lightInstance = 
			mi_api_instance_begin(mi_mem_strdup(lightinst));
			//if(lightInstance)
			//{
			//	pointsDir = points[i];
			//	mi_vector_neg(&pointsDir);
			//	mi_vector_mul(&pointsDir, radiu);
			//	mi_vector_from_world(state, &internalDir, &pointsDir);
			//	rotMatrix[12] = internalDir.x;
			//	rotMatrix[13] = internalDir.y;
			//	rotMatrix[14] = internalDir.z;
			//	mi_matrix_copy(lightInstance->tf.global_to_local, rotMatrix);
			//	mi_matrix_invert(lightInstance->tf.local_to_global, lightInstance->tf.global_to_local);
			//	//mi_info("matrix: %f %f %f %f, %f %f %f %f, %f %f %f %f, %f %f %f %f\n",rotMatrix[0],rotMatrix[1],rotMatrix[2],rotMatrix[3],rotMatrix[4],rotMatrix[5],rotMatrix[6],rotMatrix[7],rotMatrix[8],rotMatrix[9],rotMatrix[10],rotMatrix[11],rotMatrix[12],rotMatrix[13],rotMatrix[14],rotMatrix[15]);
			//}
			mi_api_instance_end(mi_mem_strdup(lightname), miNULLTAG, miNULLTAG);
			mi_api_instgroup_additem(mi_mem_strdup(lightinst));
			mi_mem_release(lightname);
			mi_mem_release(lightinst);
	}
	
	mi_geoshader_add_result(result, mi_api_instgroup_end() );
	
	//delete points data
	if(points!=NULL) mi_mem_release(points);
	if(direction!=NULL) mi_mem_release(direction);

	return(miTRUE);
}