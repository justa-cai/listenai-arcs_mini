#include <string.h>
#include "ivCodecOne.h"
#include "ico_codec.h"

//#define ICO_ENC_OUT_SIZE         (20)

//#define VOICE_PACKET_FRAME_SIZE  (16)
#define SAMPLE_RATE              (16000)
#define MAX_FRAMESIZE            (SAMPLE_RATE/50)
#define FRAME_SIZE_7K            (MAX_FRAMESIZE>>1)

#define INPUT_SIZE_ENC   (FRAME_SIZE_7K)
#define OUT_SIZE_ENC     (ENC_BITRATE/50/8/2)

// Encode handle create parameter
#define ENC_BITRATE      (16000)
#define ENC_BANDWIDTH    (7000)
#define BUFFER_SIZE      (3000)
unsigned char icoBuffer[BUFFER_SIZE] __attribute__((aligned(4))) = {0};

ivHandle handle;
TICOInitParam param;

ivStatus ico_encode_init(void) {
	ivStatus sta = 0;

	param.nBandWidth = ENC_BANDWIDTH;
	param.nBitRate = ENC_BITRATE;
	param.pBuffer = icoBuffer;
	param.nBufferSize = BUFFER_SIZE;
	param.nCoderFlag = ICO_ENCODER;
	sta = ICOCreate(&handle, &param);
	return sta;
}

void ico_codec_reset(void) {
	ICOReset(handle);
}

ivStatus ico_codec_encode(short *in, void *enc_data, short *outLen) {
	ivStatus sta = 0;
	sta = ICOEncoder(handle, (void *)in, 320, (void *)enc_data, outLen);
	return sta;
}
