/*
 * qdec_test.h
 *
 *  Created on:
 *      Author:
 */

#ifndef SRC_QDEC_TEST_H_
#define SRC_QDEC_TEST_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define QDEC_TEST_TIMES			(100)
#define QDEC_TEST_LOG			CLOG

typedef enum {
	QDEC_OP_TYPE_NORMAL = 0,
	QDEC_OP_TYPE_OTHER
} QDEC_OP_TYPE;

struct QDEC_TEST_ITEM {
	char				case_name[32];
	QDEC_OP_TYPE		op_type;
	void 				*test_op;
};


void test_loop(void);

int qdec_x_mode(void);
int qdec_x_swap(void);
int qdec_x_int_of(void);
int qdec_x_int_uf(void);
int qdec_x_int_evt(void);
int qdec_x_evt_th(void);

int qdec_y_mode(void);
int qdec_y_swap(void);
int qdec_y_int_of(void);
int qdec_y_int_uf(void);
int qdec_y_int_evt(void);
int qdec_y_evt_th(void);

int qdec_z_mode(void);
int qdec_z_swap(void);
int qdec_z_int_of(void);
int qdec_z_int_uf(void);
int qdec_z_int_evt(void);
int qdec_z_evt_th(void);

int qdec_xyz_cnt(void);
int qdec_xyz_clear(void);

#ifdef __cplusplus
}
#endif


#endif /* SRC_QDEC_TEST_H_ */
