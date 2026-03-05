#ifndef RF_PPA_GAIN_CFG_H_
#define RF_PPA_GAIN_CFG_H_

// # RF_BOARD_VER=0 or undefine for ArcsD EVB,
// # RF_BOARD_VER=1 for Taoyun DVT1
// # RF_BOARD_VER=2 for Taoyun DVT2
// # RF_BOARD_VER=3 for Haier

#if RF_BOARD_VER == 1 //Taoyun DVT1
4, 4, 6, 7, 8, 10, 14, 16, 20, 24, 32, 40, 51, 67, 87, 116, 159, 152, 183
#elif RF_BOARD_VER == 2 //Taoyun DVT2
4, 4, 6, 7, 8, 11, 14, 16, 21, 25, 32, 41, 52, 67, 87, 114, 154, 157, 196
#elif RF_BOARD_VER == 3 //Haier Demo
4, 7, 7, 8, 12, 15, 16, 22, 27, 34, 43, 55, 71, 88, 112, 146, 191, 200, 244
#else //As chip-side result
4, 4, 6, 7, 8, 11, 14, 16, 21, 25, 32, 41, 52, 67, 87, 114, 154, 157, 196
#endif


#endif // RF_PPA_GAIN_CFG_H_
