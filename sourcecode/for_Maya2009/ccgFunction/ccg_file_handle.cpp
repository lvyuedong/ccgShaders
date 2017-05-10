#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <string>

//#include <windows.h>	//for CreateDirectory

#include "shader.h"

using namespace std;

miBoolean ccg_F_fileOrDir_exists(char *path)	//file or directory string
//if filename is a directory, it doesn't matter whether the "\\" or "/" appears at the end
{
	if( (_access( path, 0 )) != -1 ) return miTRUE;
	else return miFALSE;
}

void ccg_F_getFilename(char *path, char *filename)
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

/*
void ccg_F_createDir(char *path)
//CreateDirectory build directory only if the parent tree exists
{
	CreateDirectory((LPCTSTR)path, NULL);
}
*/