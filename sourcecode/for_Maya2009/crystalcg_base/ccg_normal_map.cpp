/******************************************************************************
 * Author: lvyuedong
 * Contact: lvyuedong@hotmail.com
 * Created:	18.10.07
 * Purpose:	custom shaders for normal mapping
 *
 * Exports:
 *	ccg_normal_map
 *	ccg_passthrough_normal_map
 *
 * Description:
 * usage almost be same as mib_bump_map. the parameter "space" read 0 for object space, 1 for tangent space, 2 for world space
 *****************************************************************************/
#include "shader.h"

/*------------------------------------------ ccg_normal_map -------------------*/
void ccg_normal_map_getTangentUV(
	miVector t1,
	miVector t2,
	miVector t3,
	miVector p1,
	miVector p2,
	miVector p3,
	miVector *out_u,
	miVector *out_v
	)
{
	miVector	vc,vcA,vcB;
	miScalar	fu21,fv21,fu31,fv31;

	fu21 = t2.x - t1.x;
  fv21 = t2.y - t1.y;
  fu31 = t3.x - t1.x;
  fv31 = t3.y - t1.y;
  
  vcA.x = p2.x - p1.x;	vcA.y = fu21;		vcA.z = fv21;
  vcB.x = p3.x - p1.x;	vcB.y = fu31;		vcB.z = fv31;
  mi_vector_prod(&vc, &vcA, &vcB);
  if(vc.x!=0.0f)
  {
  	out_u->x = -vc.y/vc.x;
  	out_v->x = -vc.z/vc.x;
  }
  
  vcA.x = p2.y - p1.y;	vcA.y = fu21;		vcA.z = fv21;
  vcB.x = p3.y - p1.y;	vcB.y = fu31;		vcB.z = fv31;
  mi_vector_prod(&vc, &vcA, &vcB);
  if(vc.x!=0.0f)
  {
  	out_u->y = -vc.y/vc.x;
  	out_v->y = -vc.z/vc.x;
  }
  
  vcA.x = p2.z - p1.z;	vcA.y = fu21;		vcA.z = fv21;
  vcB.x = p3.z - p1.z;	vcB.y = fu31;		vcB.z = fv31;
  mi_vector_prod(&vc, &vcA, &vcB);
  if(vc.x!=0.0f)
  {
  	out_u->z = -vc.y/vc.x;
  	out_v->z = -vc.z/vc.x;
  }

  mi_vector_normalize(out_u);
  mi_vector_normalize(out_v);
}

miBoolean ccg_normal_map_illegalUV(
	miVector t1,
	miVector t2,
	miVector t3
	)
{
	miScalar tex1,tex2,tex3;

	if(t1.x==t1.y&&t1.x==t1.z&&t1.x==0) tex1 = 0;
		else tex1 = 1;
	if(t2.x==t2.y&&t2.x==t2.z&&t2.x==0) tex2 = 0;
		else tex2 = 1;
	if(t3.x==t3.y&&t3.x==t3.z&&t3.x==0) tex3 = 0;
		else tex3 = 1;
			
	if(tex1==0&&tex2==0) return miFALSE;
		else if(tex2==0&&tex3==0)	return miFALSE;
			else if(tex1==0&&tex3==0)	return miFALSE;
				else return miTRUE;
}

void ccg_normal_map_intensity(miState *state, miScalar intensity, miVector *bent, miVector *result)
{
	if(bent->x==-1 && bent->y==-1 && bent->z==-1)
	{
		*result = state->normal;
	}else {
		miVector sub;
		mi_vector_sub(&sub, bent, &state->normal);
		mi_vector_mul(&sub, intensity);
		mi_vector_add(&sub, &sub, &state->normal);
		mi_vector_normalize(&sub);
		*result = sub;
	}
}

struct ccg_normal_map {
	miColor	color;
	miScalar intensity;
	miInteger	normal_space;			//0 for object space, 1 for tangent space, 2 for world space
	miInteger texture_space;
};

extern "C" DLLEXPORT int ccg_normal_map_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_normal_map(
	miVector	*result,
	miState		*state,
	struct ccg_normal_map *paras)
{
	miColor		color = *mi_eval_color(&paras->color);
	miScalar	intens = *mi_eval_scalar(&paras->intensity);
	miInteger	space	=	*mi_eval_integer(&paras->normal_space);
	miInteger tex_space	=	*mi_eval_integer(&paras->texture_space);
	
