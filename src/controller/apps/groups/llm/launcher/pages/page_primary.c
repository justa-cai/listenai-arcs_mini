#include <stdint.h>
#include <string.h>

#include "ebus/ebus.h"
#include "lvgl.h"

#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "launcher_pages.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_assets.h"
#include "user_groups.h"
#include "lisaui_log.h"
#include "lisaui_user_data.h"
#include "lisa_display.h"
#include "lisa_aiui.h"

// 添加新的组件头文件
#include "lisa_ui_llm_primary.h"

// 包含emoji动画资源
#include "private/anim_images.h"

#include "../group_launcher.h"

#define TAG "launcher.page.main"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG TAG

#define STATUS_TEXT_MAX_LEN 32
#define CONTENT_TEXT_MAX_LEN 512
#define CYCLE_INDEX_MIN 1
#define CYCLE_INDEX_MAX 50
// 犯困表情切换延时时间（毫秒）
#define SLEEPY_EMOJI_DELAY_MS 30000  // 30秒后切换到犯困表情
// 待机文本轮换时间间隔（毫秒）
#define STANDBY_TEXT_ROTATE_INTERVAL_MS 5000  // 5秒切换一次待机文本

// ===================== emoji动画配置 =====================

// emoji动画配置数组
typedef struct {
    uint32_t first_frame_delay;
    uint32_t duration;
    uint32_t images_count;
    const lv_img_dsc_t *images;
    
    // 新增分阶段动画参数
    uint32_t enter_end_frame;       // 进入到循环的端点帧
    uint32_t loop_end_frame;        // 循环到退出的端点帧
    uint32_t loop_duration_ms;      // 循环阶段持续时间（毫秒）
    bool enable_staged_animation;   // 是否启用分阶段动画
} emoji_anim_config_t;

// 动画阶段枚举
typedef enum {
    EMOJI_ANIM_STAGE_ENTER = 0,     // 进入阶段
    EMOJI_ANIM_STAGE_LOOP = 1,      // 循环阶段
    EMOJI_ANIM_STAGE_EXIT = 2,      // 退出阶段
    EMOJI_ANIM_STAGE_IDLE = 3       // 空闲状态
} emoji_anim_stage_e;

// 动画状态管理结构体
typedef struct {
    lisa_ui_emoji_type_e current_emoji;    // 当前表情类型
    emoji_anim_stage_e current_stage;      // 当前动画阶段
    lv_timer_t *stage_timer;               // 阶段切换定时器
    uint32_t loop_start_time;              // 循环开始时间
    uint32_t loop_duration;                // 循环持续时间
    uint32_t loop_count;                   // 循环次数计数器
    uint32_t loop_target;                  // 目标循环次数
    bool is_running;                       // 动画是否正在运行
    bool force_exit;                       // 强制退出标志
    bool mcp_protected;                    // MCP保护标志，防止被云端打断
} emoji_anim_state_t;

static const emoji_anim_config_t emoji_anim_configs[LISA_UI_EMOJI_MAX] = {
    [LISA_UI_EMOJI_UNKNOW] = {
        .first_frame_delay = 0,
        .duration = 1200,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_blink),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_blink),
        .enter_end_frame = 1,
        .loop_end_frame =  LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_blink) - 1,
        .loop_duration_ms = 3000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_LOVE] = {
        .first_frame_delay = 0,
        .duration = 1000,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_love),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_love),
        .enter_end_frame = 5,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_love) - 5,
        .loop_duration_ms = 5000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_SAD] = {
        .first_frame_delay = 0,
        .duration = 900,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_sad),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_sad),
        .enter_end_frame = 5,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_sad) - 5,
        .loop_duration_ms = 1500,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_LAUGH] = {
        // .first_frame_delay = 0,
        // .duration = 2000,
        // .images_count = sizeof(emoji_mock) / sizeof(emoji_mock[0]),
        // .images = emoji_mock,
        .images_count = 0,
        .images = NULL,
        .enable_staged_animation = false,
    },
    [LISA_UI_EMOJI_SQUINT] = {
        // .first_frame_delay = 0,
        // .duration = 2000,
        // .images_count = sizeof(emoji_squint) / sizeof(emoji_squint[0]),
        .images_count = 0,
        .images = NULL,
        // .images = emoji_squint,
        .enable_staged_animation = false,
    },
    [LISA_UI_EMOJI_ANGRY] = {
        .first_frame_delay = 0,
        .duration = 400,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_angry),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_angry),
        .enter_end_frame = 6,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_angry) - 6,
        .loop_duration_ms = 5000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_EYE] = {
        .first_frame_delay = 0,
        .duration = 500,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_eye),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_eye),
        .enter_end_frame = 3,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_eye) - 3,
        .loop_duration_ms = 2000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_BLINK] = {
        .first_frame_delay = 1500,
        .duration = 200,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_blink),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_blink),
        .enter_end_frame = 2,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_blink) - 2,
        .loop_duration_ms = 2000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_HUG] = {
        .first_frame_delay = 0,
        .duration = 1000,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_hug),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_hug),
        .enter_end_frame = 3,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_hug) - 3,
        .loop_duration_ms = 2500,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_PUZZLED] = {
        .first_frame_delay = 0,
        .duration = 900,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_puzzled),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_puzzled),
        .enter_end_frame = 2,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_puzzled) - 2,
        .loop_duration_ms = 1800,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_WAKEUP] = {
        .first_frame_delay = 0,
        .duration = 1200,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_wakeup),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_wakeup),
        .enter_end_frame = 3,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_wakeup) - 3,
        .loop_duration_ms = 2000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_SLEEPY] = {
        .first_frame_delay = 0,
        .duration = 1600,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_sleepy),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_sleepy),
        .enter_end_frame = 2,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_sleepy) - 2,
        .loop_duration_ms = 3000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_WAIT] = {
        .first_frame_delay = 0,
        .duration = 300,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_wait),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_wait),
        .enter_end_frame = 1,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_wait) - 1,
        .loop_duration_ms = 1200,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_START_BATTERY_CHANGE] = {
        .first_frame_delay = 0,
        .duration = 400,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_battery),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_battery),
        .enter_end_frame = 5,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_battery) - 9,
        .loop_duration_ms = 3000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_HAPPY] = {
        .first_frame_delay = 0,
        .duration = 800,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_happy),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_happy),
        .enter_end_frame = 5,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_happy) - 7,
        .loop_duration_ms = 5000,
        .enable_staged_animation = true,
    },
    [LISA_UI_EMOJI_CUTE] = {
        .first_frame_delay = 0,
        .duration = 600,
        .images_count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_cute),
        .images = LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_cute),
        .enter_end_frame = 5,
        .loop_end_frame = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(img_png_cute) - 7,
        .loop_duration_ms = 5000,
        .enable_staged_animation = true,
    },
};

/**
 * @brief 根据emoji类型设置动画
 */
static void set_emoji_animation_by_type(lv_obj_t *obj, lisa_ui_emoji_type_e emoji_type)
{
    if (!obj || emoji_type >= LISA_UI_EMOJI_MAX) {
        return;
    }
    
    const emoji_anim_config_t *config = &emoji_anim_configs[emoji_type];
    
    // 检查是否启用分阶段动画
    if (config->enable_staged_animation && config->images) {
        // 使用新的分阶段动画接口（假设底层支持）
        // 这里需要调用支持分阶段动画的接口
        // 暂时保持原有接口，后续可以扩展底层接口来支持分阶段动画
        lisa_ui_llm_primary_set_custom_emoji_animation(obj, config->images, 
                                                       config->images_count, 
                                                       config->duration,
                                                       config->first_frame_delay);
        
        // 记录分阶段动画参数到日志，便于调试
        LISAUI_LOGI(TAG, "Staged animation config for emoji %d: enter[0-%d], loop[%d-%d], exit[%d-%d], loop_duration=%dms", 
                   emoji_type,
                   config->enter_end_frame,
                   config->enter_end_frame + 1, config->loop_end_frame,
                   config->loop_end_frame + 1, config->images_count - 1,
                   config->loop_duration_ms);
    } else {
        // 使用传统动画方式
        lisa_ui_llm_primary_set_custom_emoji_animation(obj, config->images, 
                                                       config->images_count, 
                                                       config->duration,
                                                       config->first_frame_delay);
    }
}

/**
 * @brief 设置分阶段emoji动画
 * @param obj UI对象
 * @param emoji_type emoji类型
 * @param stage 动画阶段：0=进入，1=循环，2=退出
 */
