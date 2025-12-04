#ifndef PHOTO_RECOGNITION_H
#define PHOTO_RECOGNITION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get photo recognition result
 * @return const char* Recognition result string
 */
const char* get_photo_recognition_result(void);

int button_photo_recognition(void);

#ifdef __cplusplus
}
#endif

#endif // PHOTO_RECOGNITION_H
