/*
This confidential and propriety software may be used
only as authorized by a licensing agreement from
InSilicon corporation

In the event of publication, the following notice is
applicable

(C) COPYRIGHT 2001 INSILICON CORPORATION
ALL RIGHTS RESERVED

the entire notice must be reproduced on all
authorized copies

File : jdem.c
Author : Vincenzo Liguori
Date : 01-01-19
Version 1.01
Abstract : This program emulates the decoder side of the JPEG codec

Modification history :
Date      by  Version  Change description
-------------------------------------------------
01-01-16  VL   1.0     Original
01-01-19  VL   1.01    Number of MCUs eliminated at start of output
03-12-17  MA   2.1     a) corrected bug in restart flag masking
                       b) fixed bug of not flushing input buffer when 
		          restart merker is encountered
                       c) corrected wrong extraction of restart marker bit
		       d) added <stdlib.h> to correct the hpux compilation
*/

#include<stdio.h>
#include<stdlib.h>

#define UNSIGNED_DCT_OUT 1

FILE *fpi, *fpo;
int block[8][8];
int qt[4][64],hdt[2][65536],hdtl[2][16],hat[2][65536],hatl[2][256];
int nc[4],q[4],hd[4],ha[4]; /* Number of data units, QT table, Huf table */
int pdc[4];
int bits, bitc;
int rstdtc,rstn;

int zz[64];

int zzs[8][8]={
{0,1,5,6,14,15,27,28},
{2,4,7,13,16,26,29,42},
{3,8,12,17,25,30,41,43},
{9,11,18,24,31,40,44,53},
{10,19,23,32,39,45,52,54},
{20,22,33,38,46,51,55,60},
{21,34,37,47,50,56,59,61},
{35,36,48,49,57,58,62,63}
};

int ic[8][8]={
{23168, 23168, 23168, 23168, 23168, 23168, 23168, 23168}, 
{32144, 27248, 18208,  6400,  6400, 18208, 27248, 32144}, 
{30272, 12544, 12544, 30272, 30272, 12544, 12544, 30272}, 
{27248,  6400, 32144, 18208, 18208, 32144,  6400, 27248}, 
{23168, 23168, 23168, 23168, 23168, 23168, 23168, 23168}, 
{18208, 32144,  6400, 27248, 27248,  6400, 32144, 18208}, 
{12544, 30272, 30272, 12544, 12544, 30272, 30272, 12544}, 
{ 6400, 18208, 27248, 32144, 32144, 27248, 18208,  6400}
};

int is[8][8]={
{0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 1, 1, 1, 1},
{0, 0, 1, 1, 1, 1, 0, 0},
{0, 1, 1, 1, 0, 0, 0, 1},
{0, 1, 1, 0, 0, 1, 1, 0},
{0, 1, 0, 0, 1, 1, 0, 1},
{0, 1, 0, 1, 1, 0, 1, 0},
{0, 1, 0, 1, 0, 1, 0, 1}
};

void pblock(){
int i,j;

  for(i=0;i<8;i++){
    for(j=0;j<8;j++)
      printf("%03x ",block[i][j]&0x7ff);
    printf("\n");
  }
  printf("\n");
}

void pzig(){
int i,j,k;

  for(k=i=0;i<8;i++){
    for(j=0;j<8;j++)
      printf("%03x ",zz[k++]&0x7ff);
    printf("\n");
  }
  printf("\n");
}

void pwriteb(){
int i,j,v;

  for(i=0;i<8;i++){
    for(j=0;j<8;j++){
      v=block[i][j];
      if(v<-128)
        v=-128;
      else if(v>127)
        v=127;
      printf("%02x ",v+128);
    }
    printf("\n");
  }
  printf("\n");
}

/* Outputs the block */
void writeb(){
int i,j,v;

  for(i=0;i<8;i++)
    for(j=0;j<8;j++){
      v=block[i][j];
#if UNSIGNED_DCT_OUT
			fputc(v,fpo);
#else
      if(v<-128)
        v=-128;
      else if(v>127)
        v=127;
      fputc(v+128,fpo);
#endif
    }     
}

void idct(){
int y,x,u,v;
int reg[8];

  /* Vertical */
  for(y=0;y<8;y++){
    for(x=0;x<8;x++)
      reg[x]=0;

    for(x=0;x<8;x++)
      for(u=0;u<8;u++){
        v=block[x][y]*ic[x][u];
        v+=8192;
        v>>=14;
        if(is[x][u])
          v=-v;
        reg[u]+=v;
      }

    for(x=0;x<8;x++)
      block[x][y]=reg[x];
  }

  /* Horizontal */
  for(y=0;y<8;y++){
    for(x=0;x<8;x++)
      reg[x]=0;

    for(x=0;x<8;x++){
      for(u=0;u<8;u++){
        v=block[y][x]*ic[x][u];
        v+=16384;
        v>>=15;
        if(is[x][u])
          v=-v;
        reg[u]+=v;
      }
    }

    for(x=0;x<8;x++){
      v=reg[x];
      v+=4;
      v>>=3;
      block[y][x]=v;
    }
  }

#if UNSIGNED_DCT_OUT
	for(y=0;y<8;y++){
    for(x=0;x<8;x++) {
			v = block[x][y];
      if(v<-128)
        v=-128;
      else if(v>127)
        v=127;
			v += 128;
			block[x][y] = v;
		}
	}
#endif

}

