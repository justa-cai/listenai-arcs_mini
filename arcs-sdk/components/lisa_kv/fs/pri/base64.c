#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lisa_mem.h"

/**
 * characters used for Base64 encoding
 */
static const char *BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * encode three bytes using base64 (RFC 3548)
 *
 * @param triple three bytes that should be encoded
 * @param result buffer of four characters where the result is stored
 */
static void _base64_encode_triple(unsigned char triple[3], char result[4])
{
	int tripleValue, i;

	tripleValue = triple[0];
	tripleValue *= 256;
	tripleValue += triple[1];
	tripleValue *= 256;
	tripleValue += triple[2];

	for (i = 0; i < 4; i++) {
		result[3 - i] = BASE64_CHARS[tripleValue % 64];
		tripleValue /= 64;
	}
}

/**
 * determine the value of a base64 encoding character
 *
 * @param base64char the character of which the value is searched
 * @return the value in case of success (0-63), -1 on failure
 */
static int _base64_char_value(char base64char)
{
	if (base64char >= 'A' && base64char <= 'Z') return base64char - 'A';
	if (base64char >= 'a' && base64char <= 'z') return base64char - 'a' + 26;
	if (base64char >= '0' && base64char <= '9') return base64char - '0' + 2 * 26;
	if (base64char == '+') return 2 * 26 + 10;
	if (base64char == '/') return 2 * 26 + 11;
	return -1;
}

/**
 * decode a 4 char base64 encoded byte triple
 *
 * @param quadruple the 4 characters that should be decoded
 * @param result the decoded data
 * @return lenth of the result (1, 2 or 3), 0 on failure
 */
static int _base64_decode_triple(char quadruple[4], unsigned char *result)
{
	int i, triple_value, bytes_to_decode = 3, only_equals_yet = 1;
	int char_value[4];

	for (i = 0; i < 4; i++)
		char_value[i] = _base64_char_value(quadruple[i]);

	/* check if the characters are valid */
	for (i = 3; i >= 0; i--) {
		if (char_value[i] < 0) {
			if (only_equals_yet && quadruple[i] == '=') {
				/* we will ignore this character anyway, make it something
				 * that does not break our calculations */
				char_value[i] = 0;
				bytes_to_decode--;
				continue;
			}
			return 0;
		}
		/* after we got a real character, no other '=' are allowed anymore */
		only_equals_yet = 0;
	}

	/* if we got "====" as input, bytes_to_decode is -1 */
	if (bytes_to_decode < 0) bytes_to_decode = 0;

	/* make one big value out of the partial values */
	triple_value = char_value[0];
	triple_value *= 64;
	triple_value += char_value[1];
	triple_value *= 64;
	triple_value += char_value[2];
	triple_value *= 64;
	triple_value += char_value[3];

	/* break the big value into bytes */
	for (i = bytes_to_decode; i < 3; i++)
		triple_value /= 256;
	for (i = bytes_to_decode - 1; i >= 0; i--) {
		result[i] = triple_value % 256;
		triple_value /= 256;
	}

	return bytes_to_decode;
}

// base64 转换表, 共64个
static const char base64_alphabet[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
		'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
		'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

// 解码时使用    base64DecodeChars
static const unsigned char lisa_base64_suffix_map[256] = {255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 253, 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62,
		255, 255, 255, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 254, 255, 255,
		255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 255, 255, 255, 255, 255, 255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
		39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

static char cmove_bits(unsigned char src, unsigned lnum, unsigned rnum)
{
	src <<= lnum;
	src >>= rnum;
	return src;
}

unsigned char *lisa_kv_base64_encode(uint8_t *str, int str_len)
{
	long len;
	unsigned char *res;
	int i, j;
	// 定义base64编码表
	unsigned char *base64_table =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	// 计算经过base64编码后的字符串长度
	if (str_len % 3 == 0)
		len = str_len / 3 * 4;
	else
		len = (str_len / 3 + 1) * 4;

	res = lisa_mem_alloc(sizeof(unsigned char) * len + 1);
	res[len] = '\0';

	// 以3个8位字符为一组进行编码
	for (i = 0, j = 0; i < len - 2; j += 3, i += 4) {
		res[i] = base64_table[str[j] >> 2];  // 取出第一个字符的前6位并找出对应的结果字符
		res[i + 1] = base64_table
				[(str[j] & 0x3) << 4 |
						(str[j + 1] >>
								4)];  // 将第一个字符的后位与第二个字符的前4位进行组合并找到对应的结果字符
		res[i + 2] = base64_table
				[(str[j + 1] & 0xf) << 2 |
						(str[j + 2] >>
								6)];  // 将第二个字符的后4位与第三个字符的前2位组合并找出对应的结果字符
		res[i + 3] = base64_table[str[j + 2] & 0x3f];  // 取出第三个字符的后6位并找出结果字符
	}

	switch (str_len % 3) {
		case 1:
			res[i - 2] = '=';
			res[i - 1] = '=';
			break;
		case 2:
			res[i - 1] = '=';
			break;
	}

	return res;
}

int lisa_kv_base64_decode(const unsigned char *indata, int inlen, char *outdata, int *outlen)
{
	int ret = 0;
	if (indata == NULL || inlen <= 0 || outdata == NULL || outlen == NULL) {
		return ret = -1;
	}
	if (inlen % 4 != 0) {  // 需要解码的数据不是4字节倍数
		return ret = -2;
	}

	int t = 0, x = 0, y = 0, i = 0;
	unsigned char c = 0;
	int g = 3;

	// while (indata[x] != 0) {
	while (x < inlen) {
		// 需要解码的数据对应的ASCII值对应base64_suffix_map的值
		c = lisa_base64_suffix_map[indata[x++]];
		if (c == 255) return -1;  // 对应的值不在转码表中
		if (c == 253) continue;  // 对应的值是换行或者回车
		if (c == 254) {
			c = 0;
			g--;
		}  // 对应的值是'='
		t = (t << 6) | c;  // 将其依次放入一个int型中占3字节
		if (++y == 4) {
			outdata[i++] = (unsigned char)((t >> 16) & 0xff);
			if (g > 1) outdata[i++] = (unsigned char)((t >> 8) & 0xff);
			if (g > 2) outdata[i++] = (unsigned char)(t & 0xff);
			y = t = 0;
		}
	}
	*outlen = i;
	return ret;
}
