#include <stdint.h>
#include "lisa_device.h"
#include "lisa_audio.h"

#define TAG "audio_play"
#include "lisa_log.h"

void audio_play_send_pcm(char *data, int size)
{
    static lisa_device_t *audio_dev = NULL;
    if(audio_dev == NULL){
        audio_dev = lisa_device_get("audio0");

    }
    int ret = lisa_audio_play_write(audio_dev, (int16_t *)data,size / 2);
   
}