#include "check.h"

#if USE_LUNA_CHECK

static int32_t g_check_enable = 0;
static void* g_sharemem_addr = 0;
static uint32_t  g_sharemem_size = 0;
static void* g_dma_src_addr = 0;
static void* g_dma_dst_addr = 0;
static uint32_t  g_dma_size = 0;

void luna_set_check_enable(int enable)
{
	g_check_enable = enable == 0 ? 0 : 1;
}

int32_t luna_is_check_enable()
{
	return g_check_enable;
}

void luna_set_sharedmem(void* addr, uint32_t size)
{
	g_sharemem_addr = addr;
	g_sharemem_size = size;
}

int32_t luna_check_addr(const void* addr, uint32_t size, uint32_t alignment, int32_t type)
{
	LUNA_ASSERT(g_sharemem_addr && g_sharemem_size > 0, "sharemem addr not set");
	LUNA_ASSERT(addr && size > 0, "addr(0x%p-%d) is invalid", addr, size);
	LUNA_ASSERT((uintptr_t)addr % alignment == 0, "addr(0x%p-%d) not aligned to (%d) bytes", addr, size, alignment);
	
	LUNA_ASSERT((uintptr_t)(addr) >= (uintptr_t)(g_sharemem_addr) && (uintptr_t)(addr)+(size) <= (uintptr_t)(g_sharemem_addr)+(g_sharemem_size), \
		"addr(0x%p-%d) overflow with sharemem addr(0x%p-%d)", \
		addr, size, g_sharemem_addr, g_sharemem_size);

	if (g_dma_size > 0 && type) // write addr
	{
		LUNA_ASSERT((uintptr_t)(addr) >= (uintptr_t)(g_dma_src_addr)+(g_dma_size) || (uintptr_t)(addr)+(size) <= (uintptr_t)(g_dma_src_addr), \
			"addr(0x%p-%d) overlap with dma addr(0x%p-%d)", \
			addr, size, g_dma_src_addr, g_dma_size);
		LUNA_ASSERT((uintptr_t)(addr) >= (uintptr_t)(g_dma_dst_addr)+(g_dma_size) || (uintptr_t)(addr)+(size) <= (uintptr_t)(g_dma_dst_addr), \
			"addr(0x%p-%d) overlap with dma addr(0x%p-%d)", \
			addr, size, g_dma_dst_addr, g_dma_size);
	}

	return 1;
}

int32_t luna_check_mat_mul_size(uint32_t row, uint32_t col, uint32_t col2, uint32_t in1_bits, uint32_t in2_bits, uint32_t out_bits)
{
	static uint32_t limit_arr[][5] = { { 4,8,4,64,32 },{ 4,2,4,32,16 },{0,0,0,0,0 },{ 2,2,2,16,8 } };
	uint32_t *limit1, *limit2;
	uint32_t in1_bytes = in1_bits >> 3;
	uint32_t in2_bytes = in2_bits >> 3;

	limit1 = limit_arr[in1_bytes -1];
	limit2 = limit_arr[in2_bytes -1];

	LUNA_ASSERT((SIZE_ALIGIN(row, limit1[0])*SIZE_ALIGIN(col, limit1[1]))*(in1_bytes) <= LUNA_SIZE_64K,
		"mat mul left matrix size [%d/%d]*%d * [%d/%d]*%d > 64K", row, limit1[0], limit1[0], col, limit1[1], limit1[1]);

	LUNA_ASSERT((SIZE_ALIGIN(col, limit2[1])*SIZE_ALIGIN(col2, limit2[2]))*(in1_bytes) <= LUNA_SIZE_32K,
		"mat mul right matrix size [%d/%d]*%d * [%d/%d]*%d > 32K", col, limit2[1], limit2[1], col2, limit2[2], limit2[2]);

	return 1;
}

