#ifndef __MRPC_M2S_TEST_API_CLIENT_H__
#define __MRPC_M2S_TEST_API_CLIENT_H__

ls_err_t m2s_echo_req(struct ipc_ep * mrpc_ep, struct cfg_ipc_echo * echo_req, struct cfg_ipc_echo_resp * echo_resp);

ls_err_t m2s_start_slave_test(struct ipc_ep * mrpc_ep, struct ipc_test_param * conf);

ls_err_t m2s_stop_slave_test(struct ipc_ep * mrpc_ep);


#endif