static void set_staged_emoji_animation(lv_obj_t *obj, lisa_ui_emoji_type_e emoji_type, uint8_t stage)
{
    if (!obj || emoji_type >= LISA_UI_EMOJI_MAX) {
        return;
    }
    
    const emoji_anim_config_t *config = &emoji_anim_configs[emoji_type];
    
    if (!config->enable_staged_animation || !config->images) {
        // 如果不支持分阶段动画，使用传统方式
        set_emoji_animation_by_type(obj, emoji_type);
        return;
    }
    
    uint32_t start_frame, end_frame, duration;
    
    switch (stage) {
        case 0: // 进入阶段: 0到enter_end_frame
            start_frame = 0;
            end_frame = config->enter_end_frame;
            duration = config->duration / 3; // 进入阶段占总时长的1/3
            break;
            
        case 1: // 循环阶段: enter_end_frame+1到loop_end_frame
            start_frame = config->enter_end_frame + 1;
            end_frame = config->loop_end_frame;
            // 循环阶段：计算单次循环时间，设置循环次数为3次
            duration = config->duration / 3; // 单次循环时间
            break;
            
        case 2: // 退出阶段: loop_end_frame+1到最后一帧
            start_frame = config->loop_end_frame + 1;
            end_frame = config->images_count - 1;
            duration = config->duration / 3; // 退出阶段占总时长的1/3
            break;
            
        default:
            LISAUI_LOGW(TAG, "Invalid animation stage: %d", stage);
            return;
    }
    
    // 计算该阶段的帧数
    uint32_t frame_count = end_frame - start_frame + 1;
    
    if (start_frame >= config->images_count || end_frame >= config->images_count) {
        LISAUI_LOGW(TAG, "Frame range [%d-%d] exceeds image count %d for emoji %d", 
                   start_frame, end_frame, config->images_count, emoji_type);
        return;
    }
    
    // 创建该阶段的图片数组指针
    const lv_img_dsc_t *stage_images = &config->images[start_frame];
    
    // 设置该阶段的动画
    lisa_ui_llm_primary_set_custom_emoji_animation(obj, stage_images, 
                                                   frame_count, 
                                                   duration,
                                                   config->first_frame_delay);
    
    // 如果是循环阶段，根据时间计算循环次数
    if (stage == 1) {
        // 计算单次循环的实际时间
        // duration是为这个阶段分配的总时间，frame_count是这个阶段的帧数
        uint32_t frame_duration_ms = duration / frame_count; // 每帧时长
        uint32_t single_loop_time = frame_count * frame_duration_ms; // 单次循环时间
        
        // 根据配置的循环总时间计算需要循环的次数
        uint32_t calculated_loops = 1; // 默认至少1次
        if (single_loop_time > 0) {
            calculated_loops = config->loop_duration_ms / single_loop_time;
            if (calculated_loops < CYCLE_INDEX_MIN) calculated_loops = CYCLE_INDEX_MIN; // 至少循环1次
            if (calculated_loops > CYCLE_INDEX_MAX) calculated_loops = CYCLE_INDEX_MAX; // 最多循环20次，避免过长
        }
        
        lisa_ui_llm_primary_set_loop_count(obj, calculated_loops);
        LISAUI_LOGI(TAG, "Loop calculation: frame_duration=%dms, single_loop=%dms, target_total=%dms, loops=%d", 
                   frame_duration_ms, single_loop_time, config->loop_duration_ms, calculated_loops);
    } else {
        lisa_ui_llm_primary_set_loop_count(obj, 1); // 其他阶段只播放一次
    }
    
    LISAUI_LOGI(TAG, "Set staged animation for emoji %d, stage %d: frames[%d-%d], duration=%dms", 
               emoji_type, stage, start_frame, end_frame, duration);
}

/**
 * 页面视图结构体
 * 遵循MVC架构，视图仅负责UI渲染
 */
typedef struct {
    lv_obj_t *inter;              /**< 主要UI组件 - 使用lisa_ui_llm_primary */
    ebus_chn_t *ebus_ch_base_event;
    
    /* 任务栏状态管理 */
    bool wifi_connected;          /**< WiFi连接状态 */
    uint8_t battery_level;        /**< 电量等级 0-4 */
    char current_status[STATUS_TEXT_MAX_LEN];      /**< 当前状态文本 */
    
    /* emoji和内容管理 */
    lisa_ui_emoji_type_e current_emoji;  /**< 当前emoji类型 */
    char content_text[CONTENT_TEXT_MAX_LEN];       /**< 内容文本 */
    
    /* 犯困表情定时器 */
    lv_timer_t *sleepy_timer;     /**< 犯困表情切换定时器 */
    bool is_sleepy_mode;          /**< 是否处于犯困模式 */
    
    /* 充电状态管理 */
    uint8_t last_charging_state;  /**< 上次充电状态，用于检测充电状态变化 */
    
    /* 分阶段动画状态管理 */
    emoji_anim_state_t anim_state; /**< 动画状态管理 */
    
    /* 待机文本轮换管理 */
    lv_timer_t *text_rotate_timer; /**< 文本轮换定时器 */
    uint8_t current_text_index;    /**< 当前显示的文本索引 */
    bool is_text_rotating;         /**< 是否正在轮换文本 */
} page_view_t;

// 当前主页面视图指针
static page_view_t *g_current_primary_view = NULL;

// 主页面是否处于活动状态（show状态）
static bool g_primary_page_is_active = false;

// ===================== 函数声明 =====================
static void stop_staged_emoji_animation(page_view_t *view);
static void start_staged_emoji_animation(page_view_t *view, lisa_ui_emoji_type_e emoji_type);
static void request_exit_staged_animation(page_view_t *view);
static void set_content_text(page_view_t *view, const char *text);
static void start_text_rotation(page_view_t *view);
static void stop_text_rotation(page_view_t *view);
// static char *replace_role_name_with_wakeup_word(const char *text, const char *role_name);
static const char *make_wake_hint_text(const char *base_text, const char *wake_word);
void trigger_primary_page_refresh(void);

// 定义映射数组
static const char *remote_inter_state_map[] = {
    [LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE] = "我在听...",
    [LISAUI_USERDATA_INTER_REMOTE_STATE_LISTENING] = "我在听...",
    [LISAUI_USERDATA_INTER_REMOTE_STATE_THINKING] = "思考中...",
    [LISAUI_USERDATA_INTER_REMOTE_STATE_TALKING] = "说话中...",
    // 其他状态可以根据需要映射
};

// ===================== 分阶段动画管理函数 =====================

/**
 * @brief 动画阶段切换定时器回调函数
 */
static void emoji_stage_timer_cb(lv_timer_t *timer)
{
    page_view_t *view = (page_view_t *)timer->user_data;
    
    if (!view || !view->inter) {
        return;
    }
    
    emoji_anim_state_t *anim_state = &view->anim_state;
    
    // 检查动画状态，如果已经结束则直接返回
    if (!anim_state->is_running || anim_state->current_stage == EMOJI_ANIM_STAGE_IDLE) {
        LISAUI_LOGI(TAG, "Animation already stopped, ignoring timer callback");
        return;
    }
    
    const emoji_anim_config_t *config = &emoji_anim_configs[anim_state->current_emoji];
    
    if (!config->enable_staged_animation) {
        return;
    }
    
    LISAUI_LOGI(TAG, "Animation stage timer callback, current stage: %d", anim_state->current_stage);
    
    switch (anim_state->current_stage) {
        case EMOJI_ANIM_STAGE_ENTER:
            // 进入阶段完成，切换到循环阶段
            anim_state->current_stage = EMOJI_ANIM_STAGE_LOOP;
            anim_state->loop_start_time = lv_tick_get();
            anim_state->loop_duration = config->loop_duration_ms;
            
            // 设置循环阶段动画
            set_staged_emoji_animation(view->inter, anim_state->current_emoji, EMOJI_ANIM_STAGE_LOOP);
            lisa_ui_llm_primary_start_emoji_animation(view->inter);
            
            // 设置循环持续时间定时器
            lv_timer_set_period(anim_state->stage_timer, config->loop_duration_ms);
            lv_timer_set_repeat_count(anim_state->stage_timer, 1); // 只执行一次
            lv_timer_reset(anim_state->stage_timer);
            
            LISAUI_LOGI(TAG, "Enter stage completed, switching to loop stage for %dms", config->loop_duration_ms);
            break;
            
        case EMOJI_ANIM_STAGE_LOOP:
            // 循环阶段时间到，切换到退出阶段
            anim_state->current_stage = EMOJI_ANIM_STAGE_EXIT;
            
            // 设置退出阶段动画
            set_staged_emoji_animation(view->inter, anim_state->current_emoji, EMOJI_ANIM_STAGE_EXIT);
            lisa_ui_llm_primary_start_emoji_animation(view->inter);
            
            // 设置退出阶段持续时间
            uint32_t exit_duration = config->duration / 3;
            lv_timer_set_period(anim_state->stage_timer, exit_duration);
            lv_timer_set_repeat_count(anim_state->stage_timer, 1); // 只执行一次
            lv_timer_reset(anim_state->stage_timer);
            
            LISAUI_LOGI(TAG, "Loop stage completed after %dms, switching to exit stage", config->loop_duration_ms);
            break;
            
        case EMOJI_ANIM_STAGE_EXIT:
            // 退出阶段完成，动画结束
            anim_state->current_stage = EMOJI_ANIM_STAGE_IDLE;
            anim_state->is_running = false;
            anim_state->mcp_protected = false;  // 清除MCP保护标志
            
            // 停止并删除定时器
            if (anim_state->stage_timer) {
                lv_timer_del(anim_state->stage_timer);
                anim_state->stage_timer = NULL;
            }
            
            // 停止动画播放
            lisa_ui_llm_primary_stop_emoji_animation(view->inter);
            
            LISAUI_LOGI(TAG, "Exit stage completed, animation finished, MCP protection cleared");

            // 检查是否为充电表情动画完成
            if (anim_state->current_emoji == LISA_UI_EMOJI_START_BATTERY_CHANGE) {
                // 充电表情动画完成后，显示唤醒提示
                const char *base_text = "请用\"#唤醒词#\"唤醒我";
                // 获取当前角色名称进行动态替换
                // const char *role_name = NULL;
                const char *wake_word = NULL;
                LISAUI_USERDATA_WITH_LOCK(_userdata) {
                    // role_name = _userdata->roles.roles[_userdata->roles.role_idx].name;
                    wake_word = _userdata->setting.wake_word;
                }
                const char *wake_hint = make_wake_hint_text(base_text, wake_word);
                set_content_text(view, wake_hint); // 更新文本
                LISAUI_LOGI(TAG, "Battery charging animation completed, showing wake-up prompt");
            }

            // 统一恢复到无表情（中立）
            view->current_emoji = LISA_UI_EMOJI_UNKNOW;
            set_emoji_animation_by_type(view->inter, view->current_emoji);
            lisa_ui_llm_primary_start_emoji_animation(view->inter);
            break;
            
        default:
            LISAUI_LOGW(TAG, "Unknown animation stage: %d", anim_state->current_stage);
            break;
    }
}

