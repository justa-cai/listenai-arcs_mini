/**
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_LAUNCHER_PAGES_H__
#define __LISAUI_LAUNCHER_PAGES_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum{
    LAUNCHER_PAGE_MAIN_UPDATE_ADD_ICON = 0,
}launcher_page_update_event_t;

typedef struct{
    launcher_page_update_event_t event;
    union{
        struct {
            void* group;
        }update_icon;

    };

}launcher_page_update_parameter_t;

/**
 * @brief ���ƶ����ô����ı�
 * 
 * �˺������������ƶ��·��Ĵ����ı����á����ƶ��·��µĴ����ı�ʱ��
 * ���ô˺������������ã���ҳ�潫�Զ�ʹ���µ��ı������ֻ���ʾ��
 * 
 * @param texts �ı�����ָ�룬ÿ��Ԫ��Ϊһ�������ı��ַ���
 * @param count �ı����������֧��10���ı�
 * @param interval_ms �ֻ���������룩������3000-10000����
 * @return 0��ʾ�ɹ���������ʾʧ��
 * 
 * ʹ��ʾ����
 * @code
 * const char *cloud_texts[] = {
 *     "��ӭʹ����������",
 *     "��������ȴ�����ָ��",
 *     "��ʲô���԰���������"
 * };
 * lisaui_primary_page_set_cloud_standby_texts(cloud_texts, 3, 5000);
 * @endcode
 */
int lisaui_primary_page_set_cloud_standby_texts(const char **texts, uint32_t count, uint32_t interval_ms);

/**
 * @brief �����ƶ˴����ı�����
 * 
 * �˺����ṩһ�����Խӿڣ�������֤�ƶ˴����ı������Ƿ�����������
 * ���ô˺���������һ������ı��������ֻ����ܡ�
 */
void lisaui_primary_page_test_cloud_standby_texts(void);

/**
 * @brief Check if primary page is currently active
 * @return true if primary page is active, false otherwise
 */
bool is_primary_page_active(void);

/**
 * @brief Get primary page UI object
 * @return UI object pointer if page is active, NULL otherwise
 */
struct _lv_obj_t *page_primary_get_ui_object(void);

/**
 * @brief 停止当前正在运行的分阶段表情动画
 * @return 0表示成功，-1表示失败（主页面视图不存在）
 * @note 如果主页面视图不存在，函数会直接返回-1
 * @note 此函数会将动画状态重置为EMOJI_ANIM_STAGE_IDLE，并清除强制退出标志
 */
int current_staged_emoji_animation_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_LAUNCHER_PAGES_H__ */
