/*
 * Static InfoNES mapper table. Remove entries or disable macros in
 * nes_mapper_infones_config.h to trim unsupported mappers.
 */

#include "nes_mapper_infones.h"

const struct infones_mapper_entry infones_mapper_table[] = {
#if INFONES_ENABLE_MAPPER_000
    { 0, Map0_Init },
#endif
#if INFONES_ENABLE_MAPPER_001
    { 1, Map1_Init },
#endif
#if INFONES_ENABLE_MAPPER_002
    { 2, Map2_Init },
#endif
#if INFONES_ENABLE_MAPPER_003
    { 3, Map3_Init },
#endif
#if INFONES_ENABLE_MAPPER_004
    { 4, Map4_Init },
#endif
#if INFONES_ENABLE_MAPPER_007
    { 7, Map7_Init },
#endif
#if INFONES_ENABLE_MAPPER_008
    { 8, Map8_Init },
#endif
#if INFONES_ENABLE_MAPPER_009
    { 9, Map9_Init },
#endif
#if INFONES_ENABLE_MAPPER_010
    { 10, Map10_Init },
#endif
#if INFONES_ENABLE_MAPPER_011
    { 11, Map11_Init },
#endif
#if INFONES_ENABLE_MAPPER_013
    { 13, Map13_Init },
#endif
#if INFONES_ENABLE_MAPPER_015
    { 15, Map15_Init },
#endif
#if INFONES_ENABLE_MAPPER_016
    { 16, Map16_Init },
#endif
#if INFONES_ENABLE_MAPPER_017
    { 17, Map17_Init },
#endif
#if INFONES_ENABLE_MAPPER_018
    { 18, Map18_Init },
#endif
#if INFONES_ENABLE_MAPPER_019
    { 19, Map19_Init },
#endif
#if INFONES_ENABLE_MAPPER_021
    { 21, Map21_Init },
#endif
#if INFONES_ENABLE_MAPPER_022
    { 22, Map22_Init },
#endif
#if INFONES_ENABLE_MAPPER_023
    { 23, Map23_Init },
#endif
#if INFONES_ENABLE_MAPPER_024
    { 24, Map24_Init },
#endif
#if INFONES_ENABLE_MAPPER_025
    { 25, Map25_Init },
#endif
#if INFONES_ENABLE_MAPPER_026
    { 26, Map26_Init },
#endif
#if INFONES_ENABLE_MAPPER_030
    { 30, Map30_Init },
#endif
#if INFONES_ENABLE_MAPPER_032
    { 32, Map32_Init },
#endif
#if INFONES_ENABLE_MAPPER_033
    { 33, Map33_Init },
#endif
#if INFONES_ENABLE_MAPPER_034
    { 34, Map34_Init },
#endif
#if INFONES_ENABLE_MAPPER_040
    { 40, Map40_Init },
#endif
#if INFONES_ENABLE_MAPPER_041
    { 41, Map41_Init },
#endif
#if INFONES_ENABLE_MAPPER_042
    { 42, Map42_Init },
#endif
#if INFONES_ENABLE_MAPPER_043
    { 43, Map43_Init },
#endif
#if INFONES_ENABLE_MAPPER_044
    { 44, Map44_Init },
#endif
#if INFONES_ENABLE_MAPPER_045
    { 45, Map45_Init },
#endif
#if INFONES_ENABLE_MAPPER_046
    { 46, Map46_Init },
#endif
#if INFONES_ENABLE_MAPPER_047
    { 47, Map47_Init },
#endif
#if INFONES_ENABLE_MAPPER_048
    { 48, Map48_Init },
#endif
#if INFONES_ENABLE_MAPPER_049
    { 49, Map49_Init },
#endif
#if INFONES_ENABLE_MAPPER_050
    { 50, Map50_Init },
#endif
#if INFONES_ENABLE_MAPPER_051
    { 51, Map51_Init },
#endif
#if INFONES_ENABLE_MAPPER_057
    { 57, Map57_Init },
#endif
#if INFONES_ENABLE_MAPPER_058
    { 58, Map58_Init },
#endif
#if INFONES_ENABLE_MAPPER_060
    { 60, Map60_Init },
#endif
#if INFONES_ENABLE_MAPPER_061
    { 61, Map61_Init },
#endif
#if INFONES_ENABLE_MAPPER_062
    { 62, Map62_Init },
#endif
#if INFONES_ENABLE_MAPPER_064
    { 64, Map64_Init },
#endif
#if INFONES_ENABLE_MAPPER_065
    { 65, Map65_Init },
#endif
#if INFONES_ENABLE_MAPPER_066
    { 66, Map66_Init },
#endif
#if INFONES_ENABLE_MAPPER_067
    { 67, Map67_Init },
#endif
#if INFONES_ENABLE_MAPPER_068
    { 68, Map68_Init },
#endif
#if INFONES_ENABLE_MAPPER_069
    { 69, Map69_Init },
#endif
#if INFONES_ENABLE_MAPPER_070
    { 70, Map70_Init },
#endif
#if INFONES_ENABLE_MAPPER_071
    { 71, Map71_Init },
#endif
#if INFONES_ENABLE_MAPPER_072
    { 72, Map72_Init },
#endif
#if INFONES_ENABLE_MAPPER_073
    { 73, Map73_Init },
#endif
#if INFONES_ENABLE_MAPPER_074
    { 74, Map74_Init },
#endif
#if INFONES_ENABLE_MAPPER_075
    { 75, Map75_Init },
#endif
#if INFONES_ENABLE_MAPPER_076
    { 76, Map76_Init },
#endif
#if INFONES_ENABLE_MAPPER_077
    { 77, Map77_Init },
#endif
#if INFONES_ENABLE_MAPPER_078
    { 78, Map78_Init },
#endif
#if INFONES_ENABLE_MAPPER_079
    { 79, Map79_Init },
#endif
#if INFONES_ENABLE_MAPPER_080
    { 80, Map80_Init },
#endif
#if INFONES_ENABLE_MAPPER_082
    { 82, Map82_Init },
#endif
#if INFONES_ENABLE_MAPPER_083
    { 83, Map83_Init },
#endif
#if INFONES_ENABLE_MAPPER_086
    { 86, Map86_Init },
#endif
#if INFONES_ENABLE_MAPPER_087
    { 87, Map87_Init },
#endif
#if INFONES_ENABLE_MAPPER_088
    { 88, Map88_Init },
#endif
#if INFONES_ENABLE_MAPPER_089
    { 89, Map89_Init },
#endif
#if INFONES_ENABLE_MAPPER_090
    { 90, Map90_Init },
#endif
#if INFONES_ENABLE_MAPPER_091
    { 91, Map91_Init },
#endif
#if INFONES_ENABLE_MAPPER_092
    { 92, Map92_Init },
#endif
#if INFONES_ENABLE_MAPPER_093
    { 93, Map93_Init },
#endif
#if INFONES_ENABLE_MAPPER_094
    { 94, Map94_Init },
#endif
#if INFONES_ENABLE_MAPPER_095
    { 95, Map95_Init },
#endif
#if INFONES_ENABLE_MAPPER_096
    { 96, Map96_Init },
#endif
#if INFONES_ENABLE_MAPPER_097
    { 97, Map97_Init },
#endif
#if INFONES_ENABLE_MAPPER_100
    { 100, Map100_Init },
#endif
#if INFONES_ENABLE_MAPPER_101
    { 101, Map101_Init },
#endif
#if INFONES_ENABLE_MAPPER_105
    { 105, Map105_Init },
#endif
#if INFONES_ENABLE_MAPPER_107
    { 107, Map107_Init },
#endif
#if INFONES_ENABLE_MAPPER_108
    { 108, Map108_Init },
#endif
#if INFONES_ENABLE_MAPPER_109
    { 109, Map109_Init },
#endif
#if INFONES_ENABLE_MAPPER_110
    { 110, Map110_Init },
#endif
#if INFONES_ENABLE_MAPPER_112
    { 112, Map112_Init },
#endif
#if INFONES_ENABLE_MAPPER_113
    { 113, Map113_Init },
#endif
#if INFONES_ENABLE_MAPPER_114
    { 114, Map114_Init },
#endif
#if INFONES_ENABLE_MAPPER_115
    { 115, Map115_Init },
#endif
#if INFONES_ENABLE_MAPPER_116
    { 116, Map116_Init },
#endif
#if INFONES_ENABLE_MAPPER_117
    { 117, Map117_Init },
#endif
#if INFONES_ENABLE_MAPPER_118
    { 118, Map118_Init },
#endif
#if INFONES_ENABLE_MAPPER_119
    { 119, Map119_Init },
#endif
#if INFONES_ENABLE_MAPPER_122
    { 122, Map122_Init },
#endif
#if INFONES_ENABLE_MAPPER_133
    { 133, Map133_Init },
#endif
#if INFONES_ENABLE_MAPPER_134
    { 134, Map134_Init },
#endif
#if INFONES_ENABLE_MAPPER_135
    { 135, Map135_Init },
#endif
#if INFONES_ENABLE_MAPPER_140
    { 140, Map140_Init },
#endif
#if INFONES_ENABLE_MAPPER_151
    { 151, Map151_Init },
#endif
#if INFONES_ENABLE_MAPPER_160
    { 160, Map160_Init },
#endif
#if INFONES_ENABLE_MAPPER_180
    { 180, Map180_Init },
#endif
#if INFONES_ENABLE_MAPPER_181
    { 181, Map181_Init },
#endif
#if INFONES_ENABLE_MAPPER_182
    { 182, Map182_Init },
#endif
#if INFONES_ENABLE_MAPPER_183
    { 183, Map183_Init },
#endif
#if INFONES_ENABLE_MAPPER_184
    { 184, Map184_Init },
#endif
#if INFONES_ENABLE_MAPPER_185
    { 185, Map185_Init },
#endif
#if INFONES_ENABLE_MAPPER_187
    { 187, Map187_Init },
#endif
#if INFONES_ENABLE_MAPPER_188
    { 188, Map188_Init },
#endif
#if INFONES_ENABLE_MAPPER_189
    { 189, Map189_Init },
#endif
#if INFONES_ENABLE_MAPPER_191
    { 191, Map191_Init },
#endif
#if INFONES_ENABLE_MAPPER_193
    { 193, Map193_Init },
#endif
#if INFONES_ENABLE_MAPPER_194
    { 194, Map194_Init },
#endif
#if INFONES_ENABLE_MAPPER_200
    { 200, Map200_Init },
#endif
#if INFONES_ENABLE_MAPPER_201
    { 201, Map201_Init },
#endif
#if INFONES_ENABLE_MAPPER_202
    { 202, Map202_Init },
#endif
#if INFONES_ENABLE_MAPPER_206
    { 206, Map206_Init },
#endif
#if INFONES_ENABLE_MAPPER_222
    { 222, Map222_Init },
#endif
#if INFONES_ENABLE_MAPPER_225
    { 225, Map225_Init },
#endif
#if INFONES_ENABLE_MAPPER_226
    { 226, Map226_Init },
#endif
#if INFONES_ENABLE_MAPPER_227
    { 227, Map227_Init },
#endif
#if INFONES_ENABLE_MAPPER_228
    { 228, Map228_Init },
#endif
#if INFONES_ENABLE_MAPPER_229
    { 229, Map229_Init },
#endif
#if INFONES_ENABLE_MAPPER_230
    { 230, Map230_Init },
#endif
#if INFONES_ENABLE_MAPPER_231
    { 231, Map231_Init },
#endif
#if INFONES_ENABLE_MAPPER_232
    { 232, Map232_Init },
#endif
#if INFONES_ENABLE_MAPPER_233
    { 233, Map233_Init },
#endif
#if INFONES_ENABLE_MAPPER_234
    { 234, Map234_Init },
#endif
#if INFONES_ENABLE_MAPPER_235
    { 235, Map235_Init },
#endif
#if INFONES_ENABLE_MAPPER_236
    { 236, Map236_Init },
#endif
#if INFONES_ENABLE_MAPPER_240
    { 240, Map240_Init },
#endif
#if INFONES_ENABLE_MAPPER_241
    { 241, Map241_Init },
#endif
#if INFONES_ENABLE_MAPPER_242
    { 242, Map242_Init },
#endif
#if INFONES_ENABLE_MAPPER_243
    { 243, Map243_Init },
#endif
#if INFONES_ENABLE_MAPPER_244
    { 244, Map244_Init },
#endif
#if INFONES_ENABLE_MAPPER_245
    { 245, Map245_Init },
#endif
#if INFONES_ENABLE_MAPPER_246
    { 246, Map246_Init },
#endif
#if INFONES_ENABLE_MAPPER_248
    { 248, Map248_Init },
#endif
#if INFONES_ENABLE_MAPPER_249
    { 249, Map249_Init },
#endif
#if INFONES_ENABLE_MAPPER_251
    { 251, Map251_Init },
#endif
#if INFONES_ENABLE_MAPPER_252
    { 252, Map252_Init },
#endif
#if INFONES_ENABLE_MAPPER_255
    { 255, Map255_Init },
#endif
};

const size_t infones_mapper_count = sizeof(infones_mapper_table) / sizeof(infones_mapper_table[0]);