int32_t luna_check_mat_tans_size(uint32_t row, uint32_t col, uint32_t in1_bits)
{
	static uint32_t limit_arr[][5] = { { 16,4,64},{ 8,4,32},{ 0,0,0,0,0 },{ 4,4,16} };
	uint32_t *limit1;
	uint32_t in1_bytes = in1_bits >> 3;

	limit1 = limit_arr[in1_bytes - 1];

	LUNA_ASSERT((SIZE_ALIGIN(row, limit1[0])*SIZE_ALIGIN(col, limit1[1]))*(in1_bytes) <= LUNA_SIZE_64K,
		"left matrix size [%d/%d]*%d * [%d/%d]*%d > 64K", row, limit1[0], limit1[0], col, limit1[1], limit1[1]);

	return 1;
}

int32_t luna_check_mat_tans_inv_size(uint32_t row, uint32_t col, uint32_t in1_bits, uint32_t i_inv, uint32_t o_inv)
{
	static uint32_t limit_arr[][5] = { { 16,4,64 },{ 8,4,32 },{ 0,0,0,0,0 },{ 4,4,16 } };
	uint32_t *limit1;
	uint32_t in1_bytes = in1_bits >> 3;

	limit1 = limit_arr[in1_bytes - 1];

	LUNA_ASSERT((SIZE_ALIGIN(row, limit1[0])*SIZE_ALIGIN(col, limit1[1]))*(in1_bytes) <= LUNA_SIZE_64K,
		"left matrix size [%d/%d]*%d * [%d/%d]*%d > 64K", row, limit1[0], limit1[0], col, limit1[1], limit1[1]);

	return 1;
}

int32_t luna_check_mat_tans_col234_size(uint32_t row, uint32_t col, uint32_t in1_bits)
{
	LUNA_ASSERT(col >= 2 && col <= 4, 
		"left matrix size must col(%d) >= 2 && col(%d) <= 4", col, col);
	return 1;
}

int32_t luna_check_conv_paras(conv_struct_t * pConv, uint32_t in_bits, uint32_t out_bits)
{
	uint32_t output_size = 0;
	uint32_t input_size, kernel_size, input_afterpad_size;

	input_size = SIZE_ALIGIN(pConv->input_w, 8*pConv->stride_w)*pConv->input_h*SIZE_ALIGIN(pConv->input_c, 8);  //?
	// input_afterpad_size = pConv->input_w_after_padding * pConv->input_h_after_padding * pConv->input_c;	 //no use
	kernel_size = SIZE_ALIGIN(pConv->input_c, 8) * pConv->weight_w * pConv->weight_h*SIZE_ALIGIN(pConv->output_c,2);
	output_size = pConv->output_w * pConv->output_h * pConv->output_c * out_bits / 8;
	
	LUNA_CHECK_EQ_3("activation_type", pConv->activation_type, RELU, PRELU, NO_ACTIVE);
	LUNA_CHECK_EQ_2("positive_shift_type", pConv->positive_shift_type, ShiftType_FloorX05, ShiftType_Floor);

	LUNA_CHECK_BETWEEN("weight_h", pConv->weight_h, 1, 5);
	LUNA_CHECK_BETWEEN("weight_w", pConv->weight_w, 1, 5);
	LUNA_CHECK_EQ_3("stride_h", pConv->stride_h, 1, 2, 4);
	LUNA_CHECK_EQ_3("stride_w", pConv->stride_w, 1, 2, 4);
	LUNA_CHECK_BETWEEN("padding_h_up", pConv->padding_h_up, 0, 4);
	LUNA_CHECK_BETWEEN("padding_h_down", pConv->padding_h_down, 0, 4);
	LUNA_CHECK_BETWEEN("padding_w_left", pConv->padding_w_left, 0, 4);
	LUNA_CHECK_BETWEEN("padding_w_right", pConv->padding_w_right, 0, 4);

	LUNA_CHECK_LE("stride_h <= weight_h", pConv->stride_h, pConv->weight_h);
	LUNA_CHECK_LE("stride_w <= weight_w", pConv->stride_w, pConv->weight_w);

	// LUNA_CHECK_LE("weight_h <= input_h_after_padding", pConv->weight_h, pConv->input_h_after_padding); //?
	// LUNA_CHECK_LE("weight_w <= input_w_after_padding", pConv->weight_w, pConv->input_w_after_padding);
	//LUNA_CHECK_LE("weight_h <= input_h", pConv->weight_h, pConv->input_h); //?
	//LUNA_CHECK_LE("weight_w <= input_w", pConv->weight_w, pConv->input_w);

	LUNA_CHECK_LE("padding_h_up <= weight_h", pConv->padding_h_up, pConv->weight_h); //?
	LUNA_CHECK_LE("padding_h_down <= weight_h", pConv->padding_h_down, pConv->weight_h);
	LUNA_CHECK_LE("padding_w_left <= weight_w", pConv->padding_w_left, pConv->weight_w);
	LUNA_CHECK_LE("padding_w_right <= weight_w", pConv->padding_w_right, pConv->weight_w);

	// LUNA_CHECK_EQ_1("input_h_after_padding", pConv->input_h_after_padding, pConv->input_h + pConv->padding_h_up + pConv->padding_h_down);
	// LUNA_CHECK_EQ_1("input_w_after_padding", pConv->input_w_after_padding, pConv->input_w + pConv->padding_w_left + pConv->padding_w_right);
	
	// LUNA_CHECK_EQ_1("output_h", pConv->output_h, (pConv->input_h_after_padding - pConv->weight_h) / pConv->stride_h + 1);
	// LUNA_CHECK_EQ_1("output_w", pConv->output_w, (pConv->input_w_after_padding - pConv->weight_w) / pConv->stride_w + 1);

	LUNA_ASSERT(input_size <= LUNA_SIZE_64K, "input size < 64K, input_size = %d", input_size);
	LUNA_ASSERT(kernel_size <= LUNA_SIZE_32K, "input size < 32K, kernel_size = %d", kernel_size);

	return 1;
}

