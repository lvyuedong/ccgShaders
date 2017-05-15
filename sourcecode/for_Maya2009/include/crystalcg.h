
#ifndef CRYSTALCG_H
#define CRYSTALCG_H

//0 preserved
#define LAYER_combined 1
#define LAYER_col 2		//color
#define LAYER_diff 3	//diffuse
#define LAYER_ambi 4	//ambient
#define LAYER_spec 5	//specular
#define LAYER_incan 6	//incandescence
#define LAYER_refl 7	//reflection
#define LAYER_refr 8	//refraction
#define LAYER_z 9		//depth
#define LAYER_nor 10	//normal
#define LAYER_ao 11		//ambient occlution
#define LAYER_shad 12	//shadow
#define LAYER_vec 13	//motion vectors
#define LAYER_uv 14		//uv texture space
#define LAYER_oid 15	//object id
#define LAYER_mid 16	//material id
#define LAYER_reflao 17	//reflection ao
#define LAYER_refrao 18 //refraction ao
#define LAYER_globillum 19 //global illumination
#define LAYER_translucent 20 //translucence
#define LAYER_sssfront 21	//sss front
#define LAYER_sssmiddle 22	//sss middle
#define LAYER_sssback 23	//sss back

#define LAYER_string_max_length 12

#define LAYER_NUM 25	//the total numbers of passes

#define LAYER_RAY 26
#define LAYER_withoutShadow 27

#define CCG_E        2.71828182845904523536
#define CCG_LOG2E    1.44269504088896340736
#define CCG_LOG10E   0.434294481903251827651
#define CCG_LN2      0.693147180559945309417
#define CCG_LN10     2.30258509299404568402
#define CCG_PI       3.14159265358979323846
#define CCG_PI_2     1.57079632679489661923
#define CCG_PIx2     6.283185307179586476925
#define CCG_PI_4     0.785398163397448309616
#define CCG_1_PI     0.318309886183790671538
#define CCG_2_PI     0.636619772367581343076
#define CCG_2_SQRTPI 1.12837916709551257390
#define CCG_SQRT2    1.41421356237309504880
#define CCG_SQRT1_2  0.707106781186547524401

//struct ccgIBLLightArray{
//	miTag *lightTag;
//	miTag number;
//	miScalar angle;
//	miTag light_type;
//};

struct ccg_passfbArray {
	int passfbArray[LAYER_NUM+1];
	miTag *lightTag;
	miTag number;
	miScalar angle;
	miTag light_type;
};

#define CCG_SCOPE_NONE 0
#define CCG_SCOPE_DISABLE_PASS 1
#define CCG_SCOPE_ENABLE_PASS 2

struct ccg_raystate{
	int raystate;
	int scope;
};

struct ccg_multipasses_structure {
	miColor		combined;
	miColor		col;
	miColor		diff;
	miColor		ambi;
	miColor		spec;
	miColor		incan;
	miColor		refl;
	miColor		refr;
	miColor		z;
	miColor		nor;
	miColor		ao;
	miColor		shad;
	//miColor		vec;
	//miColor		uv;
	//miColor		oid;
	//miColor		mid;
	miColor		reflao;
	//miScalar	refrao;
	miColor		globillum;
	//miColor		translucent;
	miColor		sssfront;
	miColor		sssmiddle;
	miColor		sssback;
};

//const miColor	ccg_solid_black = {0,0,0,1};
//const miColor	ccg_trans_black = {0,0,0,0};
#define ccg_state_swap(s,c,o) do{(o) = (s);\
						(c) = *(o);\
						(s) = &(c);}while(0)
#define ccg_state_restore(s,o) ((s) = (miState *)(o))

#define ccg_clamp(x,min,max) ( (x)<(max)?((x)<=(min)?(min):(x)):(max) )

/*************************************************************/
/********************** color operation **********************/
/*************************************************************/
#define ccg_color_init(x,y) ((x)->r = (x)->g = (x)->b = (x)->a = (y))

