
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "shader.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#define SHADER_VERSION 1

struct ccg_output_exr{
     miTag		filename;
	 miInteger	format;
     miInteger	padding;
     miInteger	compression;
	 miTag		fbWriteString;
};

miBoolean ccg_write_exr(char* fullpathfilename, miImg_image* img, miImg_format imgFormat, float imgCompress)
{
	miImg_file fd;

	if (fullpathfilename==NULL || img==NULL) return miFALSE;
	memset(&fd, 0, sizeof(miImg_file));

	fd.width   = img->width;
	fd.height   = img->height;
	fd.bits      = img->bits;
	fd.comp      = img->comp;
	fd.filter   = miFALSE;
	fd.topdown   = miFALSE;
	fd.gamma   = 1;
	fd.aspect   = 1;
	fd.format   = imgFormat;
	fd.error   = miIMG_ERR_NONE;
	fd.os_error = 0;
	fd.type      = (miImg_type)img->type;//(miImg_type)imgType;//miIMG_TYPE_RGBA_FP;//
	if(imgFormat == miIMG_FORMAT_EXR)
		fd.parms[0]   = imgCompress; //.exr compression type:0=default, 1=none, 2=piz,3=zip, 4=rle, 5=pxr24

	if (!mi_img_create(&fd, fullpathfilename, fd.type, imgFormat, fd.width, fd.height)) return miFALSE;
	if (!mi_img_image_write(&fd,img)) return miFALSE;
	mi_img_close(&fd);

	return miTRUE;
}

extern "C" DLLEXPORT int ccg_output_exr_version(void) {return(SHADER_VERSION);}

extern "C" DLLEXPORT miBoolean ccg_output_exr(
        void					*result,
        register miState		*state,
        struct ccg_output_exr	*paras)
{
	char	*filename, *fullname;
	char	pad[10], spec[5], *ext, *tmp, *fbstr;
	char	token[LAYER_NUM][30], *passes[LAYER_NUM];
	int		passesfb[LAYER_NUM];
	miTag	filenameTag, fbc;
	int		fb, padding, i, j, num;
	float	ctype;
	miImg_image *image;
	miImg_format format;

	//Get file name
    filenameTag = *mi_eval_tag(&paras->filename);
	if(!filenameTag) 
	{
		mi_error("Please set the name of the filename for ccg_output_exr shader.");
		return miFALSE;
	}else {
			filename = mi_mem_strdup((char*)mi_db_access(filenameTag));//for mi_mem_strdup, memory will be released at the end
			mi_db_unpin( filenameTag );
		}

	//Get file format
	i = *mi_eval_integer(&paras->format);
	ext = (char *)mi_mem_allocate(sizeof(char)*5);
	switch(i)
	{
		case 0: format = miIMG_FORMAT_EXR; strcpy(ext, "exr"); break;
		case 1: format = miIMG_FORMAT_RLA; strcpy(ext, "rla"); break;
		case 2: format = miIMG_FORMAT_TIFF; strcpy(ext, "tiff"); break;
		case 3: format = miIMG_FORMAT_TIFF_U; strcpy(ext, "tiff"); break;
		case 4: format = miIMG_FORMAT_TARGA; strcpy(ext, "tga"); break;
		case 5: format = miIMG_FORMAT_IFF; strcpy(ext, "iff"); break;
		default: format = miIMG_FORMAT_EXR; strcpy(ext, "exr");
	}

	//tokenize passes name
	fbc = *mi_eval_tag(&paras->fbWriteString);
	if(!fbc)
	{
		mi_error("Please set the fbWriteString parameter for ccg_output_exr shader.");
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
	num = i;
	mi_mem_release(fbstr);
	for(i=0;i<num;i++)
	{	
		passes[i] = strtok(token[i],"=");
		passesfb[i] = atoi(strtok(NULL,"="));
	}


    // Get frame, with padding.
    padding = *mi_eval_integer( &paras->padding );
	sprintf(spec,"%%0%dd", padding );
	sprintf(pad, spec, state->camera->frame);

	//Get compression type
	ctype = (float)*mi_eval_integer(&paras->compression);

	/*  fullname = (char*)calloc((strlen(filename)+1+strlen(tmp)+strlen(ext)),sizeof(char));
		if (fullname==NULL) exit (1);  */
	fullname = (char *)mi_mem_allocate(sizeof(char)*(int(strlen(filename))+1+int(strlen(pad))+int(strlen(ext))+30)); 
	/* the last 30 reserved for the name of passes */

	for(i=0,fb=0;i<(state->options->no_images - miRC_IMAGE_USER);i++,fb++)
	{
		//Write image file
		for(j=0;j<num;j++)
		{
			if(passesfb[j]==fb)
			{
				strcpy(fullname, filename);
				strcat(fullname, ".");
				strcat(fullname, passes[j]);
				strcat(fullname, ".");
				strcat(fullname, pad);
				strcat(fullname, ".");
				strcat(fullname, ext);
				image = mi_output_image_open(state, miRC_IMAGE_USER+fb);
				ccg_write_exr(fullname, image, format, ctype);
				mi_output_image_close(state, miRC_IMAGE_USER+fb);
				break;
			}
		}
	}

	mi_mem_release(ext);
	mi_mem_release(fullname);
	mi_mem_release(filename);
	return(miTRUE);
}