int32_t luna_check_deconv_paras(conv_struct_t * pConv, uint32_t in_bits, uint32_t out_bits)
{
	uint32_t output_size = 0;
	uint32_t input_size, kernel_size, input_afterpad_size;

	input_size = SIZE_ALIGIN(pConv->input_w, 8 * pConv->stride_w)*pConv->input_h*SIZE_ALIGIN(pConv->input_c, 8);  // validate + stride aligned
	// input_afterpad_size = pConv->input_w_after_padding * pConv->input_h_after_padding * pConv->input_c;	 //no use
	kernel_size = SIZE_ALIGIN(pConv->input_c, 8) * pConv->weight_w * pConv->weight_h*SIZE_ALIGIN(pConv->output_c, 2);
	output_size = pConv->output_w * pConv->output_h * pConv->output_c * out_bits / 8;

	LUNA_CHECK_EQ_3("activation_type", pConv->activation_type, RELU, PRELU, NO_ACTIVE);
	LUNA_CHECK_EQ_2("positive_shift_type", pConv->positive_shift_type, ShiftType_FloorX05, ShiftType_Floor);

	LUNA_CHECK_BETWEEN("weight_h", pConv->weight_h, 1, 5);
	LUNA_CHECK_BETWEEN("weight_w", pConv->weight_w, 1, 5);
	LUNA_CHECK_EQ_3("stride_h", pConv->stride_h, 1, 2, 4);
	LUNA_CHECK_EQ_3("stride_w", pConv->stride_w, 1, 2, 4);
	LUNA_CHECK_BETWEEN("padding_h_up", pConv->padding_h_up, 0, 4);
	LUNA_CHECK_BETWEEN("padding_h_down", pConv->padding_h_down, 0, 4);
	LUNA_CHECK_BETWEEN("padding_w_left", pConv->padding_w_left, 0, 4);
	LUNA_CHECK_BETWEEN("padding_w_right", pConv->padding_w_right, 0, 4);

	LUNA_CHECK_LE("stride_h <= weight_h", pConv->stride_h, pConv->weight_h);
	LUNA_CHECK_LE("stride_w <= weight_w", pConv->stride_w, pConv->weight_w);

	// LUNA_CHECK_LE("weight_h <= input_h_after_padding", pConv->weight_h, pConv->input_h_after_padding); //?
	// LUNA_CHECK_LE("weight_w <= input_w_after_padding", pConv->weight_w, pConv->input_w_after_padding);

	LUNA_CHECK_LE("padding_h_up <= weight_h/2", pConv->padding_h_up, pConv->weight_h >> 1); //?
	LUNA_CHECK_LE("padding_h_down <= weight_h/2", pConv->padding_h_down, pConv->weight_h >> 1);
	LUNA_CHECK_LE("padding_w_left <= weight_w/2", pConv->padding_w_left, pConv->weight_w >> 1);
	LUNA_CHECK_LE("padding_w_right <= weight_w/2", pConv->padding_w_right, pConv->weight_w >> 1);

	// LUNA_CHECK_EQ_1("input_h_after_padding", pConv->input_h_after_padding, (pConv->input_h - 1)*pConv->stride_h + 1 + pConv->padding_h_up + pConv->padding_h_down);
	// LUNA_CHECK_EQ_1("input_w_after_padding", pConv->input_w_after_padding, (pConv->input_w - 1)*pConv->stride_w + 1 + pConv->padding_w_left + pConv->padding_w_right);

	// LUNA_CHECK_EQ_1("output_h", pConv->output_h, (pConv->input_h_after_padding - pConv->weight_h) + 1);
	// LUNA_CHECK_EQ_1("output_w", pConv->output_w, (pConv->input_w_after_padding - pConv->weight_w) + 1);

	LUNA_ASSERT(input_size <= LUNA_SIZE_64K, "input size < 64K, input_size = %d", input_size);
	LUNA_ASSERT(kernel_size <= LUNA_SIZE_32K, "input size < 32K, kernel_size = %d", kernel_size);

	return 1;
}

