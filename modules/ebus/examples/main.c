/**
 * @file main.c
 * @brief ebus框架示例程序
 *
 * 本示例展示了如何使用ebus框架创建总线、通道，以及进行消息订阅和发布。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ebus/ebus.h>

// 定义消息结构体
typedef struct {
    int id;
    char data[64];
} message_t;

// 全局变量，用于信号处理
volatile int g_running = 1;

// 信号处理函数
void signal_handler(int sig) {
    printf("接收到信号 %d，准备退出...\n", sig);
    g_running = 0;
}

// 发布者通道回调函数
int publisher_callback(ebus_chn_t *chn, void *message, uint32_t msg_size, void *user_data) {
    message_t *msg = (message_t *)message;
    printf("发布者收到消息回复: ID=%d, 数据=%s\n", msg->id, msg->data);
    return 0;
}

// 订阅者通道回调函数
int subscriber_callback(ebus_chn_t *chn, void *message, uint32_t msg_size, void *user_data) {
    message_t *msg = (message_t *)message;
    printf("订阅者收到消息: ID=%d, 数据=%s\n", msg->id, msg->data);
    
    // 修改消息并返回
    snprintf(msg->data, sizeof(msg->data), "已处理消息 %d", msg->id);
    return 0;
}

int main(int argc, char *argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("ebus示例程序启动\n");
    
    // 初始化ebus框架
    int ret = ebus_init();
    if (ret != EBUS_OK) {
        printf("ebus初始化失败: %d\n", ret);
        return -1;
    }
    printf("[###][%s %d]\n",__FUNCTION__,__LINE__);
    printf("ebus初始化成功\n");
    printf("[###][%s %d]\n",__FUNCTION__,__LINE__);
    // 创建总线
    ebus_handle_t *bus = ebus_create("test_bus");
    if (bus == NULL) {
        printf("创建总线失败\n");
        return -1;
    }
    printf("创建总线 'test_bus' 成功\n");
    
    // 创建并附加通道
    ebus_chn_t *pub_channel = ebus_chn_create_attach(bus, "test_channel");
    if (pub_channel == NULL) {
        printf("创建通道失败\n");
        ebus_destroy(bus);
        return -1;
    }
    printf("创建通道 'test_channel' 成功\n");
    
    // 订阅通道消息（发布者自己也可以订阅）
    ret = ebus_message_subscribe(pub_channel, EBUS_SUBSCRIBER_TYPE_SYNC, publisher_callback);
    if (ret != EBUS_OK) {
        printf("发布者订阅通道失败: %d\n", ret);
        ebus_destroy(bus);
        return -1;
    }
    printf("发布者订阅通道成功\n");
    
    // 创建第二个进程模拟订阅者
    pid_t pid = fork();
    if (pid < 0) {
        printf("创建子进程失败\n");
        ebus_destroy(bus);
        return -1;
    } else if (pid == 0) {
        // 子进程 - 订阅者
        printf("订阅者进程启动\n");
        
        // 绑定到已存在的通道
        ebus_chn_t *sub_channel = ebus_chn_bind("test_bus", "test_channel");
        if (sub_channel == NULL) {
            printf("订阅者绑定通道失败\n");
            exit(-1);
        }
        printf("订阅者绑定通道成功\n");
        
        // 订阅通道消息
        ret = ebus_message_subscribe(sub_channel, EBUS_SUBSCRIBER_TYPE_SYNC, subscriber_callback);
        if (ret != EBUS_OK) {
            printf("订阅者订阅通道失败: %d\n", ret);
            exit(-1);
        }
        printf("订阅者订阅通道成功\n");
        
        // 子进程保持运行
        while (g_running) {
            sleep(1);
        }
        
        printf("订阅者进程退出\n");
        exit(0);
    } else {
        // 父进程 - 发布者
        printf("发布者进程继续运行\n");
        
        // 等待子进程启动并订阅
        sleep(2);
        
        // 发布消息
        int msg_count = 0;
        while (g_running && msg_count < 10) {
            message_t msg;
            msg.id = msg_count + 1;
            snprintf(msg.data, sizeof(msg.data), "测试消息 %d", msg.id);
            
            printf("发布消息: ID=%d, 数据=%s\n", msg.id, msg.data);
            ret = ebus_message_pub(pub_channel, &msg, sizeof(message_t), NULL);
            if (ret != EBUS_OK) {
                printf("发布消息失败: %d\n", ret);
            } else {
                printf("消息发布后的数据: ID=%d, 数据=%s\n", msg.id, msg.data);
            }
            
            msg_count++;
            sleep(1);
        }
        
        // 等待子进程退出
        kill(pid, SIGTERM);
        int status;
        waitpid(pid, &status, 0);
        
        // 销毁总线
        ebus_destroy(bus);
        printf("总线已销毁，程序退出\n");
    }
    
    return 0;
}