/**
 * @brief 启动分阶段emoji动画
 */
static void start_staged_emoji_animation(page_view_t *view, lisa_ui_emoji_type_e emoji_type)
{
    if (!view || !view->inter || emoji_type >= LISA_UI_EMOJI_MAX) {
        LISAUI_LOGE(TAG, "[%s] Invalid parameters - view:%p, emoji_type:%d", __FUNCTION__, view, emoji_type);
        return;
    }
    
    LISAUI_LOGI(TAG, "[%s] === Starting Animation Process === emoji_type: %d", __FUNCTION__, emoji_type);
    
    const emoji_anim_config_t *config = &emoji_anim_configs[emoji_type];
    
    LISAUI_LOGI(TAG, "[%s] Animation config - enable_staged: %d, images: %p, loop_duration: %dms", 
               __FUNCTION__, config->enable_staged_animation, config->images, config->loop_duration_ms);
    
    if (!config->enable_staged_animation || !config->images) {
        // 不支持分阶段动画，使用传统方式
        LISAUI_LOGI(TAG, "[%s] Using traditional animation for emoji %d", __FUNCTION__, emoji_type);
        set_emoji_animation_by_type(view->inter, emoji_type);
        lisa_ui_llm_primary_start_emoji_animation(view->inter);
        view->current_emoji = emoji_type;
        LISAUI_LOGI(TAG, "[%s] Traditional animation started successfully", __FUNCTION__);
        return;
    }
    
    emoji_anim_state_t *anim_state = &view->anim_state;
    
    LISAUI_LOGI(TAG, "[%s] Before stop - stage: %d, running: %d, timer: %p", 
               __FUNCTION__, anim_state->current_stage, anim_state->is_running, anim_state->stage_timer);
    
    // 停止当前动画
    stop_staged_emoji_animation(view);
    
    LISAUI_LOGI(TAG, "[%s] Animation stopped, clearing state with memset", __FUNCTION__);
    // 强制清理状态，确保完全重置
    memset(anim_state, 0, sizeof(emoji_anim_state_t));
    
    // 初始化动画状态
    anim_state->current_emoji = emoji_type;
    anim_state->current_stage = EMOJI_ANIM_STAGE_ENTER;
    anim_state->is_running = true;
    anim_state->force_exit = false;
    anim_state->loop_start_time = 0;
    anim_state->loop_duration = config->loop_duration_ms;
    anim_state->stage_timer = NULL;  // 确保定时器指针为空
    anim_state->mcp_protected = false;  // 默认不启用MCP保护
    
    LISAUI_LOGI(TAG, "[%s] State initialized - emoji: %d, stage: %d, running: %d", 
               __FUNCTION__, anim_state->current_emoji, anim_state->current_stage, anim_state->is_running);
    
    // 更新视图的当前表情
    view->current_emoji = emoji_type;
    
    // 设置进入阶段动画
    set_staged_emoji_animation(view->inter, emoji_type, EMOJI_ANIM_STAGE_ENTER);
    lisa_ui_llm_primary_start_emoji_animation(view->inter);
    
    LISAUI_LOGI(TAG, "[%s] Setting enter stage animation", __FUNCTION__);
    
    // 创建阶段切换定时器
    uint32_t enter_duration = config->duration / 3; // 进入阶段占总时长的1/3
    LISAUI_LOGI(TAG, "[%s] Creating timer with duration: %dms", __FUNCTION__, enter_duration);
    
    anim_state->stage_timer = lv_timer_create(emoji_stage_timer_cb, enter_duration, view);
    if (anim_state->stage_timer) {
        lv_timer_set_repeat_count(anim_state->stage_timer, 1); // 只执行一次
        LISAUI_LOGI(TAG, "[%s] ✓ Timer created successfully - emoji: %d, duration: %dms, timer: %p", 
                   __FUNCTION__, emoji_type, enter_duration, anim_state->stage_timer);
        LISAUI_LOGI(TAG, "[%s] === Animation Process Complete ===", __FUNCTION__);
    } else {
        LISAUI_LOGE(TAG, "[%s] ✗ Failed to create stage timer for emoji %d", __FUNCTION__, emoji_type);
        anim_state->is_running = false;
    }
}

/**
 * @brief 停止分阶段emoji动画
 */
static void stop_staged_emoji_animation(page_view_t *view)
{
    if (!view) {
        return;
    }
    
    emoji_anim_state_t *anim_state = &view->anim_state;
    
    if (anim_state->stage_timer) {
        lv_timer_del(anim_state->stage_timer);
        anim_state->stage_timer = NULL;
        LISAUI_LOGI(TAG, "Stopped staged animation timer");
    }
    
    if (view->inter) {
        lisa_ui_llm_primary_stop_emoji_animation(view->inter);
    }
    
    anim_state->current_stage = EMOJI_ANIM_STAGE_IDLE;
    anim_state->is_running = false;
    anim_state->force_exit = false;
}

int current_staged_emoji_animation_stop(void)
{
    if (!g_current_primary_view) {
        return -1;
    }
    
    emoji_anim_state_t *anim_state = &g_current_primary_view->anim_state;
    
    if (anim_state->stage_timer) {
        lv_timer_del(anim_state->stage_timer);
        anim_state->stage_timer = NULL;
        LISAUI_LOGI(TAG, "Stopped staged current animation timer");
    }
    
    if (g_current_primary_view->inter) {
        lisa_ui_llm_primary_stop_emoji_animation(g_current_primary_view->inter);
    }
    
    anim_state->current_stage = EMOJI_ANIM_STAGE_IDLE;
    anim_state->is_running = false;
    anim_state->force_exit = false;
}

/**
 * @brief 请求退出当前动画
 */
static void request_exit_staged_animation(page_view_t *view)
{
    if (!view) {
        return;
    }
    
    emoji_anim_state_t *anim_state = &view->anim_state;
    
    if (anim_state->is_running && anim_state->current_stage == EMOJI_ANIM_STAGE_LOOP) {
        anim_state->force_exit = true;
        LISAUI_LOGI(TAG, "Requested exit from loop stage");
    }
}

// ===================== 定时器回调函数 =====================

/**
 * @brief 犯困表情定时器回调函数
 */
static void sleepy_timer_cb(lv_timer_t *timer)
{
    page_view_t *view = (page_view_t *)timer->user_data;
    
    if (!view || !view->inter) {
        return;
    }
    
    // 切换到犯困表情
    view->current_emoji = LISA_UI_EMOJI_SLEEPY;
    view->is_sleepy_mode = true;
    
    set_emoji_animation_by_type(view->inter, view->current_emoji);
    lisa_ui_llm_primary_start_emoji_animation(view->inter);
    lisa_display_set_brightness(lisa_display_get(), 10);  // 设置屏幕亮度为10%
    
    LISAUI_LOGI(TAG, "Switch to sleepy mode");
    
    // 停止并删除定时器
    lv_timer_del(view->sleepy_timer);
    view->sleepy_timer = NULL;
}

/**
 * @brief 启动犯困表情定时器
 */
