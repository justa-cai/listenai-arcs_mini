# LISA 事件发布器组件

基于观察者模式的事件发布-订阅组件，为 ARCS 平台提供线程安全的事件驱动通信机制，支持组件间解耦和异步事件处理。

## 功能特性

- **发布订阅模式**: 实现观察者模式的事件发布-订阅机制，支持多对多事件通信
- **线程安全**: 内部使用互斥锁保护，确保多线程环境下的安全操作
- **两种匹配模式**: 支持按位操作和精确数值匹配两种事件管理方式
- **事件数据传输**: 支持携带用户数据的事件发布，灵活的事件数据传递
- **动态订阅**: 运行时动态添加和移除事件订阅者
- **内存管理**: 自动内存管理，支持动态分配和释放事件元素
- **性能优化**: 使用链表结构存储订阅信息，高效的遍历和查找
- **资源管理**: 完整的资源生命周期管理，防止内存泄漏

## 配置选项

在 `prj.conf` 中启用组件：

```kconfig
CONFIG_LISA_EVT_PUBLISHER=y     # 启用事件发布器组件
```

## API 接口

### 生命周期管理

```c
lisa_evt_publisher_t lisa_evt_publisher_new(void);
lisa_evt_publisher_t lisa_evt_publisher_new_eq(void);
void lisa_evt_publisher_destroy(lisa_evt_publisher_t p);
```

### 事件订阅管理

```c
int lisa_evt_publisher_evt_add(lisa_evt_publisher_t p, uint32_t evt,
                              lisa_evt_publisher_cb_t cb, void *user_data);
void lisa_evt_publisher_cb_remove(lisa_evt_publisher_t p, lisa_evt_publisher_cb_t cb);
int lisa_evt_publisher_clear(lisa_evt_publisher_t p);
```

### 事件发布接口

```c
void lisa_evt_publisher_publish(lisa_evt_publisher_t p, uint32_t evt,
                               void *data, uint32_t len);
```

### 便利函数和查询接口

```c
static inline void lisa_evt_publisher_publish_without_data(lisa_evt_publisher_t p, uint32_t evt);
int lisa_evt_publisher_has_subscriber(lisa_evt_publisher_t p, uint32_t evt);
```

## 使用示例

### 基础事件发布订阅示例

```c
#include "lisa_evt_pub.h"
#include "lisa_log.h"

// 定义事件ID
#define EVENT_USER_INPUT    (1 << 0)  // 用户输入事件
#define EVENT_SYSTEM_UPDATE (1 << 1)  // 系统更新事件
#define EVENT_ERROR_NOTIFY  (1 << 2)  // 错误通知事件

// 事件回调函数
static void on_user_input(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    char *input = (char *)data;
    LISA_INFO("收到用户输入事件: %s", input ? input : "无数据");
}

static void on_system_update(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    LISA_INFO("收到系统更新事件，数据长度: %d", data_len);

    // 处理系统更新数据
    if (data && data_len > 0) {
        // 具体的更新处理逻辑
        uint8_t *update_data = (uint8_t *)data;
        // ... 处理更新数据
    }
}

static void on_error_notify(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    int error_code = *(int *)data;
    LISA_INFO("收到错误通知事件，错误代码: %d", error_code);
}

int basic_event_example(void)
{
    // 1. 创建事件发布器（按位操作模式）
    lisa_evt_publisher_t publisher = lisa_evt_publisher_new();
    if (!publisher) {
        LISA_ERR("创建事件发布器失败");
        return -1;
    }

    // 2. 添加事件订阅者
    if (lisa_evt_publisher_evt_add(publisher, EVENT_USER_INPUT, on_user_input, NULL) != 0) {
        LISA_ERR("订阅用户输入事件失败");
        goto cleanup;
    }

    if (lisa_evt_publisher_evt_add(publisher, EVENT_SYSTEM_UPDATE, on_system_update, NULL) != 0) {
        LISA_ERR("订阅系统更新事件失败");
        goto cleanup;
    }

    if (lisa_evt_publisher_evt_add(publisher, EVENT_ERROR_NOTIFY, on_error_notify, NULL) != 0) {
        LISA_ERR("订阅错误通知事件失败");
        goto cleanup;
    }

    // 3. 发布事件
    // 发布用户输入事件
    char user_input[] = "Hello, World!";
    lisa_evt_publisher_publish(publisher, EVENT_USER_INPUT, user_input, sizeof(user_input));

    // 发布系统更新事件
    uint8_t update_data[] = {0x01, 0x02, 0x03, 0x04};
    lisa_evt_publisher_publish(publisher, EVENT_SYSTEM_UPDATE, update_data, sizeof(update_data));

    // 发布错误通知事件
    int error_code = 500;
    lisa_evt_publisher_publish(publisher, EVENT_ERROR_NOTIFY, &error_code, sizeof(error_code));

    // 发布无数据事件
    lisa_evt_publisher_publish_without_data(publisher, EVENT_USER_INPUT);

    // 4. 等待事件处理
    lisa_thread_delay(100);  // 等待 100ms 确保事件处理完成

    // 5. 检查订阅状态
    if (lisa_evt_publisher_has_subscriber(publisher, EVENT_USER_INPUT)) {
        LISA_INFO("用户输入事件仍有订阅者");
    }

cleanup:
    // 6. 清理资源
    lisa_evt_publisher_destroy(publisher);
    return 0;
}
```

