/*
 * luna.c
 *
 *  Created on: Sep 7, 2017
 *      Author: dwwang
 */

#include "luna_sim/luna.h"

static volatile uint32_t g_last_counter = 0;

uint32_t LUNA_API_SIM(luna_version)()
{
	return LUNA_VERSION;
}

void LUNA_API_SIM(start_counter)()
{
	g_last_counter = clock();
}

uint32_t LUNA_API_SIM(get_counter)()
{
	return clock() - g_last_counter;
}
