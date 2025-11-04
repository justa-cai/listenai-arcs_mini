#ifndef __PAGE_INFO_H__
#define __PAGE_INFO_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if info page is currently active
 * @return true if info page is active, false otherwise
 */
bool is_info_page_active(void);

/**
 * @brief Set info page active state
 * @param active true to set active, false to set inactive
 */
void set_info_page_active(bool active);

#ifdef __cplusplus
}
#endif

#endif /* __PAGE_INFO_H__ */