static void start_sleepy_timer(page_view_t *view)
{
    if (!view) {
        return;
    }
    
    // 如果已经有定时器在运行，先停止它
    if (view->sleepy_timer) {
        lv_timer_del(view->sleepy_timer);
        view->sleepy_timer = NULL;
    }
    
    // 重置犯困模式状态并恢复亮度
    if (view->is_sleepy_mode) {
        lisa_display_set_brightness(lisa_display_get(), 80);  // 恢复亮度为80%
        LISAUI_LOGI(TAG, "Wake up from sleepy mode, brightness restored to 80%%");
    }
    view->is_sleepy_mode = false;
    
    // 创建新的定时器
    view->sleepy_timer = lv_timer_create(sleepy_timer_cb, SLEEPY_EMOJI_DELAY_MS, view);
    if (view->sleepy_timer) {
        lv_timer_set_repeat_count(view->sleepy_timer, 1);  // 只执行一次
        LISAUI_LOGI(TAG, "Sleepy timer started, will trigger in %d ms", SLEEPY_EMOJI_DELAY_MS);
    }
}

/**
 * @brief 停止犯困表情定时器
 */
static void stop_sleepy_timer(page_view_t *view)
{
    if (!view) {
        return;
    }
    
    if (view->sleepy_timer) {
        lv_timer_del(view->sleepy_timer);
        view->sleepy_timer = NULL;
        LISAUI_LOGI(TAG, "Sleepy timer stopped");
    }
    
    // 重置犯困模式状态并恢复亮度
    if (view->is_sleepy_mode) {
        lisa_display_set_brightness(lisa_display_get(), 80);  // 恢复亮度为80%
        LISAUI_LOGI(TAG, "Wake up from sleepy mode, brightness restored to 80%%");
    }
    view->is_sleepy_mode = false;
}

static const char *make_wake_hint_text(const char *base_text, const char *wake_word)
{
    static char wake_hint[CONTENT_TEXT_MAX_LEN];

    if (!base_text) {
        return "";
    }

    if (!wake_word || strlen(wake_word) == 0) {
        wake_word = "小聆小聆";
    }

    const char *placeholder = "#唤醒词#";
    char *pos = strstr(base_text, placeholder);
    if (pos) {
        size_t prefix_len = pos - base_text;
        size_t wake_word_len = strlen(wake_word);
        size_t suffix_len = strlen(pos + strlen(placeholder));

        if (prefix_len + wake_word_len + suffix_len > CONTENT_TEXT_MAX_LEN - 1) {
            return base_text; // 超出长度限制，返回原文本
        }

        char *p = wake_hint;
        strncpy(p, base_text, prefix_len);
        p += prefix_len;
        strncpy(p, wake_word, wake_word_len);
        p += wake_word_len;
        strncpy(p, pos + strlen(placeholder), suffix_len);
        p += suffix_len;
        *p = '\0';
    } else {
        strncpy(wake_hint, base_text, CONTENT_TEXT_MAX_LEN - 1);
        wake_hint[CONTENT_TEXT_MAX_LEN - 1] = '\0';
    }

    return wake_hint;
}

// ===================== 文本轮换定时器函数 =====================

/**
 * @brief 文本轮换定时器回调函数
 */
static void text_rotate_timer_cb(lv_timer_t *timer)
{
    page_view_t *view = (page_view_t *)timer->user_data;
    
    if (!view || !view->inter || !view->is_text_rotating) {
        return;
    }
    
    uint32_t text_count = 0;
    const char *current_text = NULL;
    // const char *role_name = NULL;
    const char *wake_word = NULL;
    
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        // 获取当前角色名称用于替换
        // role_name = _userdata->roles.roles[_userdata->roles.role_idx].name;
        wake_word = _userdata->setting.wake_word;
        
        // 优先使用云端下发的待机文本
        if (_userdata->standby_texts.is_enabled && _userdata->standby_texts.text_count > 0) {
            text_count = _userdata->standby_texts.text_count;
            // 切换到下一个云端文本
            view->current_text_index = (view->current_text_index + 1) % text_count;
            current_text = _userdata->standby_texts.texts[view->current_text_index];
        } else {
            // 没有云端文本，这种情况下不应该进行轮换
            // 因为角色提示文本是静态的，不需要轮换
            LISAUI_LOGW(TAG, "No cloud texts available, should not be in rotation mode");
            current_text = "提示词获取失败";
        }
    }
    
    // 更新显示文本（应用角色名称替换）
    if (current_text) {
        const char *wake_hint = make_wake_hint_text(current_text, wake_word);
        set_content_text(view, wake_hint);  //更新文本
        // LISAUI_LOGI(TAG, "Text rotated to index %d: %s (original)", view->current_text_index, current_text);
    }
}

/**
 * @brief 启动文本轮换
 */
static void start_text_rotation(page_view_t *view)
{
    if (!view) {
        return;
    }
    
    // 如果已经有定时器在运行，先停止它
    if (view->text_rotate_timer) {
        lv_timer_del(view->text_rotate_timer);
        view->text_rotate_timer = NULL;
    }
    
    uint32_t interval_ms = STANDBY_TEXT_ROTATE_INTERVAL_MS;  // 默认间隔
    const char *first_text = NULL;
    // const char *role_name = NULL;
    const char *wake_word = NULL;
    bool should_start = false;
    
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        // 获取当前角色名称用于替换
        // role_name = _userdata->roles.roles[_userdata->roles.role_idx].name;
        wake_word = _userdata->setting.wake_word;
        
        // 检查云端文本配置
        if (_userdata->standby_texts.is_enabled && _userdata->standby_texts.text_count > 0) {
            // 使用云端配置
            interval_ms = _userdata->standby_texts.interval_ms > 0 ? 
                         _userdata->standby_texts.interval_ms : STANDBY_TEXT_ROTATE_INTERVAL_MS;
            first_text = _userdata->standby_texts.texts[0];
            should_start = true;
            LISAUI_LOGI(TAG, "Using cloud standby texts, count: %d, interval: %d ms", 
                       _userdata->standby_texts.text_count, interval_ms);
        } else {
            // 使用角色提示文本（不轮换）
            char *prompt = _userdata->roles.roles[_userdata->roles.role_idx].prompt;
            first_text = prompt ? prompt : "提示词不存在";
            should_start = false;  // 角色提示文本不轮换
            LISAUI_LOGI(TAG, "Using role prompt text (no rotation): %s", first_text);
        }
    }
    
    // 重置文本轮换状态
    view->current_text_index = 0;  // 从第一个文本开始
    view->is_text_rotating = should_start;
    
    // 立即显示第一个文本（应用角色名称替换）
    if (first_text) {
        const char *wake_hint = make_wake_hint_text(first_text, wake_word);
        set_content_text(view, wake_hint); // 更新文本
    }
    
    // 如果需要轮换且有多个文本，创建定时器
    if (should_start) {
        view->text_rotate_timer = lv_timer_create(text_rotate_timer_cb, interval_ms, view);
        if (view->text_rotate_timer) {
            lv_timer_set_repeat_count(view->text_rotate_timer, -1);  // 无限循环
            LISAUI_LOGI(TAG, "Text rotation started, interval: %d ms", interval_ms);
        }
    } else {
        LISAUI_LOGI(TAG, "Text rotation not started (using static role prompt)");
    }
}

/**
 * @brief 停止文本轮换
 */
static void stop_text_rotation(page_view_t *view)
{
    if (!view) {
        return;
    }
    
    if (view->text_rotate_timer) {
        lv_timer_del(view->text_rotate_timer);
        view->text_rotate_timer = NULL;
        LISAUI_LOGI(TAG, "Text rotation stopped");
    }
    
    view->is_text_rotating = false;
}

// ===================== 状态管理函数 =====================

/**
 * @brief 更新任务栏状态
 */
static void update_taskbar_status(page_view_t *view)
{
    if (!view || !view->inter) {
        return;
    }
    
    // 更新WiFi状态
    lisa_ui_llm_primary_set_wifi_img(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_wifi)[0]);
    
    // 更新电量状态
    lisa_ui_llm_primary_set_battery_img(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_power)[9]);
    
    // 更新交互模式图标
    lisa_ui_llm_primary_set_interactive_mode_img(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_interactive)[0]);

    // 更新闹钟图标
    lisa_ui_llm_primary_set_alarm_img(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_alarm)[0]);

    // 更新状态文本
    lisa_ui_llm_primary_set_status_text(view->inter, view->current_status);
}

/**
 * @brief 更新内容显示（emoji + 文本）
 */
static void update_content_display(page_view_t *view)
{
    if (!view || !view->inter) {
        return;
    }
    
    // 设置emoji动画
    set_emoji_animation_by_type(view->inter, view->current_emoji);
    lisa_ui_llm_primary_start_emoji_animation(view->inter);
    
    // 更新内容文本
    lisa_ui_llm_primary_set_content_text(view->inter, view->content_text);
}

/**
 * @brief 设置页面状态文本
 */
