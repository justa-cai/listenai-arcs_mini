#include <stdio.h>
#include <stdlib.h>
#include "event.h"
#include <string.h>
#define EVENT_MAX_ROW	5

event_list_elem_t     event_callback_list[EVENT_ATCMD_BLE_MAX][EVENT_MAX_ROW];
int reg_event_handler(unsigned int event_cmds, event_handler_t handler_func, void *handler_user_data)
{
	int i = 0, j = 0;
	if(event_cmds < EVENT_ATCMD_BLE_MAX){
		for(i=0; i < EVENT_MAX_ROW; i++){
			if(event_callback_list[event_cmds][i].handler == NULL){
			    for(j=0; j<EVENT_MAX_ROW; j++){           
			        if(event_callback_list[event_cmds][j].handler == handler_func){
			            return MULTIPLE_REGIST;
			        }
			    }
				event_callback_list[event_cmds][i].handler = handler_func;
				event_callback_list[event_cmds][i].handler_user_data = handler_user_data;
				return OP_OK;
			}
			
		}
		return MAX_ROW_LIMIT;
	} else {
		return ILLEGAL_EVENT;
	}
}

int unreg_event_handler(unsigned int event_cmds, event_handler_t handler_func)
{
	int i;
	if(event_cmds < EVENT_ATCMD_BLE_MAX){
		for(i = 0; i < EVENT_MAX_ROW; i++){
			if(event_callback_list[event_cmds][i].handler == handler_func){
				event_callback_list[event_cmds][i].handler = NULL;
				event_callback_list[event_cmds][i].handler_user_data = NULL;
				return OP_OK;
			}
		}
		return NOT_FOUND_REGIST;
	} else {
		return ILLEGAL_EVENT;
	}
}

void init_event_callback_list(){
	memset(event_callback_list, 0, sizeof(event_callback_list));
}

int indicate_event_handle(unsigned int event_cmd, char *buf, int buf_len, int flags)
{
	event_handler_t handle = NULL;
	int i;

	if(event_cmd >= EVENT_ATCMD_BLE_MAX)
		return ILLEGAL_EVENT;

	for(i = 0; i < EVENT_MAX_ROW; i++){
		handle = event_callback_list[event_cmd][i].handler;
		if(handle == NULL)
			continue;
		handle(buf, buf_len, flags, event_callback_list[event_cmd][i].handler_user_data);
	}

	return OP_OK;
}