### 精确匹配模式示例

```c
#include "lisa_evt_pub.h"
#include "lisa_log.h"

// 定义精确的事件ID
#define EVENT_TEMPERATURE_SENSOR    1001  // 温度传感器事件
#define EVENT_HUMIDITY_SENSOR      1002  // 湿度传感器事件
#define EVENT_PRESSURE_SENSOR      1003  // 压力传感器事件

// 传感器数据结构
typedef struct {
    float value;
    uint32_t timestamp;
    uint16_t sensor_id;
} sensor_data_t;

// 传感器数据处理函数
static void handle_temperature_event(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    if (data && data_len == sizeof(sensor_data_t)) {
        sensor_data_t *sensor_data = (sensor_data_t *)data;
        LISA_INFO("温度传感器数据: %.2f°C, 传感器ID: %d",
                 sensor_data->value, sensor_data->sensor_id);
    }
}

static void handle_humidity_event(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    if (data && data_len == sizeof(sensor_data_t)) {
        sensor_data_t *sensor_data = (sensor_data_t *)data;
        LISA_INFO("湿度传感器数据: %.1f%%, 传感器ID: %d",
                 sensor_data->value, sensor_data->sensor_id);
    }
}

int exact_match_example(void)
{
    // 1. 创建精确匹配模式的事件发布器
    lisa_evt_publisher_t sensor_publisher = lisa_evt_publisher_new_eq();
    if (!sensor_publisher) {
        LISA_ERR("创建精确匹配事件发布器失败");
        return -1;
    }

    // 2. 订阅精确的传感器事件
    lisa_evt_publisher_evt_add(sensor_publisher, EVENT_TEMPERATURE_SENSOR,
                               handle_temperature_event, NULL);
    lisa_evt_publisher_evt_add(sensor_publisher, EVENT_HUMIDITY_SENSOR,
                               handle_humidity_event, NULL);

    // 3. 模拟传感器数据发布
    sensor_data_t temp_data = {
        .value = 25.6f,
        .timestamp = 1234567890,
        .sensor_id = 100
    };
    lisa_evt_publisher_publish(sensor_publisher, EVENT_TEMPERATURE_SENSOR,
                              &temp_data, sizeof(temp_data));

    sensor_data_t humidity_data = {
        .value = 60.2f,
        .timestamp = 1234567891,
        .sensor_id = 101
    };
    lisa_evt_publisher_publish(sensor_publisher, EVENT_HUMIDITY_SENSOR,
                              &humidity_data, sizeof(humidity_data));

    // 4. 发布未订阅的事件（不会被处理）
    lisa_evt_publisher_publish_without_data(sensor_publisher, EVENT_PRESSURE_SENSOR);

    // 5. 等待处理完成
    lisa_thread_delay(100);

    // 6. 清理资源
    lisa_evt_publisher_destroy(sensor_publisher);
    return 0;
}
```

### 多发布器管理示例

