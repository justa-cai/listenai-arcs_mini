#ifndef __PROC_MGR_H__
#define __PROC_MGR_H__

void app_proc_init(struct app_client_s *app_client, struct app_cloud_s *cloud);

void app_proc_msg(const char* msg, int len);

int started_wait(int timeout);
void started_reset(void);

#endif // __PROC_MGR_H__