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

File : jcem.c
Author : Vincenzo Liguori
Date : 01-01-16
Version 1.0
Abstract : This program emulates the encoder side of the JPEG codec

Modification history :
Date      by  Version  Change description
-------------------------------------------------
01-01-16  VL   1.0     Original

03-12-17  MA   2.1     a) corrected bug in restart flag masking
                       b) corrected bug in table selection of EOB coding
		       c) corrected wrong extraction of restart marker bit
		       d) added <stdlib.h> needed for the hpux compilation
 */

#include<stdio.h>
#include<stdlib.h>

FILE *fpi, *fpo;
int block[8][8];
int hdt[2][16],hdtl[2][16],hat[2][256],hatl[2][256];
int nc[4],q[4],hd[4],ha[4]; /* Number of data units, QT table, Huf table */
int qv[4][64],sf[4][64];
int pdc[4];
int bits, bitc;

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

int c[8][8]={
{23168, 32144, 30272, 27248, 23168, 18208, 12544, 6400},
{23168, 27248, 12544, 6400, 23168, 32144, 30272, 18208},
{23168, 18208, 12544, 32144, 23168, 6400, 30272, 27248},
{23168, 6400, 30272, 18208, 23168, 27248, 12544, 32144},
{23168, 6400, 30272, 18208, 23168, 27248, 12544, 32144},
{23168, 18208, 12544, 32144, 23168, 6400, 30272, 27248},
{23168, 27248, 12544, 6400, 23168, 32144, 30272, 18208},
{23168, 32144, 30272, 27248, 23168, 18208, 12544, 6400}
};

int s[8][8]={
{0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 1, 1, 1, 1, 1},
{0, 0, 1, 1, 1, 0, 0, 0},
{0, 0, 1, 1, 0, 0, 1, 1},
{0, 1, 1, 0, 0, 1, 1, 0},
{0, 1, 1, 0, 1, 1, 0, 1},
{0, 1, 0, 0, 1, 0, 1, 0},
{0, 1, 0, 1, 0, 1, 0, 1}
};

void pblock(){
int i,j;

  for(i=0;i<8;i++){
    for(j=0;j<8;j++)
      printf("%4d ",block[i][j]);
    printf("\n");
  }
  printf("\n");
}

void pzig(){
int i,j,k;

  for(k=i=0;i<8;i++){
    for(j=0;j<8;j++)
      printf("%4d ",zz[k++]);
    printf("\n");
  }
  printf("\n");

}

void readb(){
int i,j,v;

  for(i=0;i<8;i++)
    for(j=0;j<8;j++){
      if((v=fgetc(fpi))==EOF){
        printf("Premature EOF!\n");
        exit(1);
      }
      block[i][j]=v-128;
    }
}

void dct(){
int y,x,u,v;
int reg[8];

  /* Horizontal */
  for(y=0;y<8;y++){
    for(x=0;x<8;x++)
      reg[x]=0;

    for(x=0;x<8;x++)
      for(u=0;u<8;u++){
        v=block[y][x]*c[x][u];
        v+=2048;
        v>>=12;
        if(s[x][u])
          v=-v;
        reg[u]+=v;
      }

    for(x=0;x<8;x++)
      block[y][x]=reg[x];
  }

  /* Vertical */
  for(y=0;y<8;y++){
    for(x=0;x<8;x++)
      reg[x]=0;

    for(x=0;x<8;x++)
      for(u=0;u<8;u++){
        v=block[x][y]*c[x][u];
        v+=131072;
        v>>=18;
        if(s[x][u])
          v=-v;
        reg[u]+=v;
      }

    for(x=0;x<8;x++){
      v=reg[x];
      v+=2;
      v>>=2;
      block[x][y]=v;
    }
  }

}

/* Zig-zag */
void zigzag(){
int i,j,k;

  for(k=i=0;i<8;i++)
    for(j=0;j<8;j++)
      zz[zzs[i][j]]=block[i][j];
}

/* Quntization step */
void quant(int c){
int i,qq,v;

  qq=q[c];
  for(i=0;i<64;i++){
    v=(zz[i]*qv[qq][i])>>(sf[qq][i]-1);
    v++; v>>=1;
    if(v==-1024)
      v=-1023;
    zz[i]=v;
  }
}