static void set_page_status(page_view_t *view, const char *status)
{
    if (!view || !status) {
        return;
    }
    
    strncpy(view->current_status, status, sizeof(view->current_status) - 1);
    view->current_status[sizeof(view->current_status) - 1] = '\0';
    
    if (view->inter) {
        lisa_ui_llm_primary_set_status_text(view->inter, view->current_status);
    }
}

static void set_content_text(page_view_t *view, const char *text)
{
    if (!view || !text) {
        return;
    }

    strncpy(view->content_text, text, sizeof(view->content_text) - 1);
    view->content_text[sizeof(view->content_text) - 1] = '\0';

    if (view->inter) {
        lisa_ui_llm_primary_set_content_text(view->inter, text);
    }
}

static void add_content_text(page_view_t *view, const char *text)
{
    if (!view || !text) {
        return;
    }

    strncpy(view->content_text, text, sizeof(view->content_text) - 1);
    view->content_text[sizeof(view->content_text) - 1] = '\0';

    if (view->inter) {
        lisa_ui_llm_primary_add_content_text(view->inter, text);
    }
}

/**
 * @brief 设置内容文本和emoji
 */
static void set_content_display(page_view_t *view, const char *text, lisa_ui_emoji_type_e emoji)
{
    if (!view) {
        return;
    }
    
    if (text) {
        strncpy(view->content_text, text, sizeof(view->content_text) - 1);
        view->content_text[sizeof(view->content_text) - 1] = '\0';
    }
    
    view->current_emoji = emoji;
    
    if (view->inter) {
        update_content_display(view);
    }
}

static void commu_page_enter(page_view_t *view, const char *img_name,bool is_push_stack)
{
    uint8_t role_founded = 0;

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        for (int i = 0; i < _userdata->roles.roles_count; i++) {
            if (0 == strcmp(img_name, _userdata->roles.roles[i].name)) {
                _userdata->roles.role_idx = i;
                role_founded = 1;
                break;
            }
        }
    }

    if (role_founded) {

        LISAUI_LOGI(TAG, "imgs_click_event_cb enter commu page\n");

        ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_INTER_ROLE_UPDATE, NULL, 0);
        set_content_display(view, "", LISA_UI_EMOJI_WAKEUP);
    }
}

/**
 * @brief 替换文本中的标准唤醒词为当前角色名称
 * @param text 原始文本
 * @param role_name 当前配置的角色名称
 * @return 替换后的文本（需要调用者释放内存）
 */
/*static char *replace_role_name_with_wakeup_word(const char *text, const char *role_name)
{
    if (!text) {
        return NULL;
    }
    
    // 如果角色名称为空或者就是"小聆"，则不需要替换
    if (!role_name || strcmp(role_name, "小聆") == 0) {
        // 直接返回原文本的副本
        size_t text_len = strlen(text);
        char *result = lisaui_malloc(text_len + 1);
        if (result) {
            strcpy(result, text);
        }
        return result;
    }
    
    // 构建需要替换的目标模式：角色名称+角色名称
    char replacement[64];
    snprintf(replacement, sizeof(replacement), "%s%s", role_name, role_name);
    
    // 查找并替换"小聆小聆"为"角色名称+角色名称"
    const char *pattern = "小聆小聆";
    const char *pos = strstr(text, pattern);
    if (!pos) {
        // 没有找到"小聆小聆"，返回原文本副本
        size_t text_len = strlen(text);
        char *result = lisaui_malloc(text_len + 1);
        if (result) {
            strcpy(result, text);
        }
        return result;
    }
    
    // 计算替换后的长度
    size_t pattern_len = strlen(pattern);
    size_t replacement_len = strlen(replacement);
    size_t original_len = strlen(text);
    size_t new_len = original_len - pattern_len + replacement_len;
    
    // 分配新的内存
    char *result = lisaui_malloc(new_len + 1);
    if (!result) {
        return NULL;
    }
    
    // 复制前半部分
    size_t prefix_len = pos - text;
    strncpy(result, text, prefix_len);
    result[prefix_len] = '\0';
    
    // 添加替换文本
    strcat(result, replacement);
    
    // 添加后半部分
    strcat(result, pos + pattern_len);
    
    LISAUI_LOGI(TAG, "Role name replacement: '小聆小聆' -> '%s' in text", replacement);
    
    return result;
}*/

static char *get_role_prompt(void)
{
    // 获取当前主页面视图
    page_view_t *view = g_current_primary_view;
    
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        // 优先使用云端下发的待机文本
        if (_userdata->standby_texts.is_enabled && _userdata->standby_texts.text_count > 0) {
            if (view && view->is_text_rotating) {
                // 如果正在轮换文本，返回当前轮换的云端文本
                uint32_t index = view->current_text_index % _userdata->standby_texts.text_count;
                return _userdata->standby_texts.texts[index];
            } else {
                // 如果没有轮换，返回第一个云端文本
                return _userdata->standby_texts.texts[0];
            }
        } else {
            // 没有云端文本时，使用角色提示文本
            char *prompt = _userdata->roles.roles[_userdata->roles.role_idx].prompt;
            return prompt ? prompt : "提示词不存在";
        }
    }
    
    // 默认返回备用文本（不应该到达这里）
    return "系统初始化中...";
}

static int update_inter_state(page_view_t *view)
{
    static bool last_local_state = LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE;

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->setting.loading_text != NULL) {
            continue;
        }

        if (_userdata->setting.mic_is_mute) {
            set_page_status(view, "请打开麦克风跟我说话");
        } else {
            set_page_status(view, remote_inter_state_map[_userdata->inter.remote_state]);

            switch (_userdata->inter.local_state) {
            case LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE: {
                /* 防止未唤醒状态下，双击按键拍照，图片被隐藏 */
                if (last_local_state != LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE)
                    lisa_ui_llm_primary_hide_camera_image(view->inter);

                // 仅当 remote_state 也是 IDLE 时才设置"请唤醒我"，否则保持 remote_state 的状态（如"思考中"、"说话中"）
                // 这样可以避免从其他页面（如二维码页面）返回主页时丢失云端的实时交互状态
                if (_userdata->inter.local_state == LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE &&
                    _userdata->inter.remote_state == LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE) {
                    set_page_status(view, "请唤醒我");
                }  
                start_text_rotation(view);  // 启动文本轮换
                break;
            }
            case LISAUI_USERDATA_INTER_LOCAL_STATE_RECOGNITION:
                stop_text_rotation(view);   // 停止文本轮换
                if (_userdata->inter.iat_text != NULL) {
                    set_content_text(view, _userdata->inter.iat_text);
                }

                // if (_userdata->inter.remote_state == LISAUI_USERDATA_INTER_REMOTE_STATE_THINKING) {
                //     set_content_text(view, _userdata->inter.reply_text);
                // }
                break;
            default:
                break;
            }

            last_local_state = _userdata->inter.local_state;

            LISAUI_LOGI(TAG, "lisaui update inter state,remote:%d,local:%d,iat_text:%s", _userdata->inter.remote_state,
                        _userdata->inter.local_state, _userdata->inter.iat_text);
        }
    }
    
    return 0;
}

