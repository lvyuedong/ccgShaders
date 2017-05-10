
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "shader.h"
#include "geoshader.h"
#include "crystalcg.h"
#include "ccgFunction.h"

struct ccg_geo_occlusion {
	miBoolean	enable_occlusion;
	miInteger	rays;
	miBoolean	enable_cache;
	miInteger	density;
	miInteger	cache_points;
};

extern "C" DLLEXPORT int ccg_geo_occlusion_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_geo_occlusion(
	miTag						*result,
	miState						*state,
	struct ccg_geo_occlusion	*paras)
{
	miOptions	*opt = (miOptions *)state->options;
	miCamera 	*cam = (miCamera *)state->camera;
	miInteger	fb_no, fb_type;
	miBoolean	fb_inter;
	char		*type, *filename, *fullname;
	char		pad[10], spec[5], *ext, *tmp, *fbstr;
	char		token[LAYER_NUM][30], *passes[LAYER_NUM];
	int			passesfb[LAYER_NUM];
	float		cmp[8];
	int			padding, i, num, ctype;
	miTag		filenameTag, fbc;

	//Get file name
	filenameTag = *mi_eval_tag(&paras->filename);
	if(!filenameTag) 
	{
		mi_error("Please set the name of the filename for ccg_geo_occlusion shader.");
		return miFALSE;
	}else {
			filename = mi_mem_strdup((char*)mi_db_access(filenameTag));
			mi_db_unpin( filenameTag );
		}

	//Get file format
	i = *mi_eval_integer(&paras->format);
	ext = (char *)mi_mem_allocate(sizeof(char)*5);
	switch(i)
	{
		case 0: strcpy(ext, "exr"); break;
		case 1: strcpy(ext, "rla"); break;
		case 2: strcpy(ext, "tiff"); break;
		case 3: strcpy(ext, "tiff"); break;
		case 4: strcpy(ext, "tga"); break;
		case 5: strcpy(ext, "iff"); break;
		default: strcpy(ext, "exr");
	}

	//tokenize passes name
	fbc = *mi_eval_tag(&paras->fbWriteString);
	if(!fbc)
	{
		mi_error("Please set the fbWriteString parameter for ccg_geo_occlusion shader.");
		return miFALSE;
	}else {
			fbstr = mi_mem_strdup((char*)mi_db_access(fbc));
			mi_db_unpin( fbc );
		}

	i=0;
	tmp = strtok(fbstr,";");
	while (tmp != NULL)
	{
		strcpy(token[i], tmp);
		tmp = strtok (NULL, ";");
		i++;
	}
	num = fb_no = i;
	mi_mem_release(fbstr);
	for(i=0;i<num;i++)
	{	
		passes[i] = strtok(token[i],"=");
		passesfb[i] = atoi(strtok(NULL,"="));
	}

    //Get frame, with padding.
    padding = *mi_eval_integer( &paras->padding );
	sprintf(spec,"%%0%dd", padding );
	sprintf(pad, spec, state->camera->frame);

	//Get compression type
	ctype = *mi_eval_integer(&paras->compression);
	char *compress = (char *)mi_mem_allocate(sizeof(char)*8);
	switch(ctype)
	{
		case 0:	cmp[0] = 0; 
				strcpy(compress, "default");
				break;
		case 1: cmp[0] = 1;
				strcpy(compress, "none");
				break;
		case 2: cmp[0] = 2;
				strcpy(compress, "piz");
				break;
		case 3: cmp[0] = 3;
				strcpy(compress, "zip");
				break;
		case 4: cmp[0] = 4;
				strcpy(compress, "rle");
				break;
		case 5: cmp[0] = 5;
				strcpy(compress, "pxr24");
				break;
		default: cmp[0] = 1;
				 strcpy(compress, "none");
	}

	//Get framebuffer type
	fb_type	= *mi_eval_integer(&paras->type_fb);
	fb_inter = *mi_eval_boolean(&paras->interpolate_fb);

	type = (char *)mi_mem_allocate(sizeof(char)*10);
	if(fb_inter) strcpy(type, "+");
	else strcpy(type, "-");
	switch(fb_type)
	{
		case 0: strcat(type, "rgba_fp"); break;
		case 1: strcat(type, "rgba"); break;
		default: strcat(type, "rgba_fp");
	}

	fullname = (char *)mi_mem_allocate(sizeof(char)*(int(strlen(filename))+1+int(strlen(pad))+int(strlen(ext))+30));
	mi::shader::Edit_fb framebuffers(state->camera->buffertag);

	//set framebuffer for damn maya glow
	//miTag mayaGlowShaderTag = mi_api_decl_lookup(mi_mem_strdup("maya_shaderglow"));
	//if(mayaGlowShaderTag != miNULLTAG)
	//{
	//	char *glow = mi_mem_strdup("glow");
	//	framebuffers->set(glow, mi_mem_strdup("datatype"), (const char*)type);
	//	framebuffers->set(glow, mi_mem_strdup("filtering"), true);
	//	framebuffers->set(glow, mi_mem_strdup("user"), true);
	//		strcpy(fullname, filename);
	//		strcat(fullname, ".");
	//		if(!*mi_eval_boolean(&paras->Enable_Multi_channel_OpenExr) || strcmp(ext,"exr"))
	//		{
	//			strcat(fullname, "glow");
	//			strcat(fullname, ".");
	//		}
	//		strcat(fullname, pad);
	//		strcat(fullname, ".");
	//		strcat(fullname, ext);
	//	framebuffers->set(glow, mi_mem_strdup("filename"), fullname);
	//	framebuffers->set(glow, mi_mem_strdup("filetype"), (const char*)ext);
	//	mi_mem_release(glow);
	//}

	//set framebuffer for user
	for (i=0; i<fb_no; i++)
	{
		const char *buffername = passes[i];
		framebuffers->set(buffername, mi_mem_strdup("datatype"), (const char*)type);
		framebuffers->set(buffername, mi_mem_strdup("filtering"), true);
		framebuffers->set(buffername, mi_mem_strdup("user"), true);
		strcpy(fullname, filename);
		strcat(fullname, ".");
		if(!*mi_eval_boolean(&paras->Enable_Multi_channel_OpenExr) || strcmp(ext,"exr"))
		{
			strcat(fullname, passes[i]);
			strcat(fullname, ".");
		}
		strcat(fullname, pad);
		strcat(fullname, ".");
		strcat(fullname, ext);
		framebuffers->set(buffername, mi_mem_strdup("filename"), (const char*)fullname);
		framebuffers->set(buffername, mi_mem_strdup("filetype"), (const char*)ext);
		if(!strcmp(ext,"exr")) framebuffers->set(buffername, mi_mem_strdup("compression"), (const char*)compress);
	}

	mi_mem_release(compress);
	mi_mem_release(type);
	mi_mem_release(ext);
	mi_mem_release(fullname);
	mi_mem_release(filename);

	return(miTRUE);
}

