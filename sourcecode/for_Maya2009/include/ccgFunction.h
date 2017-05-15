

#ifndef ccgFunction_h
#define ccgFunction_h

char * ccg_get_pass_name(char *, int);

void ccg_pass_string(char *, struct ccg_passfbArray *);

miScalar ccg_fresnel_reflectance(miState *, miScalar, miScalar);

miScalar ccg_smoothstep(miScalar, miScalar, miScalar);

//void ccg_fresnel_LUT(float ***);

void ccg_illegal_copyright(miState *, miColor *);

void ccg_query_light(miState *, char *, miTag *, miColor *);

miBoolean ccg_query_value(char *, miTag, miColor *, miBoolean);

void ccg_shadow_choose_volume(miState *);

miBoolean ccg_getTangentUV(	miState *, miInteger,
							miVector, miVector, miVector,
							miVector, miVector, miVector,
							miVector *, miVector *);

miBoolean ccg_illegalTex( miVector, miVector, miVector);

void ccg_sphere_point_repulsion(miVector *, int, int, int);

int ccg_on_plane(miVector *, miVector *, miVector *);

void ccg_bent_space_conversion(miState *, int, miVector *, miVector *);

void ccg_colormap2Vector(miColor *, miVector *);

miBoolean ccg_mi_call_shader(miColor *, miState *, miTag);

miColor * ccg_mi_eval_color(miState *, void *);

miScalar * ccg_mi_eval_scalar(miState *, void *);

miBoolean * ccg_mi_eval_boolean(miState *, void *);

miInteger * ccg_mi_eval_integer(miState *, void *);

miVector * ccg_mi_eval_vector(miState *, void *);

miTag * ccg_mi_eval_tag(miState *, void *);

//shading components
miScalar ccg_SC_specular_phong(miScalar *, miScalar *, miVector *, miState *);

miScalar ccg_SC_specular_blinn(miScalar, miScalar, miVector,
								miVector, miVector, miScalar, miScalar);

miScalar ccg_SC_reflect_blinn(miScalar, miScalar);

//file handle function

char* ccg_F_lastOfSlash(char *, int *);

//miBoolean  ccg_F_fileOrDir_exists(char *);

//void ccg_F_getFilename(char *, char *);

//void ccg_F_getDirectory(char *, char *);

//void ccg_F_createDir(char *);

//miBoolean ccg_mib_amb_occlusion(miColor *, miState *, struct ccg_mib_amb_occlusion *);

#endif	//ccgFunction_h