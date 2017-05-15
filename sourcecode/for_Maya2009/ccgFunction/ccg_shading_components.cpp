#include <math.h>
#include "shader.h"
#include "crystalcg.h"

miScalar ccg_SC_specular_phong(miScalar *cosine, miScalar *nl, miVector *L, miState *state) 
{
	//R = 2N(N ¡¤ L) - L
	miVector R,V;
	miScalar s;

	V = state->dir;
	mi_vector_neg(&V);

	s = 2*(*nl);
	R.x = (state->normal.x) * s - L->x;
	R.y = (state->normal.y) * s - L->y;
	R.z = (state->normal.z) * s - L->z;
	mi_vector_normalize(&R);
	s = mi_vector_dot(&R,&V);
	if(s<0.0) s = 0.0;

	return (pow(s,*cosine));
}

miScalar ccg_SC_specular_blinn(
	miScalar ecc,
	miScalar roll,
	miVector N,
	miVector L,
	miVector V,
	miScalar LN,
	miScalar VN ) 
{
	miVector H;
    float NH, NH2, NHSQ, Dd, Gg, VH, Ff, tmp;

	mi_vector_normalize(&L);
    mi_vector_add(&H, &V, &L);
	mi_vector_normalize(&H);
	//LN = mi_vector_dot(&L, &N);
    NH = mi_vector_dot(&N, &H);
    NHSQ = NH*NH;
    NH2 = NH * 2.0f;
    Dd = (ecc+1.0f) / (NHSQ + ecc);
    Dd *= Dd;
    VH = mi_vector_dot(&V, &H);
    if( VN < LN )
    {
		if( VN * NH2 < VH )
			Gg = NH2 / VH;
		else
			Gg = 1.0f / VN;
    }
    else
    {
		if( LN * NH2 < VH )
			Gg = (LN * NH2) / (VH * VN);
		else
			Gg = 1.0f / VN;
    }
    /* poor man's Fresnel */
    tmp = pow((1.0f - VH), 3.0f);
    Ff = tmp + (1.0f - tmp) * roll;
	return (Dd * Gg * Ff);
}

/* Stam's anisotropic BRDF */
float ccg_SC_stam_anisotropy( float u, float v, float w, float rx, float ry )
{
	float w2 = w * w;
	float bt = 4.0f * float(CCG_PI) * w2 * w2 * rx * ry;
	float ex = exp(-u * u / (4.0f * w2 * rx * rx)) * exp( - v * v / (4.0f * w2 * ry * ry));

	return ex / bt;
}
/*
miScalar ccg_SC_specular_anisotropic(
	miVector P, miVector N, miVector V, float VN, float NdL, miVector xdir, miVector ydir,
	float fresnel, float roughness, float spreadx, float spready)
{
	float costheta2 = VN;
	float rx = roughness / spreadx;
	float ry = roughness / spready;

	float exists, emitspec = 0;
	color ClnoShadow = 0;
	color spec = 0;

	illuminance( P, i_N, PI / 2 )
	{
		float isKeyLight = 1;
		
		if( i_keyLightsOnly != 0 )
		{
			lightsource( "iskeylight", isKeyLight );
		}
		
		if( isKeyLight != 0 )
		{
			float nonspecular = 0;
			lightsource( "__nonspecular", nonspecular );

			if( nonspecular==0 && i_N.L>0 )
			{
				vector Ln = normalize(L);

				float costheta1 = Ln.i_N;
				vector V = Ln + i_I;

				float v = V.i_xdir;
				float u = V.ydir;
				float w = V.i_N;

				float G = (1 + Ln.i_I);
				G = clamp( G * G / (costheta1*costheta2), 0, 1);

				float D = stam_anisotropy( u, v, w, rx, ry );

				float factor =  G * D;
				float HdotI = V.i_I / length(V);
				HdotI = 1.0 - HdotI;
				HdotI = HdotI * HdotI * HdotI;
				factor *= HdotI + (1.0 - HdotI);
				factor = clamp(factor, 0, 1) * i_fresnel;

				varying color cur_cl;
				GET_CL(cur_cl);
				spec += factor * cur_cl;
			}
		}
	}
	return  spec/4;    
}
*/

miScalar ccg_SC_reflect_blinn( miScalar roll, miScalar VN ) 
{
    float Ff, tmp;

    tmp = pow((1.0f - VN), 3.0f);
    Ff = tmp + (1.0f - tmp) * roll;

	return (Ff);
}