#include "lisa_ui_anim_ext.h"
#include "lisa_ui_assets.h"

#include "lisa_ui_log.h"

static const void *emoji_battery_frames[] = {
    &emoji_battery_frame_000000_png,
    &emoji_battery_frame_000001_png,
    &emoji_battery_frame_000002_png,
    &emoji_battery_frame_000003_png,
    &emoji_battery_frame_000004_png,
    &emoji_battery_frame_000005_png,
    &emoji_battery_frame_000006_png,
    &emoji_battery_frame_000007_png,
    &emoji_battery_frame_000008_png,
    &emoji_battery_frame_000009_png,
    &emoji_battery_frame_000010_png,
    &emoji_battery_frame_000011_png,
    &emoji_battery_frame_000012_png,
    &emoji_battery_frame_000013_png,
    &emoji_battery_frame_000014_png,
    &emoji_battery_frame_000015_png,
    &emoji_battery_frame_000016_png,
    &emoji_battery_frame_000017_png,
    &emoji_battery_frame_000018_png,
    &emoji_battery_frame_000019_png,
    &emoji_battery_frame_000020_png,
    &emoji_battery_frame_000021_png,
};

static const void *emoji_love_enter[] = {
    &emoji_love_love_000_png,
    &emoji_love_love_001_png,
    &emoji_love_love_002_png,
    &emoji_love_love_003_png,
    &emoji_love_love_004_png,
    &emoji_love_love_005_png,
};

static const void *emoji_love_loop[] = {
    &emoji_love_love_005_png,
    &emoji_love_love_006_png,
    &emoji_love_love_007_png,
    &emoji_love_love_008_png,
    &emoji_love_love_009_png,
    &emoji_love_love_010_png,
    &emoji_love_love_011_png,
    &emoji_love_love_012_png,
    &emoji_love_love_013_png,
    &emoji_love_love_014_png,
    &emoji_love_love_015_png,
    &emoji_love_love_016_png,
};

static const void *emoji_love_exit[] = {
    &emoji_love_love_016_png,
    &emoji_love_love_017_png,
    &emoji_love_love_018_png,
    &emoji_love_love_019_png,
    &emoji_love_love_020_png,
    &emoji_love_love_021_png,
    &emoji_love_love_022_png,
};

static const void *emoji_sad_enter[] = {
    &emoji_sad_sad_000_png,
    &emoji_sad_sad_001_png,
    &emoji_sad_sad_002_png,
    &emoji_sad_sad_003_png,
    &emoji_sad_sad_004_png,
    &emoji_sad_sad_005_png,
};

static const void *emoji_sad_loop[] = {
    &emoji_sad_sad_005_png,
    &emoji_sad_sad_006_png,
    &emoji_sad_sad_007_png,
    &emoji_sad_sad_008_png,
    &emoji_sad_sad_009_png,
    &emoji_sad_sad_010_png,
    &emoji_sad_sad_011_png,
    &emoji_sad_sad_012_png,
};

static const void *emoji_sad_exit[] = {
    &emoji_sad_sad_013_png,
    &emoji_sad_sad_014_png,
    &emoji_sad_sad_015_png,
    &emoji_sad_sad_016_png,
    &emoji_sad_sad_017_png,
    &emoji_sad_sad_018_png,
};

static const void *emoji_puzzled_enter[] = {
    &emoji_puzzled_puzzled_000_png,
    &emoji_puzzled_puzzled_001_png,
    &emoji_puzzled_puzzled_002_png,
    &emoji_puzzled_puzzled_003_png,
    &emoji_puzzled_puzzled_004_png,
    &emoji_puzzled_puzzled_005_png,
};
static const void *emoji_puzzled_loop[] = {
    &emoji_puzzled_puzzled_006_png,
    &emoji_puzzled_puzzled_007_png,
    &emoji_puzzled_puzzled_008_png,
    &emoji_puzzled_puzzled_009_png,
    &emoji_puzzled_puzzled_010_png,
    &emoji_puzzled_puzzled_011_png,
    &emoji_puzzled_puzzled_012_png,
};

static const void *emoji_puzzled_exit[] = {
    &emoji_puzzled_puzzled_013_png,
    &emoji_puzzled_puzzled_014_png,
    &emoji_puzzled_puzzled_015_png,
    &emoji_puzzled_puzzled_016_png,
    &emoji_puzzled_puzzled_017_png,
    &emoji_puzzled_puzzled_018_png,
};