int prio(int x){
int p=0;

  if(x<0)
    x=-x;
  if((x&0xff00)>(x&0x00ff))
    p+=8;
  if((x&0xf0f0)>(x&0x0f0f))
    p+=4;
  if((x&0xcccc)>(x&0x3333))
    p+=2;
  if((x&0xaaaa)>(x&0x5555))
    p+=1;
  if(x)
    p++;
  return(p);
}

/* Output a byte */
void bout(int b){
  b&=255;
  fputc(b,fpo);
  if(b==255)
    fputc(0,fpo);
}

void code(int x,int sz){
int nb;

  x&=(1<<sz)-1;
  bits<<=sz;
  bits|=x;
  bitc+=sz;
  nb=bitc>>3;
  bitc&=7;
  switch(nb){
  case 2:
    bout(bits>>(bitc+8));
  case 1:
    bout(bits>>bitc);
  }
}

/* Huffman encoding */
void enc(int c){
int t,i,diff;
int zc,sz,rs;

  /* DC encoding */
  /* printf("    DC[%d,%d]: %d\n",c,0,zz[0]); */
  diff=zz[0]-pdc[c];
  pdc[c]=zz[0];
  t=hd[c];
  sz=prio(diff);

  if(hdtl[t][sz]==0){
    printf("DC%d Huff code wrong !\n",t);
    exit(1);
  }

  code(hdt[t][sz],hdtl[t][sz]);
  if(diff<0)
    diff--;
  code(diff,sz);

  /* AC encoding */
  t=ha[c];
  zc=0;
  for(i=1;i<64;i++){
    if(zz[i]){
      while(zc>=16){
        /* Encode ZRLs */
        code(hat[t][240],hatl[t][240]);
        zc-=16;
      }
      /* Encode any other case */
      sz=prio(zz[i]);
      rs=(zc<<4)|sz;

      if(hatl[t][sz]==0){
        printf("AC%d Huff code wrong !\n",t);
        exit(1);
      }

      code(hat[t][rs],hatl[t][rs]);
      if(zz[i]<0)
        zz[i]--;
      code(zz[i],sz);
      zc=0;
    } else
    zc++;
  }
  if(zc) /* Encode EOB */
    code(hat[t][0],hatl[t][0]);
}

/* Flush the encoder */
void flush(){
  if(bitc){
    bitc=8-bitc;
    bits<<=bitc;
    bits|=(1<<bitc)-1;
    bout(bits);
    bitc=0;
  }
}

