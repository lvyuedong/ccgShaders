//#include <io.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <cstring>
#include <string>
#include "shader.h"

char* ccg_F_lastOfSlash(char *path, int *last)
{
	char *pch;
	pch = strrchr(path, '\\');
	if(pch){
		*last = int(strlen(pch) - 1); 
	}
	else {
			pch = strrchr(path, '/');
			if(pch){
				*last = int(strlen(pch) - 1);
			}else *last = 0;
		}
	return (pch+1);
}

/*
void ccg_F_getFilename(char *path, char *filename, char )
//extract file name from path, *filename should be allocated from external
{
	string str_path = path;

	size_t last_1 = str_path.length() - str_path.find_last_of('\\') - 1;
	size_t last_2 = str_path.length() - str_path.find_last_of('/') - 1;
	string str_sub;
	if(last_1<last_2){
		str_sub = str_path.substr( str_path.find_last_of('\\')+1);
	}else str_sub = str_path.substr( str_path.find_last_of('/')+1);

	strcpy(filename, str_sub.c_str());


}

void ccg_F_getDirectory(char *path, char *directory)
//extract directory from path, *directory should be allocated from external
{
	string str_path = path;

	size_t last_1 = str_path.length() - str_path.find_last_of('\\') - 1;
	size_t last_2 = str_path.length() - str_path.find_last_of('/') - 1;
	string str_sub;
	if(last_1<last_2){
		str_sub = str_path.substr( 0, str_path.find_last_of('\\')+1);
	}else str_sub = str_path.substr( 0, str_path.find_last_of('/')+1);

	strcpy(directory, str_sub.c_str());
}
*/