static const void *emoji_angry_enter[] = {
    &emoji_angry_angry_000_png,
    &emoji_angry_angry_001_png,
    &emoji_angry_angry_002_png,
    &emoji_angry_angry_003_png,
    &emoji_angry_angry_004_png,
};

static const void *emoji_angry_loop[] = {
    &emoji_angry_angry_005_png,
    &emoji_angry_angry_006_png,
    &emoji_angry_angry_007_png,
    &emoji_angry_angry_008_png,
    &emoji_angry_angry_009_png,
};

static const void *emoji_angry_exit[] = {
    &emoji_angry_angry_010_png,
    &emoji_angry_angry_011_png,
    &emoji_angry_angry_012_png,
    &emoji_angry_angry_013_png,
    &emoji_angry_angry_014_png,
    &emoji_angry_angry_015_png,
};

static const void *emoji_standby[] = {
    &emoji_blink_blink_000_png,
    &emoji_blink_blink_001_png,
    &emoji_blink_blink_002_png,
    &emoji_blink_blink_003_png,
    &emoji_blink_blink_004_png,
};

static const void *emoji_eye[] = {
    &emoji_eye_eye_000_png,
    &emoji_eye_eye_001_png,
    &emoji_eye_eye_002_png,
    &emoji_eye_eye_003_png,
    &emoji_eye_eye_004_png,
    &emoji_eye_eye_005_png,
    &emoji_eye_eye_006_png,
    &emoji_eye_eye_007_png,
    &emoji_eye_eye_008_png,
    &emoji_eye_eye_009_png,
    &emoji_eye_eye_010_png,
    &emoji_eye_eye_011_png,
};

static const void *emoji_hug_enter[] = {
    &emoji_hug_hug_000_png,
    &emoji_hug_hug_001_png,
    &emoji_hug_hug_002_png,
    &emoji_hug_hug_003_png,
    &emoji_hug_hug_004_png,
    &emoji_hug_hug_005_png,
};

static const void *emoji_hug_loop[] = {
    &emoji_hug_hug_006_png,
    &emoji_hug_hug_007_png,
    &emoji_hug_hug_008_png,
    &emoji_hug_hug_009_png,
    &emoji_hug_hug_010_png,
    &emoji_hug_hug_011_png,
    &emoji_hug_hug_012_png,
    &emoji_hug_hug_013_png,
    &emoji_hug_hug_014_png,
    &emoji_hug_hug_015_png,
    &emoji_hug_hug_016_png,
    &emoji_hug_hug_017_png,
    &emoji_hug_hug_018_png,
};

static const void *emoji_hug_exit[] = {
    &emoji_hug_hug_019_png,
    &emoji_hug_hug_020_png,
    &emoji_hug_hug_021_png,
    &emoji_hug_hug_022_png,
    &emoji_hug_hug_023_png,
    &emoji_hug_hug_024_png,
};

static const void *emoji_wakeup_enter[] = {
    &emoji_wakeup_wakeup_000_png,
    &emoji_wakeup_wakeup_001_png,
    &emoji_wakeup_wakeup_002_png,
    &emoji_wakeup_wakeup_003_png,
    &emoji_wakeup_wakeup_004_png,
    &emoji_wakeup_wakeup_005_png,
    &emoji_wakeup_wakeup_006_png,
};

static const void *emoji_wakeup_loop[] = {
    &emoji_wakeup_wakeup_006_png,
    &emoji_wakeup_wakeup_007_png,
    &emoji_wakeup_wakeup_008_png,
    &emoji_wakeup_wakeup_009_png,
    &emoji_wakeup_wakeup_010_png,
    &emoji_wakeup_wakeup_011_png,
    &emoji_wakeup_wakeup_012_png,
    &emoji_wakeup_wakeup_013_png,
    &emoji_wakeup_wakeup_014_png,
    &emoji_wakeup_wakeup_015_png,
    &emoji_wakeup_wakeup_016_png,
    &emoji_wakeup_wakeup_017_png,
    &emoji_wakeup_wakeup_018_png,
    &emoji_wakeup_wakeup_019_png,
    &emoji_wakeup_wakeup_020_png,
    &emoji_wakeup_wakeup_021_png,
};

static const void *emoji_wakeup_exit[] = {
    &emoji_wakeup_wakeup_021_png,
    &emoji_wakeup_wakeup_022_png,
    &emoji_wakeup_wakeup_023_png,
    &emoji_wakeup_wakeup_024_png,
    &emoji_wakeup_wakeup_025_png,
    &emoji_wakeup_wakeup_026_png,
    &emoji_wakeup_wakeup_027_png,
};

