//http://www.math.niu.edu/~rusin/known-math/96/repulsion
//http://www.math.niu.edu/~rusin/known-math/index/spheres.html

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "shader.h"



typedef double vec3[3];

double frand(void){return ((rand()-(RAND_MAX/2))/(RAND_MAX/2.));}

double dot(vec3 v1,vec3 v2){ return v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2];}

double length(vec3 v){  return sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); }

double length(vec3 v1,vec3 v2)
{
  vec3 v;
  v[0] = v2[0] - v1[0]; v[1] = v2[1] - v1[1]; v[2] = v2[2] - v1[2];
  return length(v);
}

double get_coulomb_energy(int N,vec3 p[])
{
  double e = 0;
  for(int i = 0;i<N;i++)  
    for(int j = i+1; j<N; j++ ) {
      e += 1/ length(p[i],p[j]);
    }
  return e;
}

void get_forces(int N,vec3 f[], vec3 p[])
{
  int i,j;
  for(i = 0;i<N;i++){
    f[i][0] = 0;f[i][1] = 0;f[i][2] = 0;
  }
  for(i = 0;i<N;i++){
    for(j = i+1; j<N; j++ ) {
      vec3 r = {p[i][0]-p[j][0],p[i][1]-p[j][1],p[i][2]-p[j][2]};
      double l = length(r); l = 1/(l*l*l);double ff;
      ff = l*r[0]; f[i][0] += ff; f[j][0] -= ff;
      ff = l*r[1]; f[i][1] += ff; f[j][1] -= ff;
      ff = l*r[2]; f[i][2] += ff; f[j][2] -= ff;
    }
  }
}

void ccg_sphere_point_repulsion(miVector *r, int iN, int iNstep, int iS)
{
	//N=100, Nstep=1000
	if(iN<1) iN = 1;
	if(iNstep<10) iNstep = 10;
	
	static int N=iN,Nstep=iNstep;
  static double step=0.01;
  static double minimal_step=1.e-10;
  static int S = iS;

  vec3 *p0 = new vec3[N];
  vec3 *p1 = new vec3[N];
  vec3 *f = new vec3[N];
  int i,k;
  vec3 *pp0 = p0, *pp1 = p1;

  srand(S);
  
  for(i = 0; i<N; i++ ) {
    p0[i][0] = 2*frand();
    p0[i][1] = 2*frand();
    p0[i][2] = 2*frand();
    double l = length(p0[i]);
    if(l!=0.0){
      p0[i][0] /= l;
      p0[i][1] /= l;
      p0[i][2] /= l;
    } else
      i--;
  }

  double e0 = get_coulomb_energy(N,p0);
  for(k = 0;k<Nstep;k++) {
    get_forces(N,f,p0);
    for(i=0; i < N;i++) {
      double d = dot(f[i],pp0[i]);
      f[i][0]  -= pp0[i][0]*d;
      f[i][1]  -= pp0[i][1]*d;
      f[i][2]  -= pp0[i][2]*d;
      pp1[i][0] = pp0[i][0]+f[i][0]*step;
      pp1[i][1] = pp0[i][1]+f[i][1]*step;
      pp1[i][2] = pp0[i][2]+f[i][2]*step;
      double l = length(pp1[i]);
      pp1[i][0] /= l;
      pp1[i][1] /= l;
      pp1[i][2] /= l;
    }
    double e = get_coulomb_energy(N,pp1);
    if(e >= e0){  // not successfull step
      step /= 2;
      if(step < minimal_step)
				break;
      continue;
    } else {   // successfull step
      vec3 *t = pp0;      pp0 = pp1; pp1 = t;      
      e0 = e;
      step*=2;
    }
    //mi_info("\rPoints distributing=> n: %5d, e = %18.8f step = %12.10f", k, e, step);   
    //fprintf(stderr,"\rn: %5d, e = %18.8f step = %12.10f",k,e,step);
    //fflush(stderr);
  }
  
  for(i = 0; i<N; i++ ) {
  	r[i].x = (float)p0[i][0];
  	r[i].y = (float)p0[i][1];
  	r[i].z = (float)p0[i][2];
  }

  delete[] p0;
  delete[] p1;
  delete[] f;
}