static int update_mcp_emoji(page_view_t *view)
{
    if (!view) {
        LISAUI_LOGE(TAG, "[%s] Invalid view parameter", __FUNCTION__);
        return -EINVAL;
    }

    LISAUI_LOGI(TAG, "[%s] === MCP Emoji Update Process Start ===", __FUNCTION__);
    LISAUI_LOGI(TAG, "[%s] Current view state - current_emoji: %d, anim_running: %d", 
               __FUNCTION__, view->current_emoji, view->anim_state.is_running);

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        uint8_t role_idx = _userdata->roles.role_idx;
        uint8_t emoji = _userdata->roles.roles[role_idx].emoji;
        
        LISAUI_LOGI(TAG, "[%s] MCP data - role_idx: %d, raw_emoji: %d", __FUNCTION__, role_idx, emoji);
        
        if (_userdata->setting.loading_text != NULL) {
            LISAUI_LOGI(TAG, "[%s] Loading in progress, skipping MCP emoji update", __FUNCTION__);
            continue;
        }
        
        // 映射表情值
        lisa_ui_emoji_type_e emoji_type;
        
        // 将角色表情映射到UI表情
        switch (emoji) {
            case ROLE_EMOJI_UNKNOW:
                emoji_type = LISA_UI_EMOJI_UNKNOW;
                break;
            case ROLE_EMOJI_LOVE:
                emoji_type = LISA_UI_EMOJI_LOVE;
                break;
            case ROLE_EMOJI_SAD:
                emoji_type = LISA_UI_EMOJI_SAD;
                break;
            case ROLE_EMOJI_LAUGH:
                emoji_type = LISA_UI_EMOJI_LAUGH;
                break;
            case ROLE_EMOJI_SQUINT:
                emoji_type = LISA_UI_EMOJI_SQUINT;
                break;
            case ROLE_EMOJI_ANGRY:
                emoji_type = LISA_UI_EMOJI_ANGRY;
                break;
            case ROLE_EMOJI_EYE:
                emoji_type = LISA_UI_EMOJI_EYE;
                break;
            case ROLE_EMOJI_BLINK:
                emoji_type = LISA_UI_EMOJI_BLINK;
                break;
            case ROLE_EMOJI_HAPPY:
                emoji_type = LISA_UI_EMOJI_HAPPY;
                break;
            case ROLE_EMOJI_CUTE:
                emoji_type = LISA_UI_EMOJI_CUTE;
                break;
            default:
                emoji_type = LISA_UI_EMOJI_BLINK;  // 默认使用眨眼表情
                break;
        }
        
        LISAUI_LOGI(TAG, "[%s] MCP Emoji mapping - raw: %d -> mapped: %d", __FUNCTION__, emoji, emoji_type);
        
        // 确保emoji_type在有效范围内
        if (emoji_type >= LISA_UI_EMOJI_MAX) {
            LISAUI_LOGW(TAG, "[%s] Invalid emoji value: %d, using default blink", __FUNCTION__, emoji_type);
            emoji_type = LISA_UI_EMOJI_BLINK;  // 默认使用眨眼表情
        }

        // 检查是否为重复设置相同表情，无论动画是否在运行都强制重新启动
        if (view->current_emoji == emoji_type) {
            LISAUI_LOGI(TAG, "[%s] REPEAT DETECTION: Same emoji %d detected (current_running: %d, stage: %d)", 
                       __FUNCTION__, emoji_type, view->anim_state.is_running, view->anim_state.current_stage);
            LISAUI_LOGI(TAG, "[%s] Force stopping current animation before restart", __FUNCTION__);
            stop_staged_emoji_animation(view);
        } else {
            LISAUI_LOGI(TAG, "[%s] NEW EMOJI: Changing from %d to %d", __FUNCTION__, view->current_emoji, emoji_type);
        }
        
        LISAUI_LOGI(TAG, "[%s] Starting staged emoji animation for type: %d", __FUNCTION__, emoji_type);
        // 使用新的分阶段动画系统
        start_staged_emoji_animation(view, emoji_type);
        
        // MCP表情启动后立即设置保护标志，防止被云端表情打断
        view->anim_state.mcp_protected = true;
        LISAUI_LOGI(TAG, "[%s] MCP protection enabled for emoji: %d", __FUNCTION__, emoji_type);

        LISAUI_LOGI(TAG, "[%s] === MCP Emoji Update Process Complete === role_idx:%d, emoji:%d, mapped_emoji:%d", 
                   __FUNCTION__, role_idx, emoji, emoji_type);
    }
    
    return 0;
}

static int update_role_emoji(page_view_t *view)
{
    if (!view) {
        LISAUI_LOGE(TAG, "[%s] Invalid view parameter", __FUNCTION__);
        return -EINVAL;
    }

    LISAUI_LOGI(TAG, "[%s] === Cloud Emoji Update Process Start ===", __FUNCTION__);
    LISAUI_LOGI(TAG, "[%s] Current view state - current_emoji: %d, anim_running: %d", 
               __FUNCTION__, view->current_emoji, view->anim_state.is_running);

    // 检查MCP保护标志
    if (view->anim_state.mcp_protected) {
        LISAUI_LOGI(TAG, "[%s] Cloud emoji blocked by MCP protection", __FUNCTION__);
        return 0;  // 被MCP保护，不处理云端表情
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->setting.loading_text != NULL) {
            LISAUI_LOGI(TAG, "[%s] Loading in progress, skipping cloud emoji update", __FUNCTION__);
            continue;
        }

        uint8_t role_idx = _userdata->roles.role_idx;
        uint8_t emoji = _userdata->roles.roles[role_idx].emoji;
        
        LISAUI_LOGI(TAG, "[%s] Cloud data - role_idx: %d, raw_emoji: %d", __FUNCTION__, role_idx, emoji);
        
        // 映射表情值
        lisa_ui_emoji_type_e emoji_type;
        
        // 将角色表情映射到UI表情
        switch (emoji) {
            case ROLE_EMOJI_UNKNOW:
                emoji_type = LISA_UI_EMOJI_UNKNOW;
                break;
            case ROLE_EMOJI_LOVE:
                emoji_type = LISA_UI_EMOJI_LOVE;
                break;
            case ROLE_EMOJI_SAD:
                emoji_type = LISA_UI_EMOJI_SAD;
                break;
            case ROLE_EMOJI_LAUGH:
                emoji_type = LISA_UI_EMOJI_LAUGH;
                break;
            case ROLE_EMOJI_SQUINT:
                emoji_type = LISA_UI_EMOJI_SQUINT;
                break;
            case ROLE_EMOJI_ANGRY:
                emoji_type = LISA_UI_EMOJI_ANGRY;
                break;
            case ROLE_EMOJI_EYE:
                emoji_type = LISA_UI_EMOJI_EYE;
                break;
            case ROLE_EMOJI_BLINK:
                emoji_type = LISA_UI_EMOJI_BLINK;
                break;
            case ROLE_EMOJI_HAPPY:
                emoji_type = LISA_UI_EMOJI_HAPPY;
                break;
            case ROLE_EMOJI_CUTE:
                emoji_type = LISA_UI_EMOJI_CUTE;
                break;
            default:
                emoji_type = LISA_UI_EMOJI_BLINK;  // 默认使用眨眼表情
                break;
        }
        
        LISAUI_LOGI(TAG, "[%s] Cloud Emoji mapping - raw: %d -> mapped: %d", __FUNCTION__, emoji, emoji_type);
        
        // 确保emoji_type在有效范围内
        if (emoji_type >= LISA_UI_EMOJI_MAX) {
            LISAUI_LOGW(TAG, "[%s] Invalid cloud emoji value: %d, using default blink", __FUNCTION__, emoji_type);
            emoji_type = LISA_UI_EMOJI_BLINK;  // 默认使用眨眼表情
        }
        
        // 检查是否为重复设置相同表情，无论动画是否在运行都强制重新启动
        if (view->current_emoji == emoji_type) {
            LISAUI_LOGI(TAG, "[%s] REPEAT DETECTION: Same emoji %d detected (current_running: %d, stage: %d)", 
                       __FUNCTION__, emoji_type, view->anim_state.is_running, view->anim_state.current_stage);
            LISAUI_LOGI(TAG, "[%s] Force stopping current animation before restart", __FUNCTION__);
            stop_staged_emoji_animation(view);
        } else {
            LISAUI_LOGI(TAG, "[%s] NEW EMOJI: Changing from %d to %d", __FUNCTION__, view->current_emoji, emoji_type);
        }
        
        LISAUI_LOGI(TAG, "[%s] Starting staged emoji animation for type: %d", __FUNCTION__, emoji_type);
        // 使用新的分阶段动画系统
        start_staged_emoji_animation(view, emoji_type);

        LISAUI_LOGI(TAG, "[%s] === Cloud Emoji Update Process Complete === role_idx:%d, emoji:%d, mapped_emoji:%d", 
                   __FUNCTION__, role_idx, emoji, emoji_type);
    }
    
    return 0;
}

static inline uint8_t wifi_rssi_to_level(int rssi)
{
   if (rssi >= -60)       return 2;  // 强 (-60dBm 及以上)
   else if (rssi >= -75)  return 1;  // 中等   (-75dBm 到 -61dBm)
   else                   return 0;  // 非常弱 (-76dBm 及以下)
}

static int update_wifi_state(page_view_t *view)
{
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->setting.loading_text != NULL) {
            continue;
        }

        uint8_t connect_state = _userdata->setting.wifi.connect_info.state;

        if (connect_state == LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECTED) {
            view->wifi_connected = true;
            lisa_ui_llm_primary_set_wifi_img(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_wifi)[4]);
        }
        else {
            view->wifi_connected = false;
            lisa_ui_llm_primary_set_wifi_img(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_wifi)[0]);
            set_page_status(view, "网络未连接");
            stop_text_rotation(view);   // 停止文本轮换
            set_content_text(view, "请连接网络");
            start_staged_emoji_animation(view, LISA_UI_EMOJI_WAIT);
        }
    }
    
    return 0;
}

