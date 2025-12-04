#ifndef __ACOMP_IPC_H_
#define __ACOMP_IPC_H_

#include <stdint.h>
#define IPC_ALIGN_SIZE    (32)
#define ACOMP_DEV_NAME_MAX_LEN (16)

/*components ipc global define*/
typedef union{
    uint8_t glb_cmd;
    struct{
        uint8_t cmd:4; 			/*global cmd*/
        uint8_t req_reply:1; 	/*request global reply*/
        uint8_t resp:1;  	    /*resp 0:成功，1：失败*/
        uint8_t reserved:2; 
    }hdr;
}__attribute__((packed)) acomp_ipc_header_t;

/*components ipc global define*/
#ifndef BIT
#define BIT(x)                 (1<<x)
#endif
#define IPC_HEADER_REQ_REPALY        BIT(4)
#define IPC_HEADER_GLB_CMD(cmd)      (cmd & 0x0f)

/*components global commands  APP CORE  <-> ALGO CORE */
#define ACOMP_CONTEXT_IPC_GLB_REPLY   (0x00)


/*components global commands  APP CORE  ->  ALGO CORE*/
#define ACOMP_CONTEXT_IPC_GLB_NEW     (0x01)
#define ACOMP_CONTEXT_IPC_GLB_FREE    (0x02)
#define ACOMP_CONTEXT_IPC_GLB_CONTROL (0x03)
#define ACOMP_CONTEXT_IPC_GLB_DEVINFO_QUERY (0x04)


/*components global commands  ALGO CORE ->  APP CORE*/
#define ACOMP_CONTEXT_IPC_GLB_NOTIFY   (0x01)
#define ACOMP_CONTEXT_IPC_GLB_DEVINFO_QUERY_RESP (0x02)

/*components subcmd define  APP CORE ->  ALGO CORE*/
#define ACOMP_IPC_CMD_PREPARE             (0x00)  /*sync prepare load resource ,init memory ...*/
#define ACOMP_IPC_CMD_CLEANUP             (0x01)  /*sync cleanup free resource ,free memory ...*/
#define ACOMP_IPC_CMD_START               (0x02)
#define ACOMP_IPC_CMD_STOP                (0x03)
#define ACOMP_IPC_CMD_ABORT               (0x04)
#define ACOMP_IPC_CMD_CONTROL             (0x05)
#define ACOMP_IPC_CMD_STREAM_CREATE       (0x06)
#define ACOMP_IPC_CMD_STREAM_DESTROY      (0x07)
#define ACOMP_IPC_CMD_STREAM_UPDATE       (0x08) /*sync stream update*/


/*components subcmd  ALGO CORE ->  APP CORE*/
#define ACOMP_IPC_CMD_NOTIFY_RESULT         (0x01) /*async notify result*/
#define ACOMP_IPC_CMD_NOTIFY_STREAM_UPDATE  (0x02) /*async notify data write*/
#define ACOMP_IPC_CMD_NOTIFY_SUBCMD         (0x03) /*async notify subcmd*/


typedef struct {

    acomp_ipc_header_t hdr;
	uint8_t dev_index;
	uint8_t acomp_cmd;
    union {
        struct{
            uint8_t flags;
        }req;
        struct{
            uint8_t err;
        }reply;
    };
	
	uint16_t len;
	uint32_t address;
} __attribute__((packed)) acomp_ipc_message_t;


typedef union{
    uint32_t data;
    struct{
        uint32_t storage:3; /*0:FLASH;1:SD;2:PSRAM;*/
        uint32_t reserved:29;
    }hdr;
}acomp_res_item_attr_t;

typedef struct {

    uint32_t index;
    acomp_res_item_attr_t attr;
	uint32_t addr;
	uint32_t offset;
	uint32_t size;

}__attribute__((packed))acomp_res_item_t;

typedef struct {
	
	uint32_t number;
	acomp_res_item_t item[0];

}__attribute__((packed,aligned(32)))acomp_ipc_prepare_t;

typedef struct{
    uint32_t index;
    uint8_t name[ACOMP_DEV_NAME_MAX_LEN];
}__attribute__((packed))acomp_ipc_dev_info_t;

typedef struct{
    uint32_t number;
    acomp_ipc_dev_info_t item[0];
}__attribute__((packed,aligned(32)))acomp_ipc_dev_info_query_msg_t;


typedef struct{
	int control;
	uint32_t len;
	uint8_t data[0];
}__attribute__((packed,aligned(32)))acomp_ipc_control_t;



typedef struct{
	uint32_t len;
	uint8_t data[];
}__attribute__((packed,aligned(32)))acomp_ipc_notify_result_t;

typedef struct{
	uint32_t len;
	uint8_t data[];
}__attribute__((packed,aligned(32)))acomp_ipc_notify_subcmd_t;

typedef struct{
	char name[16];
    uint8_t direction; /*0: m2r,1: r2m*/
    uint8_t index;
    
    uint32_t kick_policy;  /* Kick policy: 0:manual/ >0: buffer count to kick */
    /* vring memory*/
    void *phy_addr;
    uint32_t mem_size;
    uint32_t align;
    uint32_t buffer_size;

}__attribute__((packed,aligned(32)))acomp_ipc_stream_create_desc_t;
typedef struct{
	uint32_t index;

}__attribute__((packed,aligned(32)))acomp_ipc_stream_destroy_desc_t;

typedef struct{
	uint32_t index;

}__attribute__((packed,aligned(32)))acomp_ipc_stream_update_t;

#define ACOMP_IPC_NOTIFY_DATA_ALIGN(len) ACOMP_IPC_ALIGN(offsetof(acomp_ipc_notify_t,data)+ len)


typedef void(*ipc_event_cb_t) (acomp_ipc_message_t *message, void *priv);

extern int acomp_ipc_init(void);
extern int acomp_ipc_add_callback(uint32_t dev_index, ipc_event_cb_t cb, void *priv);
extern int acomp_ipc_build_frame_send_sync(int dev_index,int cmd,int acomp_cmd,uint8_t flags,void* data,uint16_t len);
extern int acomp_ipc_get_dev_index(const char *name);

#endif