static const void *emoji_sleepy_enter[] = {
    &emoji_sleepy_sleepy_000_png,
    &emoji_sleepy_sleepy_001_png,
    &emoji_sleepy_sleepy_002_png,
    &emoji_sleepy_sleepy_003_png,
    &emoji_sleepy_sleepy_004_png,
    &emoji_sleepy_sleepy_005_png,
    &emoji_sleepy_sleepy_006_png,
    &emoji_sleepy_sleepy_007_png,
    &emoji_sleepy_sleepy_008_png,
    &emoji_sleepy_sleepy_009_png,
    &emoji_sleepy_sleepy_010_png,
};

static const void *emoji_sleepy_loop[] = {
    &emoji_sleepy_sleepy_010_png,
    &emoji_sleepy_sleepy_011_png,
    &emoji_sleepy_sleepy_012_png,
    &emoji_sleepy_sleepy_013_png,
    &emoji_sleepy_sleepy_014_png,
    &emoji_sleepy_sleepy_015_png,
    &emoji_sleepy_sleepy_016_png,
    &emoji_sleepy_sleepy_017_png,
    &emoji_sleepy_sleepy_018_png,
    &emoji_sleepy_sleepy_019_png,
    &emoji_sleepy_sleepy_020_png,
    &emoji_sleepy_sleepy_021_png,
    &emoji_sleepy_sleepy_022_png,
    &emoji_sleepy_sleepy_023_png,
    &emoji_sleepy_sleepy_024_png,
    &emoji_sleepy_sleepy_025_png,
    &emoji_sleepy_sleepy_026_png,
    &emoji_sleepy_sleepy_027_png,
    &emoji_sleepy_sleepy_028_png,
    &emoji_sleepy_sleepy_029_png,
    &emoji_sleepy_sleepy_030_png,
    &emoji_sleepy_sleepy_031_png,
    &emoji_sleepy_sleepy_032_png,
    &emoji_sleepy_sleepy_033_png,
    &emoji_sleepy_sleepy_034_png,
};

static const void *emoji_sleepy_exit[] = {
    &emoji_sleepy_sleepy_034_png,
    &emoji_sleepy_sleepy_035_png,
    &emoji_sleepy_sleepy_036_png,
    &emoji_sleepy_sleepy_037_png,
    &emoji_sleepy_sleepy_038_png,
    &emoji_sleepy_sleepy_039_png,
    &emoji_sleepy_sleepy_040_png,
};

static const void *emoji_wait_loop[] = {
    &emoji_wait_wait_000_png,
    &emoji_wait_wait_001_png,
    &emoji_wait_wait_002_png,
    &emoji_wait_wait_003_png,
    &emoji_wait_wait_004_png,
    &emoji_wait_wait_005_png,
    &emoji_wait_wait_006_png,
    &emoji_wait_wait_007_png,
};

static const void *emoji_happy_enter[] = {
    &emoji_happy_frame_000000_png,
    &emoji_happy_frame_000001_png,
    &emoji_happy_frame_000002_png,
    &emoji_happy_frame_000003_png,
    &emoji_happy_frame_000004_png,
    &emoji_happy_frame_000005_png,
};

static const void *emoji_happy_loop[] = {
    &emoji_happy_frame_000005_png,
    &emoji_happy_frame_000006_png,
    &emoji_happy_frame_000007_png,
    &emoji_happy_frame_000008_png,
    &emoji_happy_frame_000009_png,
    &emoji_happy_frame_000010_png,
    &emoji_happy_frame_000011_png,
    &emoji_happy_frame_000012_png,
    &emoji_happy_frame_000013_png,
    &emoji_happy_frame_000014_png,
    &emoji_happy_frame_000015_png,
};

static const void *emoji_happy_exit[] = {
    &emoji_happy_frame_000015_png,
    &emoji_happy_frame_000016_png,
    &emoji_happy_frame_000017_png,
    &emoji_happy_frame_000018_png,
    &emoji_happy_frame_000019_png,
    &emoji_happy_frame_000020_png,
};

static const void *emoji_cute_enter[] = {
    &emoji_cute_frame_000000_png,
    &emoji_cute_frame_000001_png,
    &emoji_cute_frame_000002_png,
    &emoji_cute_frame_000003_png,
    &emoji_cute_frame_000004_png,
    &emoji_cute_frame_000005_png,
};