/* Zig-zag */
void zigzag(){
int i,j,k;

  for(k=i=0;i<8;i++)
    for(j=0;j<8;j++)
      block[i][j]=zz[zzs[i][j]];
}

/* Dequantization step */
void dequant(int c){
int i,v;

  c=q[c];
  for(i=0;i<64;i++){
    v=zz[i]*qt[c][i];
    if(v<-1023)
      v=-1023;
    else if(v>1023)
      v=1023;
    zz[i]=v;
  }
}

/* Input a byte */
int bin(){
int b;

 if(!rstdtc){
	 b=fgetc(fpi);
	 if(b==255){
		 while((b=fgetc(fpi))==255);
		 if((b&0xf8)==0xd0){
			 rstdtc=1;
			 rstn=b&7;
		 }
		 else if (b==0xd9)
			 return b;
		 else if(b){
			 printf("Marker %02x not expected !\n",b);
			 exit(1);
		 } else
			 b=255;
	 }
 }
 return b;
}

int num(v,sz){
int k;

  if(((1<<(sz-1))&v)==0){
    k=0xffffffff<<sz;
    v|=k;
    v++;
  }
  return v;
}

void enough(x){

  /* Make sure that there are at */
  /* least x bits in the buffer */
  while(bitc<x){
    bits<<=8;
    bits|=bin()&255;
    bitc+=8;
  }
}

/* Huffman decoding */
void dec(int c){
int t,i,diff,v;
int zc,sz,rs;

  /* Clear the whole block */
  for(i=1;i<64;i++)
    zz[i]=0;

  /* Decode the DC */
  t=hd[c];

  enough(16);
  rs=bits>>(bitc-16);
  rs&=65535;
  rs=hdt[t][rs];
  bitc-=hdtl[t][rs];

  if(rs){
    enough(rs);
    diff=bits>>(bitc-rs);
    diff&=(1<<rs)-1;
    bitc-=rs;
    diff=num(diff,rs);
  } else
    diff=0;

  zz[0]=diff+pdc[c]; /* Get the DC value */
  pdc[c]=zz[0];

  /* Decode the AC */
  t=ha[c];
  i=1;
  while(i<64){
    enough(16);
    rs=bits>>(bitc-16);
    rs&=65535;
    rs=hat[t][rs];
    bitc-=hatl[t][rs];
    if(!rs)
      break;
    i+=rs>>4;
    if(sz=rs&15){
      enough(sz);
      v=bits>>(bitc-sz);
      v&=(1<<sz)-1;
      bitc-=sz;
      v=num(v,sz);
      zz[i]=v;      
    }
    i++;
  }
}

