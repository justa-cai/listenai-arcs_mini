/**
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_LAUNCHER_PAGES_H__
#define __LISAUI_LAUNCHER_PAGES_H__

#ifdef __cplusplus
extern "C" {
#endif


typedef enum{
    _PAGE_MAIN_UPDATE_ADD_ICON = 0,
}launcher_page_update_event_t;

typedef struct{
    launcher_page_update_event_t event;
    union{
        struct {
            void* group;
        }update_icon;

    };

}launcher_page_update_parameter_t;


#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_LAUNCHER_PAGES_H__ */
