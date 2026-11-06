// memscan: find guest floats that move like a position between snapshots of
// the guest RAM (32 MB dumps taken with dd from /proc/<pid>/mem).
//   memscan a.bin b.bin [c.bin] [min max]   -> offsets whose float changed by
//   min..max between each pair of consecutive snapshots (default 0.05..30).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *load(const char *p, size_t *n){
  FILE *f=fopen(p,"rb"); if(!f){perror(p);exit(1);} fseek(f,0,SEEK_END); *n=ftell(f); fseek(f,0,SEEK_SET);
  unsigned char *b=malloc(*n); if(fread(b,1,*n,f)!=*n){perror("read");exit(1);} fclose(f); return b;}
static int moved(float a,float b,float lo,float hi){
  if(!isfinite(a)||!isfinite(b)) return 0; if(fabsf(a)>5000||fabsf(b)>5000) return 0;
  float d=fabsf(a-b); return d>=lo && d<=hi; }
int main(int argc,char **argv){
  if(argc<3){fprintf(stderr,"usage: memscan a b [c] [min max]\n");return 2;}
  size_t na,nb,nc=0; unsigned char *a=load(argv[1],&na),*b=load(argv[2],&nb),*c=NULL;
  int ai=3; if(argc>3 && strstr(argv[3],".bin")){c=load(argv[3],&nc);ai=4;}
  float lo=0.05f,hi=30.0f; if(argc>ai+1){lo=atof(argv[ai]);hi=atof(argv[ai+1]);}
  size_t n=na<nb?na:nb; if(c&&nc<n)n=nc; long count=0;
  for(size_t o=0;o+4<=n;o+=4){
    float fa,fb,fc; memcpy(&fa,a+o,4); memcpy(&fb,b+o,4);
    if(!moved(fa,fb,lo,hi)) continue;
    if(c){ memcpy(&fc,c+o,4); if(!moved(fb,fc,lo,hi)) continue; }
    if(++count<=300){ if(c) printf("%08zx %.3f %.3f %.3f\n",0x08000000+o,fa,fb,fc); else printf("%08zx %.3f %.3f\n",0x08000000+o,fa,fb);}
  }
  printf("total %ld\n",count); return 0; }