main(int argc, char **argv){
char buff[100],s[4];
int i,j,k,hid;
/* Number of MCUs, colour components, QT tables, Huf tables */
int nmcu,ncc,nqt,ndht,naht;
int restart,rstcnt,nrst,rstc;
int code,hcode;

if (argc < 6){
  fprintf(stderr,"Usage: jdem <regs> <Q table> <hmem.sim> <input ECS file> <Decoded YUV MCU file> \n");
  exit(1);
}

  /* Load the registers */
  if((fpi=fopen(argv[1],"r"))==NULL){
    printf("Could not open %s\n",argv[1]);
    exit(1);
  }

  /* Get Register 1 */
  fscanf(fpi,"%x",&i);

  printf("Register 1\n");
  ncc=i&3;
  printf("Ncol=%d\n",ncc);
  ncc++;
  restart=(i&0x4)>>2;
  printf("Re=%d\n",restart);

  /* Get Register 2 */
  fscanf(fpi,"%x",&i);

  printf("\nRegister 2\n");
  nmcu=i;
  printf("NMCu=%d\n",nmcu);
  nmcu++;

  /* Get Register 3 */
  fscanf(fpi,"%x",&nrst);
  printf("\nRegister 3\n");
  printf("NRST=%d\n",nrst);

  /* Get Register 4 */
  fscanf(fpi,"%x",&i);

  printf("\nRegister 4\n");
  hd[0]=i&1;
  ha[0]=i>>1; ha[0]&=1;
  q[0]=i>>2; q[0]&=3;
  nc[0]=i>>4; nc[0]&=15;
  printf("HD0=%d HA0=%d QT0=%d NBlock0=%d\n",hd[0],ha[0],q[0],nc[0]);
  nc[0]++;

  /* Get Register 5 */
  fscanf(fpi,"%x",&i);

  printf("\nRegister 5\n");
  hd[1]=i&1;
  ha[1]=i>>1; ha[1]&=1;
  q[1]=i>>2; q[1]&=3;
  nc[1]=i>>4; nc[1]&=15;
  printf("HD1=%d HA1=%d QT1=%d NBlock1=%d\n",hd[1],ha[1],q[1],nc[1]);
  nc[1]++;

  /* Get Register 6 */
  fscanf(fpi,"%x",&i);

  printf("\nRegister 6\n");
  hd[2]=i&1;
  ha[2]=i>>1; ha[2]&=1;
  q[2]=i>>2; q[2]&=3;
  nc[2]=i>>4; nc[2]&=15;
  printf("HD2=%d HA2=%d QT2=%d NBlock2=%d\n",hd[2],ha[2],q[2],nc[2]);
  nc[2]++;

  /* Get Register 7 */
  fscanf(fpi,"%x",&i);

  printf("\nRegister 7\n");
  hd[3]=i&1;
  ha[3]=i>>1; ha[3]&=1;
  q[3]=i>>2; q[3]&=3;
  nc[3]=i>>4; nc[3]&=15;
  printf("HD3=%d HA3=%d QT3=%d NBlock3=%d\n",hd[3],ha[3],q[3],nc[3]);
  nc[3]++;

  fclose(fpi);

  /* Load the Quantization tables */
  if((fpi=fopen(argv[2],"r"))==NULL){
    printf("Could not open %s\n",argv[2]);
    exit(1);
  }
  for(i=0;i<4;i++){
    if(fscanf(fpi,"%x",&qt[i][0])==EOF)
      break;
    for(j=1;j<64;j++)
      fscanf(fpi,"%x",&qt[i][j]);
  }
  fclose(fpi);

  /* Load the Huffman tables */
  if((fpi=fopen(argv[3],"r"))==NULL){
    printf("Could not open %s\n",argv[3]);
    exit(1);
  }

  for(j=0;j<16;j++)
    hdtl[0][j]=hdtl[1][j]=0;
  for(j=0;j<256;j++)
    hatl[0][j]=hatl[1][j]=0;

  /* Read Huf tables */
  while(fscanf(fpi,"%d",&hid)!=EOF){
    fscanf(fpi,"%d",&code);
    fscanf(fpi,"%d",&hcode);
    if(hid&2){
      fscanf(fpi,"%d",&hatl[hid&1][code]);
      k=16-hatl[hid&1][code];
      hcode<<=k;
      k=1<<k;
      for(j=0;j<k;j++)
        hat[hid&1][hcode|j]=code;
    } else {
      fscanf(fpi,"%d",&hdtl[hid&1][code]);
      k=16-hdtl[hid&1][code];
      hcode<<=k;
      k=1<<k;
      for(j=0;j<k;j++)
        hdt[hid&1][hcode|j]=code;
    }
  }
  fclose(fpi);

  /* Open the encoded file */
  if((fpi=fopen(argv[4],"rb"))==NULL){
    printf("Could not open %s\n",argv[4]);
    exit(1);
  }

  /* Open the output file */
  if((fpo=fopen(argv[5],"wb"))==NULL){
    printf("Could not open %s\n",argv[5]);
    exit(1);
  }

  /* Init the DC previous values */
  for(i=0;i<ncc;i++)
    pdc[i]=0;

  /* Init the bit buffer */
  bits=0;
  bitc=0;

  /* Decode the blocks */
  rstcnt=nrst; rstc=0;/* Init the RESTART counters */
  for(i=0;i<nmcu;i++){

    /* For each colour component */
    for(j=0;j<ncc;j++)
      for(k=0;k<nc[j];k++){

        dec(j); /* Decode a block */

        dequant(j); /* Dequantize it */

        zigzag(); /* Zig-zag rearrangement */

        idct(); /* Perform IDCT */

        writeb(); /* Write it */

      }

    if(rstdtc && rstcnt == 0) {
				if(rstn!=rstc){
					printf("Wrong Restart marker\n");
					printf("Exp %d Found %d\n",rstc,rstn);
				}
				pdc[0]=0;
				pdc[1]=0;
				pdc[2]=0;
				pdc[3]=0;
				bitc=0;
				/* Reload the RESTART counter */
				rstcnt=nrst;
				/* Update the restart number */
				rstc++;
				rstc&=7; 
				rstdtc=0;
    } else if (restart && (i!=(nmcu-1))){
      if(rstcnt)
        rstcnt--;
      else{
        if(fgetc(fpi)!=255){
          printf("Restart Marker expected : no marker found\n");
          printf("%x %x %x %x\n",fgetc(fpi),fgetc(fpi),fgetc(fpi),fgetc(fpi));
          exit(1);
        }
        while((k=fgetc(fpi))==255);
        if((k&0xf8)==0xd0){
          if((k&7)!=rstc){
            printf("Wrong Restart marker\n");
            printf("Exp %d Found %d\n",rstc,k&7);
          }
        } else{
          printf("Unexpected marker found\n");
          exit(1);
        }
        /* Clear DC and bit buffer*/
        pdc[0]=0;
        pdc[1]=0;
        pdc[2]=0;
        pdc[3]=0;
        bitc=0;
        /* Reload the RESTART counter */
        rstcnt=nrst;
        /* Update the restart number */
        rstc++;
        rstc&=7;      
      }
    }
  }

  /* Close the file */
  fclose(fpi);
  fclose(fpo);
}