```c
#include "lisa_evt_pub.h"
#include "lisa_log.h"

// 应用事件定义
#define EVENT_APP_START       0x01
#define EVENT_APP_STOP        0x02
#define EVENT_APP_SUSPEND     0x04

// 系统事件定义
#define EVENT_SYS_BOOT        0x01
#define EVENT_SYS_SHUTDOWN    0x02
#define EVENT_SYS_LOW_BATTERY 0x04

// 应用管理器结构
typedef struct {
    lisa_evt_publisher_t app_publisher;
    lisa_evt_publisher_t sys_publisher;
    int app_count;
    bool system_ready;
} app_manager_t;

static app_manager_t g_app_manager = {0};

// 应用事件处理函数
static void handle_app_start(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    g_app_manager.app_count++;
    LISA_INFO("应用启动，当前应用数量: %d", g_app_manager.app_count);
}

static void handle_app_stop(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    if (g_app_manager.app_count > 0) {
        g_app_manager.app_count--;
    }
    LISA_INFO("应用停止，当前应用数量: %d", g_app_manager.app_count);
}

// 系统事件处理函数
static void handle_sys_boot(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    g_app_manager.system_ready = true;
    LISA_INFO("系统启动完成，应用管理器就绪");
}

static void handle_sys_shutdown(uint32_t evt, void *data, uint32_t data_len, void *user_data)
{
    g_app_manager.system_ready = false;
    LISA_INFO("系统关闭，清理所有应用");

    // 清理所有应用事件
    lisa_evt_publisher_clear(g_app_manager.app_publisher);
}

int multi_publisher_example(void)
{
    // 1. 创建两个发布器
    g_app_manager.app_publisher = lisa_evt_publisher_new();
    g_app_manager.sys_publisher = lisa_evt_publisher_new();

    if (!g_app_manager.app_publisher || !g_app_manager.sys_publisher) {
        LISA_ERR("创建发布器失败");
        goto cleanup;
    }

    // 2. 订阅应用事件
    lisa_evt_publisher_evt_add(g_app_manager.app_publisher, EVENT_APP_START,
                               handle_app_start, NULL);
    lisa_evt_publisher_evt_add(g_app_manager.app_publisher, EVENT_APP_STOP,
                               handle_app_stop, NULL);

    // 3. 订阅系统事件
    lisa_evt_publisher_evt_add(g_app_manager.sys_publisher, EVENT_SYS_BOOT,
                               handle_sys_boot, NULL);
    lisa_evt_publisher_evt_add(g_app_manager.sys_publisher, EVENT_SYS_SHUTDOWN,
                               handle_sys_shutdown, NULL);

    // 4. 模拟系统启动
    lisa_evt_publisher_publish_without_data(g_app_manager.sys_publisher, EVENT_SYS_BOOT);

    // 5. 模拟应用启动和停止
    lisa_evt_publisher_publish_without_data(g_app_manager.app_publisher, EVENT_APP_START);
    lisa_evt_publisher_publish_without_data(g_app_manager.app_publisher, EVENT_APP_START);
    lisa_evt_publisher_publish_without_data(g_app_manager.app_publisher, EVENT_APP_START);

    lisa_evt_publisher_publish_without_data(g_app_manager.app_publisher, EVENT_APP_STOP);

    // 6. 模拟系统关闭
    lisa_thread_delay(100);
    lisa_evt_publisher_publish_without_data(g_app_manager.sys_publisher, EVENT_SYS_SHUTDOWN);

cleanup:
    // 7. 清理资源
    if (g_app_manager.app_publisher) {
        lisa_evt_publisher_destroy(g_app_manager.app_publisher);
    }
    if (g_app_manager.sys_publisher) {
        lisa_evt_publisher_destroy(g_app_manager.sys_publisher);
    }

    return 0;
}
```

## 事件匹配模式说明

### 按位操作模式（lisa_evt_publisher_new）

- **特点**: 事件ID按位进行管理，支持位掩码匹配
- **适用场景**: 事件类型相对固定，需要组合事件的场景
- **优势**: 节省内存，支持事件组合，提高效率
- **限制**: 事件ID不能超过32位，最大支持32种基本事件类型

### 精确数值匹配模式（lisa_evt_publisher_new_eq）

- **特点**: 按照事件数值进行精确匹配
- **适用场景**: 事件类型不固定或数量很多的情况
- **优势**: 支持任意数量的事件ID，灵活性高
- **限制**: 内存占用相对较高，无法进行事件组合

## 使用注意事项

1. **发布器选择**: 根据应用场景选择合适的发布器类型，位操作适合事件组合，精确匹配适合大量独立事件
2. **内存管理**: 事件数据在回调函数执行完成后即可释放，组件会自动复制需要的数据
3. **回调函数**: 回调函数应该尽量简短，避免长时间阻塞事件处理线程
4. **线程安全**: 组件内部保证线程安全，但回调函数中的用户代码需要自行处理线程安全问题
5. **事件ID设计**: 在按位操作模式下，事件ID应该是2的幂次方，避免位冲突
6. **订阅管理**: 及时移除不再需要的订阅，避免内存泄漏
7. **资源清理**: 应用退出前必须调用 `lisa_evt_publisher_destroy()` 清理发布器资源
8. **错误处理**: 所有API调用都应检查返回值，确保操作成功
9. **数据一致性**: 确保发布的事件数据长度与实际数据大小一致
10. **回调参数**: 用户数据指针在订阅时传入，会在每次回调时传递给回调函数

## 文件说明

- `lisa_evt_pub.h` - 事件发布器组件头文件，包含 API 接口和数据结构定义
- `lisa_evt_pub.c` - 事件发布器组件实现文件，基于链表和互斥锁实现
- `Kconfig` - 组件配置选项定义文件
- `CMakeLists.txt` - 组件构建配置文件