static int update_battery_info(page_view_t *view)
{
    uint8_t battery_level;
    
    if (!view) {
        return -1;
    }
    
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->setting.loading_text != NULL) {
            continue;
        }

        uint8_t is_usb_plug = _userdata->setting.battery.usb_status;
        uint8_t is_charging = _userdata->setting.battery.is_charging;
        if (_userdata->setting.battery.power_percent >= 100) {
            battery_level = 9;
        } else {
            battery_level = _userdata->setting.battery.power_percent / 10;
        }

        if (!get_show_battery_status()) {
            lisa_ui_llm_primary_battery_icon_hide(view->inter);
            continue;
        } else {
            if (is_charging) {
                lisa_ui_llm_primary_set_battery_img(view->inter, &LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_charging)[battery_level]);
            } else {
                lisa_ui_llm_primary_set_battery_img(view->inter, &LISA_UI_ASSETS_IMG_DSC_LIST_GET(img_png_power)[battery_level]);
            }
        }

        if (_userdata->setting.ota.state != OTA_STATE_UP_TO_DATE) {
            // 正在更新时不处理充电表情
            continue;
        }

        if (is_usb_plug) {
            // 检测充电状态变化
            if (view->last_charging_state == 0) {
                // 刚开始充电 - 显示开始充电表情
                set_content_text(view, "开始充电...");
                start_staged_emoji_animation(view, LISA_UI_EMOJI_START_BATTERY_CHANGE);
                LISAUI_LOGI(TAG, "Battery charging started");
            } else {
                // 持续充电中 - 不显示任何充电表情，保持当前状态
                // 如果当前正在显示充电开始动画，让它自然完成后恢复到默认状态
                if (view->current_emoji == LISA_UI_EMOJI_START_BATTERY_CHANGE) {
                    // 充电开始动画会自动完成，不需要额外处理
                    LISAUI_LOGI(TAG, "Charging start animation in progress");
                }
            }
            view->last_charging_state = 1;
        }
        else {
            // 检测充电结束
            if (view->last_charging_state == 1) {
                // 充电刚结束 - 显示结束充电表情
                set_content_text(view, "充电完成");
                start_staged_emoji_animation(view, LISA_UI_EMOJI_START_BATTERY_CHANGE);
                LISAUI_LOGI(TAG, "Battery charging ended");
                view->last_charging_state = 0;
            } else {
                // 如果当前是任何充电相关表情，且不再充电，则恢复到默认状态
                if (view->current_emoji == LISA_UI_EMOJI_START_BATTERY_CHANGE ) {
                    // 恢复到待机状态的表情和文本
                    start_text_rotation(view);  // 启动文本轮换
                    start_staged_emoji_animation(view, LISA_UI_EMOJI_BLINK);
                }
            }
        }
    }
    return 0;
}

static int update_ota_state(page_view_t *view)
{
    static char progress_text[32] = {0};

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        ota_state_t *ota = &_userdata->setting.ota;

        if (ota->state == OTA_STATE_UP_TO_DATE) {
            // 仅当交互状态都是 IDLE 时才设置"请唤醒我"，否则保持云端的实时交互状态
            // 这样可以避免从其他页面返回主页时丢失云端的实时交互状态
            if (_userdata->inter.local_state == LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE &&
                _userdata->inter.remote_state == LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE) {
                set_page_status(view, "请唤醒我");
            }
            start_text_rotation(view);  // 启动文本轮换
            start_staged_emoji_animation(view, LISA_UI_EMOJI_BLINK);
            start_sleepy_timer(view);
        } else {
            if (ota->state == OTA_STATE_CHECKING) {
                set_page_status(view, "检查更新中…");
                set_content_text(view, "");
            } else if (ota->state == OTA_STATE_UPDATING) {
                set_page_status(view, "正在更新…");
                if (ota->bytes_total == 0) {
                    set_content_text(view, "");
                } else {
                    snprintf(progress_text, sizeof(progress_text), "%.1f%%",
                             (float)ota->bytes_processed / (float)ota->bytes_total * 100.0f);
                    set_content_text(view, progress_text);
                }
            } else if (ota->state == OTA_STATE_SUCCESSED) {
                set_page_status(view, "更新完毕");
                set_content_text(view, "");
            } else if (ota->state == OTA_STATE_FAILED) {
                set_page_status(view, "更新失败");
                set_content_text(view, "");
            }

            if (ota->state == OTA_STATE_SUCCESSED || ota->state == OTA_STATE_FAILED) {
                if (ota->reboot == OTA_REBOOT_STRATEGY_AUTO) {
                    set_content_text(view, "正在重启...");
                } else {
                    set_content_text(view, "请手动重启设备");
                }
            }

            stop_text_rotation(view);   // 停止文本轮换
            start_staged_emoji_animation(view, LISA_UI_EMOJI_LOVE);
        }
    }

    return 0;
}

static int update_loading_state(page_view_t *view)
{
    bool has_loading = false;
    bool ota_up_to_date = true;
    char loading_text_buf[CONTENT_TEXT_MAX_LEN];

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        has_loading = (_userdata->setting.loading_text != NULL);
        ota_up_to_date = (_userdata->setting.ota.state == OTA_STATE_UP_TO_DATE);
        if (_userdata->setting.loading_text) {
            strncpy(loading_text_buf, _userdata->setting.loading_text, sizeof(loading_text_buf) - 1);
            loading_text_buf[sizeof(loading_text_buf) - 1] = '\0';
        }
    }

    if (has_loading) {
        if (!ota_up_to_date) {
            return 0;
        }

        set_content_text(view, loading_text_buf);
        start_staged_emoji_animation(view, LISA_UI_EMOJI_WAIT);
        stop_text_rotation(view);
        stop_sleepy_timer(view);
        return 0;
    }

    update_inter_state(view);
    update_wifi_state(view);
    update_battery_info(view);
    update_ota_state(view);
    update_role_emoji(view);

    return 0;
}

static int update_interactive_mode_state(page_view_t *view)
{
    if (!view || !view->inter) {
        return 0;
    }

    bool should_show = false;

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        switch (_userdata->setting.inter_mode) {
        case LISAUI_USERDATA_SETTING_INTER_MODE_DUAL:
            should_show = true;
            break;
        case LISAUI_USERDATA_SETTING_INTER_MODE_HALF:
        case LISAUI_USERDATA_SETTING_INTER_MODE_KEY:
        default:
            should_show = false;
            break;
        }
    }

    if (should_show) {
        lisa_ui_llm_primary_interactive_mode_icon_show(view->inter);
    } else {
        lisa_ui_llm_primary_interactive_mode_icon_hide(view->inter);
    }

    return 0;
}


static int update_alarm_state(page_view_t *view)
{
    if (!view || !view->inter) {
        return 0;
    }

    bool should_show = false;

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->setting.has_alarm){
            should_show = true;
        }
    }

    if (should_show) {
        lisa_ui_llm_primary_alarm_icon_show(view->inter);
    } else {
        lisa_ui_llm_primary_alarm_icon_hide(view->inter);
    }

    return 0;
}

static int event_inter_state_handler(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size, void *user_data)
{
    page_view_t *view = (page_view_t *)user_data;

    if (view == NULL) {
        return -1;
    }
    
    LISAUI_LOGD(TAG, "page_primary event_inter_state_handler, code:%d", code);
    
    switch (code) {
    case LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP: {
        const char *role_name = NULL;
        LISAUI_USERDATA_WITH_LOCK(_userdata)
        {
            role_name = _userdata->roles.roles[_userdata->roles.role_idx].name;
        }
        stop_sleepy_timer(view);
        stop_text_rotation(view);  // 停止文本轮换
        commu_page_enter(
            view, role_name,
            (lisaui_manager_get_current_group()->current_page->attribute.type == LISAUI_PAGE_TYPE_PRIMARY_PAGE)
                ? true
                : false);
    } break;

    case LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE:
        update_inter_state(view);
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_ROLE_EMOJI_UPDATE:
        update_role_emoji(view);
        break;
    
    case LISAUI_EBUS_CH_EVENT_M2U_MCP_EMOJI_UPDATE:
        update_mcp_emoji(view);
        break;
    
    case LISAUI_EBUS_CH_EVENT_M2U_INTER_END:
        update_role_emoji(view);
        // 启动犯困表情定时器和文本轮换
        start_sleepy_timer(view);
        start_text_rotation(view);  // 重新启动文本轮换
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_INTER_MODE_UPDATE:
        update_interactive_mode_state(view);
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_ALARM_UPDATE:
        update_alarm_state(view);
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE:
        update_wifi_state(view);
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_SETTING_BATTERY_UPDATE:
        update_battery_info(view);
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_OTA_STATE_UPDATE:
        update_ota_state(view);
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_STANDBY_TEXTS_UPDATE:
        // 待机文本配置更新，重新启动文本轮换
        if (view->wifi_connected) {
            stop_text_rotation(view);   // 停止当前轮换
            start_text_rotation(view);  // 使用新配置重新启动
            LISAUI_LOGI(TAG, "Standby texts updated, restarted text rotation");
        }
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_SHOW_LOADING:
        update_loading_state(view);
        break;

    default:
        LISAUI_LOGW(TAG, "Unknown event code: %d", code);
        break;
    }

    return 0;
}