	miVector	normal, normal_t, normal_tmp;
	miVector	v_t,v_b,v_n,v_r;
	miVector 	t1,t2,t3,p1,p2,p3;
	miVector	tU1,tV1;
	miMatrix	TBN;
	
	normal.x = color.r * 2.0f - 1.0f;
	normal.y = color.g * 2.0f - 1.0f;
	normal.z = color.b * 2.0f - 1.0f;

	if(space == 0)
	{
		mi_normal_from_object(state, &normal_tmp, &normal);
		ccg_normal_map_intensity(state, intens, &normal_tmp, &normal_t);
		state->normal = normal_t;
		//mi_vector_normalize(&state->normal);
		state->dot_nd = mi_vector_dot(&state->normal, &state->dir);
		*result = state->normal;
	}else if(space==1)
				{
					const miVector *t[3],*p[3];
					if(mi_tri_vectors(state, 't', tex_space, &t[0], &t[1], &t[2]))
					{
						t1.x = t[0]->x;	t1.y = t[0]->y;	t1.z = t[0]->z;
	  				t2.x = t[1]->x;	t2.y = t[1]->y;	t2.z = t[1]->z;
	  				t3.x = t[2]->x;	t3.y = t[2]->y;	t3.z = t[2]->z;
						if(ccg_normal_map_illegalUV(t1,t2,t3)&&mi_tri_vectors(state, 'p', 0, &p[0], &p[1], &p[2]))
						{
		  				p1.x = p[0]->x;	p1.y = p[0]->y;	p1.z = p[0]->z;
		  				p2.x = p[1]->x;	p2.y = p[1]->y;	p2.z = p[1]->z;
		  				p3.x = p[2]->x;	p3.y = p[2]->y;	p3.z = p[2]->z;
		  				
		  				ccg_normal_map_getTangentUV(t1, t2, t3, p1, p2, p3, &tU1, &tV1);
		  				
//					  tU.x = (tU1.x*state->bary[0])+(tU2.x*state->bary[1])+(tU3.x*state->bary[2]);
//					  tU.y = (tU1.y*state->bary[0])+(tU2.y*state->bary[1])+(tU3.y*state->bary[2]);
//					  tU.z = (tU1.z*state->bary[0])+(tU2.z*state->bary[1])+(tU3.z*state->bary[2]);
//					  tV.x = (tV1.x*state->bary[0])+(tV2.x*state->bary[1])+(tV3.x*state->bary[2]);
//					  tV.y = (tV1.y*state->bary[0])+(tV2.y*state->bary[1])+(tV3.y*state->bary[2]);
//					  tV.z = (tV1.z*state->bary[0])+(tV2.z*state->bary[1])+(tV3.z*state->bary[2]);
							v_t = tU1;
							v_b = tV1;
							v_n = state->normal;
							mi_vector_normalize(&v_n);
							TBN[0]=v_t.x; TBN[1]=v_t.y; TBN[2]=v_t.z; TBN[3]=0; TBN[4]=v_b.x; TBN[5]=v_b.y; TBN[6]=v_b.z; TBN[7]=0; TBN[8]=v_n.x; TBN[9]=v_n.y; TBN[10]=v_n.z; TBN[11]=0; TBN[12]=0; TBN[13]=0; TBN[14]=0; TBN[15]=1;
							//TBN[0]=1; TBN[1]=0; TBN[2]=0; TBN[3]=0; TBN[4]=0; TBN[5]=1; TBN[6]=0; TBN[7]=0; TBN[8]=0; TBN[9]=0; TBN[10]=1; TBN[11]=0; TBN[12]=0; TBN[13]=0; TBN[14]=0; TBN[15]=1;
							//TBN[0]=v_t.x; TBN[1]=v_b.x; TBN[2]=v_n.x; TBN[3]=0; TBN[4]=v_t.y; TBN[5]=v_b.y; TBN[6]=v_n.y; TBN[7]=0; TBN[8]=v_t.z; TBN[9]=v_b.z; TBN[10]=v_n.z; TBN[11]=0; TBN[12]=0; TBN[13]=0; TBN[14]=0; TBN[15]=1;
							//mi_matrix_invert(invertTBN, TBN);
							mi_vector_transform(&v_r, &normal, TBN);
							ccg_normal_map_intensity(state, intens, &v_r, &normal_tmp);
							state->normal = normal_tmp;
							//mi_vector_normalize(&state->normal);
							state->dot_nd = mi_vector_dot(&state->normal, &state->dir);
							*result = state->normal;
						}	else *result = state->normal;
					}	else *result = state->normal;
				}else if(space==2)
							{
								mi_normal_from_world(state,&normal_t,&normal);
								ccg_normal_map_intensity(state, intens, &normal_tmp, &normal_t);
								state->normal = normal_t;
								//mi_vector_normalize(&state->normal);
								state->dot_nd = mi_vector_dot(&state->normal, &state->dir);
								*result = state->normal;
							}