main(int argc, char **argv){
char buff[100],s[4];
int i,j,k,c;
/* Number of MCUs, colour components, QT tables, Huf tables */
int nmcu,ncc,nqt,ndht,naht;
int restart,rstcnt,nrst,rstc;

if (argc < 6){
  fprintf(stderr,"Usage: jcem <regs> <Q table> <H table> <input MCU file> <Encoded ECS file> \n");
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
    if(fscanf(fpi,"%x",&k)==EOF)
      break;
    if(k==1){
      qv[i][0]=2;
      sf[i][0]=1;
    }else{
      qv[i][0]=k&0x7ff;
      sf[i][0]=(k>>11)+11;
    }
    for(j=1;j<64;j++){
      fscanf(fpi,"%x",&k);
      if(k==1){
        qv[i][j]=2;
        sf[i][j]=1;
      }else{
        qv[i][j]=k&0x7ff;
        sf[i][j]=(k>>11)+11;
      }
    }
  }
  fclose(fpi);

  /* Load the Huffman tables */
  if((fpi=fopen(argv[3],"r"))==NULL){
    printf("Could not open %s\n",argv[3]);
    exit(1);
  }

  /* Get AC table 0 */
  for(i=0;i<16;i++){
    for(j=1;j<=10;j++){
      fscanf(fpi,"%x",&k);
      c=(i<<4)|j;
      hat[0][c]=k&255;
      k=(k>>8)+1;
      hatl[0][c]=k;
      if(k>8){
        k=(1<<(k-8))-1;
        hat[0][c]=hat[0][c]|(k<<8);
      }
    }
  }
  fscanf(fpi,"%x",&k);
  c=0;
  hat[0][c]=k&255;
  k=(k>>8)+1;
  hatl[0][c]=k;
  if(k>8){
    k=(1<<(k-8))-1;
    hat[0][c]=hat[0][c]|(k<<8);
  }
  fscanf(fpi,"%x",&k);
  c=240;
  hat[0][c]=k&255;
  k=(k>>8)+1;
  hatl[0][c]=k;
  if(k>8){
    k=(1<<(k-8))-1;
    hat[0][c]=hat[0][c]|(k<<8);
  }
  for(j=2;j<16;j++)
    fscanf(fpi,"%x",&k);

  /* Get AC table 1 */
  for(i=0;i<16;i++){
    for(j=1;j<=10;j++){
      fscanf(fpi,"%x",&k);
      c=(i<<4)|j;
      hat[1][c]=k&255;
      k=(k>>8)+1;
      hatl[1][c]=k;
      if(k>8){
        k=(1<<(k-8))-1;
        hat[1][c]=hat[1][c]|(k<<8);
      }
    }
  }
  fscanf(fpi,"%x",&k);
  c=0;
  hat[1][c]=k&255;
  k=(k>>8)+1;
  hatl[1][c]=k;
  if(k>8){
    k=(1<<(k-8))-1;
    hat[1][c]=hat[1][c]|(k<<8);
  }
  fscanf(fpi,"%x",&k);
  c=240;
  hat[1][c]=k&255;
  k=(k>>8)+1;
  hatl[1][c]=k;
  if(k>8){
    k=(1<<(k-8))-1;
    hat[1][c]=hat[1][c]|(k<<8);
  }
  for(j=2;j<16;j++)
    fscanf(fpi,"%x",&k);

  /* Get DC table 0 */
  for(j=0;j<=11;j++){
    fscanf(fpi,"%x",&k);
    hdt[0][j]=k&255;
    k=(k>>8)+1;
    hdtl[0][j]=k;
    if(k>8){
      k=(1<<(k-8))-1;
      hdt[0][j]=hdt[0][j]|(k<<8);
    }
  }
  for(j=12;j<16;j++)
    fscanf(fpi,"%x",&k);

  /* Get DC table 1 */
  for(j=0;j<=11;j++){
    fscanf(fpi,"%x",&k);
    hdt[1][j]=k&255;
    k=(k>>8)+1;
    hdtl[1][j]=k;
    if(k>8){
      k=(1<<(k-8))-1;
      hdt[1][j]=hdt[1][j]|(k<<8);
    }
  }

  fclose(fpi);

  /* Open the block file */
  if((fpi=fopen(argv[4],"rb"))==NULL){
    printf("Could not open %s block file\n",argv[4]);
    exit(1);
  }

  /* Open the output file */
  if((fpo=fopen(argv[5],"wb"))==NULL){
    printf("Could not open %s output file\n",argv[5]);
    exit(1);
  }

  /* Init the DC previous values */
  for(i=0;i<ncc;i++)
    pdc[i]=0;

  /* Init the bit buffer */
  bits=0;
  bitc=0;

  /* Encode the blocks */
  rstcnt=nrst; rstc=0;/* Init the RESTART counters */
  for(i=0;i<nmcu;i++){

    /* For each colour component */
    for(j=0;j<ncc;j++)
      for(k=0;k<nc[j];k++){

        readb(); /* Read a block */

        dct(); /* Perform DCT */

        zigzag(); /* Zig-zag rearrangement */

        quant(j); /* Quantize */

        enc(j); /* Encode */

      }

    if(restart && (i!=(nmcu-1)))
      if(rstcnt)
        rstcnt--;
      else{
        /* Clear DC */
        pdc[0]=0;
        pdc[1]=0;
        pdc[2]=0;
        pdc[3]=0;
        /* Flush */
        flush();
        /* Output the marker */
        fputc(255,fpo);
        fputc(208|rstc,fpo);
        /* Reload the RESTART counter */
        rstcnt=nrst;
        /* Update the restart number */
        rstc++;
        rstc&=7;      
      }
  }

  flush();
  fputc(255,fpo);
  fputc(217,fpo);
  /* Close the file */
  fclose(fpi);
  fclose(fpo);
}