int32_t luna_check_depthwise_paras(conv_struct_t * pConv, uint32_t in_bits, uint32_t out_bits)
{
	uint32_t output_size = 0;
	uint32_t input_size, kernel_size, input_afterpad_size;

	input_size = SIZE_ALIGIN(pConv->input_w, 8 * pConv->stride_w)*pConv->input_h*SIZE_ALIGIN(pConv->input_c, 8);  // validate
	// input_afterpad_size = pConv->input_w_after_padding * pConv->input_h_after_padding * pConv->input_c;	 //no use
	kernel_size = SIZE_ALIGIN(pConv->input_c, 16) * pConv->weight_w * pConv->weight_h;
	output_size = pConv->output_w * pConv->output_h * pConv->output_c * out_bits / 8;

	LUNA_CHECK_EQ_3("activation_type", pConv->activation_type, RELU, PRELU, NO_ACTIVE);
	LUNA_CHECK_EQ_2("positive_shift_type", pConv->positive_shift_type, ShiftType_FloorX05, ShiftType_Floor);

	LUNA_CHECK_BETWEEN("weight_h", pConv->weight_h, 1, 5);
	LUNA_CHECK_BETWEEN("weight_w", pConv->weight_w, 1, 5);
	LUNA_CHECK_EQ_3("stride_h", pConv->stride_h, 1, 2, 4);
	LUNA_CHECK_EQ_3("stride_w", pConv->stride_w, 1, 2, 4);
	LUNA_CHECK_BETWEEN("padding_h_up", pConv->padding_h_up, 0, 4);
	LUNA_CHECK_BETWEEN("padding_h_down", pConv->padding_h_down, 0, 4);
	LUNA_CHECK_BETWEEN("padding_w_left", pConv->padding_w_left, 0, 4);
	LUNA_CHECK_BETWEEN("padding_w_right", pConv->padding_w_right, 0, 4);
	LUNA_CHECK_EQ_1("output_c == input_c", pConv->output_c, pConv->input_c);

	LUNA_CHECK_LE("stride_h <= weight_h", pConv->stride_h, pConv->weight_h);
	LUNA_CHECK_LE("stride_w <= weight_w", pConv->stride_w, pConv->weight_w);

	// LUNA_CHECK_LE("weight_h <= input_h_after_padding", pConv->weight_h, pConv->input_h_after_padding); //?
	// LUNA_CHECK_LE("weight_w <= input_w_after_padding", pConv->weight_w, pConv->input_w_after_padding);
// 	LUNA_CHECK_LE("weight_h <= input_h", pConv->weight_h, pConv->input_h); //? 
// 	LUNA_CHECK_LE("weight_w <= input_w", pConv->weight_w, pConv->input_w);

	LUNA_CHECK_LE("padding_h_up <= weight_h", pConv->padding_h_up, pConv->weight_h); //?
	LUNA_CHECK_LE("padding_h_down <= weight_h", pConv->padding_h_down, pConv->weight_h);
	LUNA_CHECK_LE("padding_w_left <= weight_w", pConv->padding_w_left, pConv->weight_w);
	LUNA_CHECK_LE("padding_w_right <= weight_w", pConv->padding_w_right, pConv->weight_w);

	// LUNA_CHECK_EQ_1("input_h_after_padding", pConv->input_h_after_padding, pConv->input_h + pConv->padding_h_up + pConv->padding_h_down);
	// LUNA_CHECK_EQ_1("input_w_after_padding", pConv->input_w_after_padding, pConv->input_w + pConv->padding_w_left + pConv->padding_w_right);

	// LUNA_CHECK_EQ_1("output_h", pConv->output_h, (pConv->input_h_after_padding - pConv->weight_h) / pConv->stride_h + 1);
	// LUNA_CHECK_EQ_1("output_w", pConv->output_w, (pConv->input_w_after_padding - pConv->weight_w) / pConv->stride_w + 1);

	LUNA_ASSERT(input_size <= LUNA_SIZE_64K, "input size < 64K, input_size = %d", input_size);
	LUNA_ASSERT(kernel_size <= LUNA_SIZE_32K, "input size < 32K, kernel_size = %d", kernel_size);

	return 1;
}

