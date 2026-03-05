#ifndef CONFIG_H
#define CONFIG_H

// #define ENC_BITRATE_12K
#define ENC_BITRATE_16K

#define PCM_FRAME_SIZE (640)

#ifdef ENC_BITRATE_12K
#define ENC_BITRATE    (12000)
#define ICO_FRAME_SIZE (30)
#elif defined ENC_BITRATE_16K
#define ENC_BITRATE    (16000)
#define ICO_FRAME_SIZE (40)
#endif

#endif
