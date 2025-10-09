#include "../include/fs_env/fs_env.h"

#if(CONFIG_FS_ENV_OS_FREERTOS)

int fs_env_mutex_create(fs_env_mutex_handle_t* handle){

	handle->mutex = xSemaphoreCreateRecursiveMutex();
	return 0;
}

int fs_env_mutex_lock(fs_env_mutex_handle_t* handle,int timeout_ms){
	
	int ret;

	if(FS_ENV_MAX_DELAY == timeout_ms){
		ret = xSemaphoreTakeRecursive(handle->mutex, portMAX_DELAY);
	}
	else{
		ret = xSemaphoreTakeRecursive(handle->mutex, pdMS_TO_TICKS(timeout_ms));
		
	}
	
	return ret;
}

int fs_env_mutex_unlock(fs_env_mutex_handle_t* handle){
	int ret;

	ret = xSemaphoreGiveRecursive(handle->mutex);

	return ret;
}

int fs_env_mutex_destroy(fs_env_mutex_handle_t* handle){

	vSemaphoreDelete(handle->mutex);
	
	return 0;
}

#elif(CONFIG_FS_ENV_OS_BARE_METAL)

int fs_env_mutex_create(fs_env_mutex_handle_t* handle){

	ARG_UNUSED(handle);
	return 0;
}

int fs_env_mutex_lock(fs_env_mutex_handle_t* handle,int timeout_ms){
	
	ARG_UNUSED(handle);
	ARG_UNUSED(timeout_ms);
	
	return 0;
}

int fs_env_mutex_unlock(fs_env_mutex_handle_t* handle){
	
	ARG_UNUSED(handle);
	return 0;
}
int fs_env_mutex_destroy(fs_env_mutex_handle_t* handle){
	
	ARG_UNUSED(handle);
	return 0;
}
#else
#error "FS_ENV_OS_TYPE is not defined"
#endif