static const void *emoji_cute_loop[] = {
    &emoji_cute_frame_000005_png,
    &emoji_cute_frame_000006_png,
    &emoji_cute_frame_000007_png,
    &emoji_cute_frame_000008_png,
    &emoji_cute_frame_000009_png,
    &emoji_cute_frame_000010_png,
    &emoji_cute_frame_000011_png,
    &emoji_cute_frame_000012_png,
    &emoji_cute_frame_000013_png,
    &emoji_cute_frame_000014_png,
    &emoji_cute_frame_000015_png,
    &emoji_cute_frame_000016_png,
    &emoji_cute_frame_000017_png,
    &emoji_cute_frame_000018_png,
    &emoji_cute_frame_000019_png,
};

static const void *emoji_cute_exit[] = {
    &emoji_cute_frame_000019_png,
    &emoji_cute_frame_000020_png,
    &emoji_cute_frame_000021_png,
    &emoji_cute_frame_000022_png,
    &emoji_cute_frame_000023_png,
};

static const lisa_ui_anim_ext_config_t anim_ext_config_sleep = {
    .enter =
        {
            .frames = emoji_sleepy_enter,
            .frame_count = sizeof(emoji_sleepy_enter) / sizeof(emoji_sleepy_enter[0]),
            .default_delay = 100,
            .loop = 0,
        },
    .loop =
        {
            .frames = emoji_sleepy_loop,
            .frame_count = sizeof(emoji_sleepy_loop) / sizeof(emoji_sleepy_loop[0]),
            .default_delay = 100,
            .loop = -1,
        },
    .exit =
        {
            .frames = emoji_sleepy_exit,
            .frame_count = sizeof(emoji_sleepy_exit) / sizeof(emoji_sleepy_exit[0]),
            .default_delay = 100,
            .loop = 0,
        },
};

static const lisa_ui_anim_ext_config_t anim_ext_config_wakeup = {
    .enter =
        {
            .frames = emoji_wakeup_enter,
            .frame_count = sizeof(emoji_wakeup_enter) / sizeof(emoji_wakeup_enter[0]),
            .default_delay = 100,
            .loop = 0,
        },
    .loop =
        {
            .frames = emoji_wakeup_loop,
            .frame_count = sizeof(emoji_wakeup_loop) / sizeof(emoji_wakeup_loop[0]),
            .default_delay = 100,
            .loop = -1,
        },
    .exit =
        {
            .frames = emoji_wakeup_exit,
            .frame_count = sizeof(emoji_wakeup_exit) / sizeof(emoji_wakeup_exit[0]),
            .default_delay = 100,
            .loop = 0,
        },
};

static const lisa_ui_anim_ext_config_t anim_ext_config_sad = {
    .enter =
        {
            .frames = emoji_sad_enter,
            .frame_count = sizeof(emoji_sad_enter) / sizeof(emoji_sad_enter[0]),
            .default_delay = 100,
            .loop = 0,
        },
    .loop =
        {
            .frames = emoji_sad_loop,
            .frame_count = sizeof(emoji_sad_loop) / sizeof(emoji_sad_loop[0]),
            .default_delay = 100,
            .loop = -1,
        },
    .exit =
        {
            .frames = emoji_sad_exit,
            .frame_count = sizeof(emoji_sad_exit) / sizeof(emoji_sad_exit[0]),
            .default_delay = 100,
            .loop = 0,
        },
};

static const lisa_ui_anim_ext_config_t anim_ext_config_happy = {
    .enter =
        {
            .frames = emoji_happy_enter,
            .frame_count = sizeof(emoji_happy_enter) / sizeof(emoji_happy_enter[0]),
            .default_delay = 100,
            .loop = 0,
        },
    .loop =
        {
            .frames = emoji_happy_loop,
            .frame_count = sizeof(emoji_happy_loop) / sizeof(emoji_happy_loop[0]),
            .default_delay = 100,
            .loop = -1,
        },
    .exit =
        {
            .frames = emoji_happy_exit,
            .frame_count = sizeof(emoji_happy_exit) / sizeof(emoji_happy_exit[0]),
            .default_delay = 100,
            .loop = 0,
        },
};

static const lisa_ui_anim_ext_config_t anim_ext_config_angry = {
    .enter =
        {
            .frames = emoji_angry_enter,
            .frame_count = sizeof(emoji_angry_enter) / sizeof(emoji_angry_enter[0]),
            .default_delay = 100,
            .loop = 0,
        },
    .loop =
        {
            .frames = emoji_angry_loop,
            .frame_count = sizeof(emoji_angry_loop) / sizeof(emoji_angry_loop[0]),
            .default_delay = 100,
            .loop = -1,
        },
    .exit =
        {
            .frames = emoji_angry_exit,
            .frame_count = sizeof(emoji_angry_exit) / sizeof(emoji_angry_exit[0]),
            .default_delay = 100,
            .loop = 0,
        },
};