	return(miTRUE);
}


/*------------------------------------------ ccg_passthrough_normal_map -------*/
extern "C" DLLEXPORT int ccg_passthrough_normal_map_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_passthrough_normal_map(
	miColor		*result,
	miState		*state,
	struct ccg_normal_map *paras)
{
	miVector	dummy;
	return(ccg_normal_map(&dummy, state, paras));
}

/******************************************************************************
 * Author: lvyuedong
 * Contact: lvyuedong@hotmail.com
 * Created:	19.10.07
 * Purpose:	custom shaders for shader list
 *
 * Exports:
 *	ccg_passthrough
 *
 * Description:
 * mental ray calls the shaders in order in shader list and unnecessarily match the type of output value with input,
 * like mib_passthrough_bump_map shader, If user(especially in Maya) connects this node to, say lambert, he has to 
 * use one of attributes as input plug, but in most case all attributes of this lambert are already in use.
 * So ccg_passthrough shader just make sure these cases work well without worry about using valuable attributes.
 *****************************************************************************/
/*------------------------------------------ ccg_passthrough -------------------*/
struct ccg_passthrough_return {
	miColor		color;
	miVector	vector;
	miScalar	scalar;
};

struct ccg_passthrough {
	miColor		pass_color;
	miVector	pass_vector;
	miScalar	pass_scalar;
	miColor		sub_color;
	miVector	sub_vector;
	miScalar	sub_scalar;
};

extern "C" DLLEXPORT int ccg_passthrough_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_passthrough(
	struct ccg_passthrough_return	*result,
	miState		*state,
	struct ccg_passthrough *paras)
{
	miColor		pColor	= *mi_eval_color(&paras->pass_color);
	miVector	pVector	=	*mi_eval_vector(&paras->pass_vector);
	miScalar	pScalar	= *mi_eval_scalar(&paras->pass_scalar);
	miColor		sColor	= *mi_eval_color(&paras->sub_color);
	miVector	sVector	= *mi_eval_vector(&paras->sub_vector);
	miScalar	sScalar	= *mi_eval_scalar(&paras->sub_scalar);
	
	result->color.r = sColor.r;
	result->color.g = sColor.g;
	result->color.b = sColor.b;
	result->color.a = sColor.a;
	
	result->vector.x = sVector.x;
	result->vector.y = sVector.y;
	result->vector.z = sVector.z;
	
	result->scalar = sScalar;

	return(miTRUE);
}


/******************************************************************************
 * Author: lvyuedong
 * Contact: lvyuedong@hotmail.com
 * Created:	19.10.07
 * Purpose:	custom shaders for shader list
 *
 * Exports:
 *	ccg_normals_list
 *
 * Description:
 * In the list, call the shaders orderly which disturb normals.
 *****************************************************************************/
/*------------------------------------------ ccg_normals_list -------------------*/
struct ccg_normals_list {
	miVector	prev_normal;
	miVector	seq_normal;
};

extern "C" DLLEXPORT int ccg_normals_list_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_normals_list(
	miVector	*result,
	miState		*state,
	struct ccg_normals_list *paras)
{
	miVector pVector =	*mi_eval_vector(&paras->prev_normal);
	miVector sVector	= *mi_eval_vector(&paras->seq_normal);
	
	*result = state->normal;

	return(miTRUE);
}