#define ccg_color_assign(x,y) ((x)->r = (y)->r,\
								(x)->g = (y)->g,\
								(x)->b = (y)->b,\
								(x)->a = (y)->a)

#define ccg_rgb2luminance(x,y) do{(y)->r = (x)->r * 0.3f;\
								(y)->g = (x)->g * 0.59f;\
								(y)->b = (x)->b * 0.11f;\
								(y)->a = (y)->r + (y)->g + (y)->b;}while(0)

#define ccg_color_compare(x,y) ((x)->r==(y)->r &&\
								(x)->g==(y)->g &&\
								(x)->b==(y)->b)

#define ccg_color_add(x,y,z) ((z)->r = (x)->r + (y)->r,\
								(z)->g = (x)->g + (y)->g,\
								(z)->b = (x)->b + (y)->b,\
								(z)->a = (x)->a + (y)->a)

#define ccg_color_add_scalar(x,y,z) ((z)->r = (x)->r + (y),\
								(z)->g = (x)->g + (y),\
								(z)->b = (x)->b + (y) )

#define ccg_color_multiply(x,y,z) ((z)->r = (x)->r * (y)->r,\
									(z)->g = (x)->g * (y)->g,\
									(z)->b = (x)->b * (y)->b,\
									(z)->a = (x)->a * (y)->a)

#define ccg_color_multiply_scalar(x,y,z)	((z)->r = (x)->r * (y),\
											(z)->g = (x)->g * (y),\
											(z)->b = (x)->b * (y))

#define ccg_color_divide(x,y,z) ((z)->r = ((y)->r==0?0:((x)->r/(y)->r)),\
								 (z)->g = ((y)->g==0?0:((x)->g/(y)->g)),\
								 (z)->b = ((y)->b==0?0:((x)->b/(y)->b)),\
								 (z)->a = (x)->a)

#define ccg_color_sub(x,y,z) ((z)->r = (x)->r - (y)->r,\
							(z)->g = (x)->g - (y)->g,\
							(z)->b = (x)->b - (y)->b,\
							(z)->a = (x)->a - (y)->a)

#define ccg_color_inverse(x,y) ((y)->r = 1 - (x)->r,\
								(y)->g = 1 - (x)->g,\
								(y)->b = 1 - (x)->b,\
								(y)->a = 1 - (x)->a)

#define ccg_color_gamma(x,y,z) ( (z)->r = pow((x)->r, (1/(y==0?1:y))),\
								(z)->g = pow((x)->g, (1/(y==0?1:y))),\
								(z)->b = pow((x)->b, (1/(y==0?1:y)))  )

/*************************************************************/
/********************** vector operation *********************/
/*************************************************************/
#define ccg_vector_init(a,b) ((a)->x = (a)->y = (a)->z = (b))

#define ccg_vector_assign(a,b) ((a)->x = (b)->x,\
								(a)->y = (b)->y,\
								(a)->z = (b)->z)



/*************************************************************/
/******************** composite operation ********************/
/*************************************************************/
#define ccg_screen_comp(x,y,z)  ((z)->r = (1-(x)->r)*((y)->r) + (x)->r,\
								 (z)->g = (1-(x)->g)*((y)->g) + (x)->g,\
								 (z)->b = (1-(x)->b)*((y)->b) + (x)->b)

#define ccg_over_comp(x,y,z) ((z)->r = ((x)->r)*((x)->a) + ((y)->r)*(1-(x)->a),\
							  (z)->g = ((x)->g)*((x)->a) + ((y)->g)*(1-(x)->a),\
							  (z)->b = ((x)->b)*((x)->a) + ((y)->b)*(1-(x)->a))


#define ccg_is_eval_connected( _p_ ) (!state->shader->ghost_offs ? miFALSE : \
    (!*((miTag*)((char*)(_p_)+state->shader->ghost_offs)) ? miFALSE : miTRUE ) )

#endif	//CRYSTALCG_H