static lisaui_page_t *create(lisaui_page_t *page)
{
    page_view_t *view = NULL;

    // 分配视图内存
    view = lisaui_malloc(sizeof(page_view_t));
    if (view == NULL) {
        LISAUI_LOGE(TAG, "[%s %d]no memory!", __FUNCTION__, __LINE__);
        goto _ERR;
    }

    // 初始化状态
    memset(view, 0, sizeof(page_view_t));
    view->wifi_connected = false;         // 默认WiFi已连接
    view->battery_level = 9;             // 默认满电
    view->current_emoji = LISA_UI_EMOJI_WAIT;
    view->sleepy_timer = NULL;           // 初始化定时器为空
    view->is_sleepy_mode = false;        // 初始化为非犯困模式
    view->last_charging_state = 0;       // 初始化充电状态
    
    // 初始化动画状态
    view->anim_state.current_emoji = LISA_UI_EMOJI_WAIT;
    view->anim_state.current_stage = EMOJI_ANIM_STAGE_IDLE;
    view->anim_state.stage_timer = NULL;
    view->anim_state.is_running = false;
    view->anim_state.force_exit = false;
    
    // 初始化文本轮换状态
    view->text_rotate_timer = NULL;      // 初始化文本轮换定时器为空
    view->current_text_index = 0;        // 初始化文本索引为0
    view->is_text_rotating = false;      // 初始化为非轮换状态

    // 创建主要UI组件
    view->inter = lisa_ui_llm_primary_create(NULL);
    if (view->inter == NULL) {
        LISAUI_LOGE(TAG, "Failed to create lisa_ui_llm_primary component");
        goto _ERR;
    }
    
    // 设置初始显示状态
    update_taskbar_status(view);
    update_content_display(view);
    // 更新显示内容
    set_page_status(view, "网络未连接");
    set_content_text(view, "请连接网络");
    start_staged_emoji_animation(view, LISA_UI_EMOJI_BLINK);
    update_interactive_mode_state(view);
    update_alarm_state(view);

    // 保存当前页面视图到全局变量
    g_current_primary_view = view;
    
    view->ebus_ch_base_event = ebus_chn_bind(LISAUI_EBUS_NAME, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    if (view->ebus_ch_base_event != NULL) {
        ebus_message_subscribe( view->ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP,
                        event_inter_state_handler, 
                        view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE,
                        event_inter_state_handler, 
                        view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_ROLE_EMOJI_UPDATE,
                        event_inter_state_handler, 
                        view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_MCP_EMOJI_UPDATE,
                        event_inter_state_handler, 
                        view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_INTER_END,
                        event_inter_state_handler, 
                        view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_INTER_MODE_UPDATE,
                        event_inter_state_handler, 
                        view);
                    
        ebus_message_subscribe( view->ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_ALARM_UPDATE,
                        event_inter_state_handler, 
                        view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE,
                        event_inter_state_handler, 
                        view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                EBUS_SUBSCRIBER_TYPE_SYNC, 
                LISAUI_EBUS_CH_EVENT_M2U_SETTING_BATTERY_UPDATE,
                event_inter_state_handler, 
                view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                EBUS_SUBSCRIBER_TYPE_SYNC, 
                LISAUI_EBUS_CH_EVENT_M2U_OTA_STATE_UPDATE,
                event_inter_state_handler, 
                view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                EBUS_SUBSCRIBER_TYPE_SYNC, 
                LISAUI_EBUS_CH_EVENT_M2U_STANDBY_TEXTS_UPDATE,
                event_inter_state_handler, 
                view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                EBUS_SUBSCRIBER_TYPE_SYNC, 
                LISAUI_EBUS_CH_EVENT_M2U_SHOW_LOADING,
                event_inter_state_handler, 
                view);
    }

    page->view = view;
    
    LISAUI_LOGI(TAG, "Launcher main page create success:%p", page);
    return page;

_ERR:
    if (view) {
        if (view->inter) {
            lv_obj_del(view->inter);
        }
        if (view->ebus_ch_base_event) {
            ebus_message_unsubscribe(view->ebus_ch_base_event, event_inter_state_handler);
        }
        lisaui_free(view);
    }
    return NULL;
}

static lisaui_err_t destroy(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for destroy");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (page->view) {
        page_view_t *view = (page_view_t *)page->view;
        
        // 停止犯困表情定时器
        stop_sleepy_timer(view);
        
        // 停止分阶段emoji动画
        stop_staged_emoji_animation(view);
        
        // 停止文本轮换定时器
        stop_text_rotation(view);
        
        // 清理全局视图变量
        if (g_current_primary_view == view) {
            g_current_primary_view = NULL;
        }
        
        // 取消事件总线订阅
        if (view->ebus_ch_base_event) {
            ebus_message_unsubscribe(view->ebus_ch_base_event, event_inter_state_handler);
            view->ebus_ch_base_event = NULL;
            LISAUI_LOGI(TAG, "Event bus unsubscribed");
        }
        
        // 销毁UI组件
        if (view->inter) {
            lv_obj_del(view->inter);
            view->inter = NULL;
            LISAUI_LOGI(TAG, "UI component deleted");
        }

        // 释放视图内存
        lisaui_free(view);
        page->view = NULL;
    }

    if (page->page_data) {
        lisaui_free(page->page_data);
        page->page_data = NULL;
    }
    g_primary_page_is_active = false;
    LISAUI_LOGI(TAG, "Destroy %s page successfully", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t show(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (view->inter) {
        // 显示界面（使用LVGL标准API）
        lv_scr_load(view->inter);
        
        // 启动emoji动画
        trigger_primary_page_refresh();
        lisa_ui_llm_primary_start_emoji_animation(view->inter);

    }
    
    // 设置主页面为活动状态
    g_primary_page_is_active = true;

    // 请求角色数据更新
    ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_INTER_ROLE_EXIT, NULL, 0);
    
    LISAUI_LOGI(TAG, "Show %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t close(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page %s view not created", page->cname);
        return LISAUI_ERR_INVALID_PARAM;
    }

    // 停止犯困表情定时器
    stop_sleepy_timer(view);
    
    // 停止分阶段emoji动画
    stop_staged_emoji_animation(view);
    
    // 停止文本轮换定时器
    stop_text_rotation(view);
    
    // 隐藏拍照图片（页面切换时）
    if (view->inter) {
        lisa_ui_llm_primary_hide_camera_image(view->inter);
    }

    // 清除主页面活动状态
    g_primary_page_is_active = false;

    LISAUI_LOGI(TAG, "Close %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page, void *data)
{
    launcher_page_update_parameter_t *parameter = (launcher_page_update_parameter_t *)data;
    if (!page || !data) {
        LISAUI_LOGE(TAG, "Invalid parameters for update");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view || !view->inter) {
        LISAUI_LOGE(TAG, "Page %s view not created or inter is NULL", page->cname);
        return LISAUI_ERR_INVALID_PARAM;
    }

    LISAUI_LOGI(TAG, "Update %s page", page->cname);
    return LISAUI_ERR_OK;
}

static const lisaui_page_t luancher_main_page = {
    .group_index = LISAUI_GROUP_INDEX_LAUNCHER,
    .page_index = LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY,
    .cname = "launcher.main",
    .view = NULL,
    .page_data = NULL,
    .attribute =
        {
            .type = LISAUI_PAGE_TYPE_PRIMARY_PAGE,
        },
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};



/**
 * @brief 获取当前主页面视图
 */
static page_view_t *get_current_page_view(void)
{
    return g_current_primary_view;
}

/**
 * @brief 触发主页面刷新，用于页面切换后更新状态
 */
void trigger_primary_page_refresh(void)
{
    // 获取当前页面视图
    page_view_t *view = get_current_page_view();
    if (!view) {
        LISAUI_LOGW(TAG, "No current page view available for refresh");
        return;
    }
    
    LISAUI_LOGI(TAG, "Triggering primary page refresh after page toggle");
    // 更新交互状态
    update_inter_state(view);
    
    update_loading_state(view);
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->setting.loading_text) {
            return;
        }
    }
    
    // 更新角色表情
    update_role_emoji(view);
    
    // 重新更新WiFi状态
    update_wifi_state(view);
    
    // 重新更新电池状态
    update_battery_info(view);

    // 重新更新OTA状态
    update_ota_state(view);
    
    // 如果WiFi已连接，重新启动犯困定时器和文本轮换
    if (view->wifi_connected) {
        start_sleepy_timer(view);
        start_text_rotation(view);  // 重新启动文本轮换
        LISAUI_LOGI(TAG, "Restarted sleepy timer and text rotation after page refresh");
    }
}

/**
 * @brief Check if primary page is currently active
 * @return true if primary page is active (in show state), false otherwise
 */
bool is_primary_page_active(void)
{
    return g_primary_page_is_active;
}

/**
 * @brief Get primary page UI object
 * @return UI object pointer if page is active, NULL otherwise
 */
lv_obj_t *page_primary_get_ui_object(void)
{
    if (g_current_primary_view && g_primary_page_is_active) {
        return g_current_primary_view->inter;
    }
    return NULL;
}

LISAUI_PAGE_EXPORT(luancher_main_page);