int32_t luna_check_split_conv_paras(conv_struct_t * pConv, uint32_t in_bits, uint32_t out_bits, uint32_t split_num)
{
	uint32_t output_size = 0;
	uint32_t input_size, kernel_size, input_afterpad_size;

	input_size = SIZE_ALIGIN(pConv->input_w, 8*pConv->stride_w)*pConv->input_h*SIZE_ALIGIN(pConv->input_c, 8);  //?
	// input_afterpad_size = pConv->input_w_after_padding * pConv->input_h_after_padding * pConv->input_c;	 //no use
	kernel_size = SIZE_ALIGIN(pConv->input_c, 8) * pConv->weight_w * pConv->weight_h*SIZE_ALIGIN(pConv->output_c,2);
	output_size = pConv->output_w * pConv->output_h * pConv->output_c * out_bits / 8;

	LUNA_CHECK_EQ_3("activation_type", pConv->activation_type, RELU, PRELU, NO_ACTIVE);
	LUNA_CHECK_EQ_2("positive_shift_type", pConv->positive_shift_type, ShiftType_FloorX05, ShiftType_Floor);

	LUNA_CHECK_BETWEEN("weight_h", pConv->weight_h, 1, 5);
	LUNA_CHECK_BETWEEN("weight_w", pConv->weight_w, 1, 5);
	LUNA_CHECK_EQ_3("stride_h", pConv->stride_h, 1, 2, 4);
	LUNA_CHECK_EQ_3("stride_w", pConv->stride_w, 1, 2, 4);
	LUNA_CHECK_BETWEEN("padding_h_up", pConv->padding_h_up, 0, 4);
	LUNA_CHECK_BETWEEN("padding_h_down", pConv->padding_h_down, 0, 4);
	LUNA_CHECK_BETWEEN("padding_w_left", pConv->padding_w_left, 0, 4);
	LUNA_CHECK_BETWEEN("padding_w_right", pConv->padding_w_right, 0, 4);

	LUNA_CHECK_LE("stride_h <= weight_h", pConv->stride_h, pConv->weight_h);
	LUNA_CHECK_LE("stride_w <= weight_w", pConv->stride_w, pConv->weight_w);

	// LUNA_CHECK_LE("weight_h <= input_h_after_padding", pConv->weight_h, pConv->input_h_after_padding); //?
	// LUNA_CHECK_LE("weight_w <= input_w_after_padding", pConv->weight_w, pConv->input_w_after_padding);
	//LUNA_CHECK_LE("weight_h <= input_h", pConv->weight_h, pConv->input_h); //?
	//LUNA_CHECK_LE("weight_w <= input_w", pConv->weight_w, pConv->input_w);

	LUNA_CHECK_LE("padding_h_up <= weight_h", pConv->padding_h_up, pConv->weight_h); //?
	LUNA_CHECK_LE("padding_h_down <= weight_h", pConv->padding_h_down, pConv->weight_h);
	LUNA_CHECK_LE("padding_w_left <= weight_w", pConv->padding_w_left, pConv->weight_w);
	LUNA_CHECK_LE("padding_w_right <= weight_w", pConv->padding_w_right, pConv->weight_w);

	// LUNA_CHECK_EQ_1("input_h_after_padding", pConv->input_h_after_padding, pConv->input_h + pConv->padding_h_up + pConv->padding_h_down);
	// LUNA_CHECK_EQ_1("input_w_after_padding", pConv->input_w_after_padding, pConv->input_w + pConv->padding_w_left + pConv->padding_w_right);

	// LUNA_CHECK_EQ_1("output_h", pConv->output_h, (pConv->input_h_after_padding - pConv->weight_h) / pConv->stride_h + 1);
	// LUNA_CHECK_EQ_1("output_w", pConv->output_w, (pConv->input_w_after_padding - pConv->weight_w) / pConv->stride_w + 1);

	LUNA_ASSERT((input_size / split_num) <= LUNA_SIZE_64K, "input size < 64K, input_size = %d", input_size);
	LUNA_ASSERT(kernel_size <= LUNA_SIZE_32K, "input size < 32K, kernel_size = %d", kernel_size);

	//split cnn param validate
	int in_h = pConv->input_h;
	int in_w = pConv->input_w;
	int kernel_h = pConv->weight_h;
	int stride_h = pConv->stride_h;
	int pad_ht = pConv->padding_h_up;
	int pad_hb = pConv->padding_h_down;
	int cal_var0 = in_h + pad_ht + pad_hb - kernel_h + stride_h;
	int cal_var1 = floor(cal_var0 / stride_h);
	int cal_var2 = floor(cal_var1 / split_num);
	int cal_var3 = cal_var2 * stride_h - stride_h - pad_ht;
	int cal_var4 = cal_var2 * stride_h - stride_h - pad_hb;
	int condition = (0 == (cal_var0 % stride_h)) && (0 == (cal_var1 % split_num)) && (cal_var3 >= 0) && (cal_var4 >= 0);

	LUNA_ASSERT(((condition != 0) && (in_h * in_w <= LUNA_SIZE_32K)), "split cnn param validate failed");

	return 1;
}

int32_t luna_check_dma_cpy(int chn, void *dst, void *src, int32_t size)
{
	LUNA_ASSERT(g_dma_size == 0, "dma cpy not complete");

	if ((uintptr_t)dst >= (uintptr_t)g_sharemem_addr + g_sharemem_size || (uintptr_t)dst + size <= (uintptr_t)g_sharemem_addr)
	{
		LUNA_ASSERT(((uintptr_t)dst) % 4 == 0, "addr(0x%p) not aligned to (%d) bytes", dst, 4);
	}

	g_dma_src_addr = src;
	g_dma_dst_addr = dst;
	g_dma_size = size;

	return 1;
}

int32_t luna_check_dma_wait(int chn)
{
	LUNA_ASSERT(g_dma_size != 0, "dma_cpy must be called before dma_wait");

	g_dma_src_addr = 0;
	g_dma_dst_addr = 0;
	g_dma_size = 0;

	return 1;
}

#else 

void luna_set_check_enable(int enable)
{

}

int32_t luna_is_check_enable()
{

}

void luna_set_sharedmem(void* addr, uint32_t size)
{

}

#endif