static const lisa_ui_anim_ext_config_t anim_ext_config_cute = {
    .enter =
        {
            .frames = emoji_cute_enter,
            .frame_count = sizeof(emoji_cute_enter) / sizeof(emoji_cute_enter[0]),
            .default_delay = 100,
            .loop = 0,
        },
    .loop =
        {
            .frames = emoji_cute_loop,
            .frame_count = sizeof(emoji_cute_loop) / sizeof(emoji_cute_loop[0]),
            .default_delay = 100,
            .loop = -1,
        },
    .exit =
        {
            .frames = emoji_cute_exit,
            .frame_count = sizeof(emoji_cute_exit) / sizeof(emoji_cute_exit[0]),
            .default_delay = 100,
            .loop = 0,
        },
};

static const lisa_ui_anim_ext_config_t anim_ext_config_love = {
    .enter =
        {
            .frames = emoji_love_enter,
            .frame_count = sizeof(emoji_love_enter) / sizeof(emoji_love_enter[0]),
            .default_delay = 100,
            .loop = 0,
        },
    .loop =
        {
            .frames = emoji_love_loop,
            .frame_count = sizeof(emoji_love_loop) / sizeof(emoji_love_loop[0]),
            .default_delay = 100,
            .loop = -1,
        },
    .exit =
        {
            .frames = emoji_love_exit,
            .frame_count = sizeof(emoji_love_exit) / sizeof(emoji_love_exit[0]),
            .default_delay = 100,
            .loop = 0,
        },
};

static const uint32_t waiting_delays[] = {1000, 100, 100, 100, 100, 100, 100, 100};

static const lisa_ui_anim_ext_config_t anim_ext_config_waiting = {
    .loop =
        {
            .frames = emoji_wait_loop,
            .frame_count = sizeof(emoji_wait_loop) / sizeof(emoji_wait_loop[0]),
            .delays = waiting_delays,
            .default_delay = 100,
            .loop = -1,
        },
};

static const uint32_t standby_delays[] = {1000, 100, 100, 100, 100};

static const lisa_ui_anim_ext_config_t anim_ext_config_standby = {
    .loop =
        {
            .frames = emoji_standby,
            .frame_count = sizeof(emoji_standby) / sizeof(emoji_standby[0]),
            .delays = standby_delays,
            .default_delay = 100,
            .loop = -1,
        },
};

struct emoji_config {
    char *name;
    const lisa_ui_anim_ext_config_t *anim;
    uint16_t idx;
};

static const struct emoji_config emoji_configs[] = {
    {
        .name = "wait",
        .anim = &anim_ext_config_waiting,
        .idx = 0,
    },
    {
        .name = "love",
        .anim = &anim_ext_config_love,
        .idx = 1,
    },
    {
        .name = "sad",
        .anim = &anim_ext_config_sad,
        .idx = 2,
    },
    {
        .name = "laugh",
        .anim = &anim_ext_config_love,
        .idx = 3,
    },
    {
        .name = "angry",
        .anim = &anim_ext_config_angry,
        .idx = 5,
    },
    {
        .name = "eye",
        .anim = &anim_ext_config_standby,
        .idx = 6,
    },
    {
        .name = "blink",
        .anim = &anim_ext_config_standby,
        .idx = 7,
    },
    {
        .name = "happy",
        .anim = &anim_ext_config_happy,
        .idx = 8,
    },
    {
        .name = "cute",
        .anim = &anim_ext_config_cute,
        .idx = 9,
    },
    {
        .name = "wakeup",
        .anim = &anim_ext_config_wakeup,
        .idx = 10,
    },
    {
        .name = "neutral",
        .anim = &anim_ext_config_standby,
        .idx = 11,
    },
};

const lisa_ui_anim_ext_config_t *emoji_anim_get_by_name(const char *name)
{
    for (int i = 0; i < sizeof(emoji_configs) / sizeof(emoji_configs[0]); i++) {
        if (strcmp(emoji_configs[i].name, name) == 0) {
            return emoji_configs[i].anim;
        }
    }

    LISA_UI_LOGE("Invalid emoji name: %s, use standby instead", name);

    return &anim_ext_config_standby;
}
