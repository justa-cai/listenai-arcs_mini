#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include"log_print.h"

//fir filter coef
const int FirFilterCoef[160][20]={
		#include"coef_c.tab"
		};

#if 0
#define INPUT_LENTH 512
short Pcm44_1kBuffer[INPUT_LENTH];
short Pcm48kBuffer[INPUT_LENTH*2];

void Usage(void)
{
    printf("\nUsage:\n");
    printf(">>  micfile.pcm speaker.pcm outfile.pcm << \n");
    return;
}
#endif
/// uiDecSize : sample count.
/// return count : sample count.
int resample_44100_to_48000(short *pInPutData, short *pOutPutdata, int uiDecSize,int ch)
{
    int idx = 0;
    int iFrameCount = 0;
    static int iInterPhase = 0;
    static short ssFirBuffer_L[20];
    static short ssFirBuffer_R[20];

    if(ch > 2)
    {
        return 0xff;
    }
    if(uiDecSize <= 0)
    {
        //printf(" *** pInPutData uiDecSize is error! ***\n");
        return 0;
    }
    if((pInPutData == NULL)||(pOutPutdata == NULL))
    {
        //printf(" *** pInPutData or pOutPutdata is error! ***\n");
        return 0;
    }

    if(ch == 1)
    {
        while(idx < uiDecSize)
        {    
            int i;
            int iFilterDataTemp_L = 0;
            iInterPhase += 147;
            if(iInterPhase >= 160)
            {
                iInterPhase -= 160;
                
                for(i=19; i>0; i--)
                { 
                    ssFirBuffer_L[i] = ssFirBuffer_L[i - 1];
                }
                /// resample have 0.5db gain.
                ssFirBuffer_L[0] = (short)((int)*(pInPutData + 2*idx)*960/1024);
                idx++;
            }
        
            for(i=0; i<20; i++)
            {
                short temp;
                temp = (short)(FirFilterCoef[iInterPhase][i]);
                iFilterDataTemp_L += ssFirBuffer_L[i]*temp;
            }
            if(iFilterDataTemp_L > 0x7ffffff)
            {
                //CLOGD("L overflow:0x%x",iFilterDataTemp_L);
                iFilterDataTemp_L = 0x7ffffff;
            }
            else if(iFilterDataTemp_L < (0 - 0x7ffffff))
            {
                //CLOGD("L overflow:0x%x",iFilterDataTemp_L);
                iFilterDataTemp_L = (0 - 0x7ffffff);
            }
            *(pOutPutdata + iFrameCount) = (short)(iFilterDataTemp_L >> 12);
            iFrameCount += ch;
        }

    }
    else
    {
        while(idx < uiDecSize/ch)
        {    
            int i;
            int iFilterDataTemp_L = 0;
            int iFilterDataTemp_R = 0;
            iInterPhase += 147;
            if(iInterPhase >= 160)
            {
                iInterPhase -= 160;
                
                for(i=19; i>0; i--)
                { 
                    ssFirBuffer_L[i] = ssFirBuffer_L[i - 1];
                    ssFirBuffer_R[i] = ssFirBuffer_R[i - 1];
                }
                /// resample have 0.5db gain,so input data increase 0.5db.
                ssFirBuffer_L[0] = (short)((int)*(pInPutData + 2*idx)*960/1024);
                ssFirBuffer_R[0] = (short)((int)*(pInPutData + 2*idx + 1)*960/1024);
                idx++;
            }

            for(i=0; i<20; i++)
            {
                short temp;
                temp = (short)(FirFilterCoef[iInterPhase][i]);
                iFilterDataTemp_L += ssFirBuffer_L[i]*temp;
                iFilterDataTemp_R += ssFirBuffer_R[i]*temp;
            }
            if(iFilterDataTemp_L > 0x7ffffff)
            {
                //CLOGD("L overflow:0x%x",iFilterDataTemp_L);
                iFilterDataTemp_L = 0x7ffffff;
            }
            else if(iFilterDataTemp_L < (0 - 0x7ffffff))
            {
                //CLOGD("L overflow:0x%x",iFilterDataTemp_L);
                iFilterDataTemp_L = (0 - 0x7ffffff);
            }
            if(iFilterDataTemp_R > 0x7ffffff)
            {
                //CLOGD("R overflow:0x%x",iFilterDataTemp_R);
                iFilterDataTemp_R = 0x7ffffff;
            }
            else if(iFilterDataTemp_R < (0 - 0x7ffffff))
            {
                //CLOGD("R overflow:0x%x",iFilterDataTemp_R);
                iFilterDataTemp_R = (0 - 0x7ffffff);
            }

            *(pOutPutdata + iFrameCount) = (short)(iFilterDataTemp_L >> 12);
            *(pOutPutdata + iFrameCount + 1) = (short)(iFilterDataTemp_R >> 12);
            iFrameCount += ch;
        }
    }

    return iFrameCount;
}
#if 0
 int main(int argc, char **argv)
{
    char *Pcm44_1kInFileName, *Pcm48kOutFileName;
    FILE *fPcm48kOutFile = NULL;
    FILE *fPcm44kinFile = NULL;
    int nFrames, FrameLenth;

    
    //test
    /*{
       int i;
        for(i = 0; i < 21; i++){
            printf("%x;\n",*(CvsdFilter1Coef+i));
        }
        for(i = 0; i < 7; i++){
            printf("%x;\n",*(CvsdFilter2Coef+i));
        }
        for(i = 0; i < 5; i++){
            printf("%x;\n",*(CvsdFilter3Coef+i));
        }
        while(1);
    }*/
    
    if (argc != 3) 
    {
        Usage();
        return -1;
    }
    Pcm44_1kInFileName = argv[1];
    Pcm48kOutFileName = argv[2];

    /* open PCM 44.1k input file */
    fPcm44kinFile = fopen(Pcm44_1kInFileName, "rb");
    if (!fPcm44kinFile) {
        printf(" *** Error opening input file %s ***\n", Pcm44_1kInFileName);
        return -1;
    }
    
    
    /* open PCM 48k output file */
    fPcm48kOutFile = fopen(Pcm48kOutFileName, "wb");
    if (!fPcm48kOutFile) {
        printf(" *** Error opening output file %s ***\n", Pcm48kOutFileName);
        return -1;
    }

    //fwrite(FirFilterCoef, 1, sizeof(FirFilterCoef), fPcm48kOutFile);
    //fflush(fPcm48kOutFile);

    nFrames=0;

    do {

        if((fread(Pcm44_1kBuffer, 2, INPUT_LENTH, fPcm44kinFile)) != INPUT_LENTH)
        {
            break;
        }
        //input 147 44.1k pcm, output 160 48k pcm;
        FrameLenth = Pcm44_1kT048k(Pcm44_1kBuffer, Pcm48kBuffer, INPUT_LENTH,2);

        fwrite(Pcm48kBuffer, 2, FrameLenth, fPcm48kOutFile);
        fflush(fPcm48kOutFile);

        printf("Frame:%d\n",nFrames);
        nFrames++;
        
            
    } while (1);


    printf("end!\n");



    /* close files */
    fclose(fPcm44kinFile);
    fclose(fPcm48kOutFile);

    
    return 0;
}
#endif
