
#ifndef __AACDEC_H__
#define __AACDEC_H__
#if defined (__arm) && defined (__ARMCC_VERSION)
#elif defined(__GNUC__) && defined(__arm__)
#elif defined(__GNUC__) && defined(__i386__)
#elif defined(__GNUC__) && defined(__amd64__)
#elif defined(__GNUC__) && (defined(__powerpc__) || defined(__POWERPC__))
#elif defined(_OPENWAVE_SIMULATOR) || defined(_OPENWAVE_ARMULATOR)
#elif defined(_SOLARIS) && !defined(__GNUC__)
#elif defined(ARDUINO)
#elif defined(__riscv) || defined(__riscv__)
#else
#error No platform defined. See valid options in aacdec.h
#endif
#ifdef __cplusplus
extern"C"{
#endif
#ifndef AAC_MAX_NCHANS
#define AAC_MAX_NCHANS		(0x1286+5246-0x2702)
#endif
#define AAC_MAX_NSAMPS		(0x604+7778-0x2066)
#define AAC_MAINBUF_SIZE	((0x10b7+4958-0x2115) * AAC_MAX_NCHANS)
#define AAC_NUM_PROFILES	(0xa98+5264-0x1f25)
#define AAC_PROFILE_MP		(0x1204+3402-0x1f4e)
#define AAC_PROFILE_LC		(0xc58+5441-0x2198)
#define AAC_PROFILE_SSR		(0xf18+1812-0x162a)
#if defined(AACDEC_FEATURE_AUDIO_CODEC_AAC_SBR)
#define AAC_ENABLE_SBR
#endif 
#define AAC_ENABLE_MPEG4
enum{ERR_AAC_NONE=(0x827+3288-0x14ff),
ERR_AAC_INDATA_UNDERFLOW=-(0xfa3+1826-0x16c4),
ERR_AAC_NULL_POINTER=-(0x1327+1573-0x194a),
ERR_AAC_INVALID_ADTS_HEADER=-(0x1e09+1291-0x2311),
ERR_AAC_INVALID_ADIF_HEADER=-(0x129d+1157-0x171e),
ERR_AAC_INVALID_FRAME=-(0xad8+1901-0x1240),
ERR_AAC_MPEG4_UNSUPPORTED=-(0x1d7b+1581-0x23a2),
ERR_AAC_CHANNEL_MAP=-(0x432+7323-0x20c6),
ERR_AAC_SYNTAX_ELEMENT=-(0x776+396-0x8fa),
ERR_AAC_DEQUANT=-(0x1c48+2228-0x24f3),
ERR_AAC_STEREO_PROCESS=-(0x4c8+5175-0x18f5),
ERR_AAC_PNS=-(0x1c16+98-0x1c6d),
ERR_AAC_SHORT_BLOCK_DEINT=-(0xe00+5766-0x247a),
ERR_AAC_TNS=-(0x12e8+1102-0x1729),ERR_AAC_IMDCT=-
(0x88d+641-0xb00),ERR_AAC_NCHANS_TOO_HIGH=-(0x1c5d+742-0x1f34),
ERR_AAC_SBR_INIT=-(0x198c+151-0x1a13),
ERR_AAC_SBR_BITSTREAM=-(0x1ac3+1466-0x206c),
ERR_AAC_SBR_DATA=-(0x4a1+7644-0x226b),
ERR_AAC_SBR_PCM_FORMAT=-(0x1c+7602-0x1dbb),
ERR_AAC_SBR_NCHANS_TOO_HIGH=-(0x1ca3+2119-0x24d6),
ERR_AAC_SBR_SINGLERATE_UNSUPPORTED=-(0x8e4+3332-0x15d3),
ERR_AAC_RAWBLOCK_PARAMS=-(0x110b+2779-0x1bd0),
ERR_AAC_UNKNOWN=-9999};
typedef struct _AACFrameInfo {
    int bitRate;
    int nChans;
    int sampRateCore;
    int sampRateOut;
    int bitsPerSample;
    int outputSamps;
    int profile;
    int tnsUsed;
    int pnsUsed;
} AACFrameInfo;
typedef void* HAACDecoder;
HAACDecoder AACInitDecoder(void);HAACDecoder AACInitDecoderPre(void*
ReplacementFor_ptr,int ReplacementFor_sz);void AACFreeDecoder(HAACDecoder 
ReplacementFor_hAACDecoder);int AACDecode(HAACDecoder ReplacementFor_hAACDecoder
,unsigned char**ReplacementFor_inbuf,int*ReplacementFor_bytesLeft,short*
ReplacementFor_outbuf);int AACFindSyncWord(unsigned char*ReplacementFor_buf,int 
ReplacementFor_nBytes);void AACGetLastFrameInfo(HAACDecoder 
ReplacementFor_hAACDecoder,AACFrameInfo*
ReplacementFor_aacFrameInfo);int AACSetRawBlockParams(HAACDecoder 
ReplacementFor_hAACDecoder,int ReplacementFor_copyLast,
AACFrameInfo*ReplacementFor_aacFrameInfo);int AACFlushCodec(
HAACDecoder ReplacementFor_hAACDecoder);
#ifdef AACDEC_CONFIG_AAC_GENERATE_TRIGTABS_FLOAT
int AACInitTrigtabsFloat(void);void AACFreeTrigtabsFloat(void);
#endif
#ifdef __cplusplus
}
#endif
#endif	

