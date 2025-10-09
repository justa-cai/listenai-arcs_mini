#include "luna_cnn_cmd.h"


__luna_cmd_attr__ uint32_t luna_api_split_cnn[] = {
    0x10200000,  //     ldro  r1, r0, #0
    0x61060000,  //     mov  r6, #0
                 // __loop_load_weight:
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x10a10140,  //     ldro  r5, r1, #320
    0x4b05000f,  //     and  r5, #0xf
    0x0b050000,  //     cmp  r5, #0
    0x0d100004,  //     beq  #4
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300004,  //     b  #4
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x8a800200,  //     mnts  sel=grp4,lmwr=s0_0
    0x8aa00240,  //     mnts  sel=grp5,lmwr=s0_1
    0x86130000,  //     memc   mode=1,access=1,grp4=1,grp5=1
    0x108100e8,  //     ldro  r4, r1, #232
    0x44840600,  //     sub  r4, r6
    0x0b040001,  //     cmp  r4, #1
    0x0d100003,  //     beq  #3
    0x11010000,  //     ldro  r8, r1, #0
    0x0d300002,  //     b  #2
    0x11010004,  //     ldro  r8, r1, #4
    0x62090800,  //     mov  r9, r8
    0x40680000,  //     setrh  r8, #0
    0x51090010,  //     lsr  r9, #16
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x10410144,  //     ldro  r2, r1, #324
    0x0b020004,  //     cmp  r2, #4
    0x0d100003,  //     beq  #3
    0x418a0008,  //     setrl  r10,#8
    0x0d300002,  //     b  #2
    0x418a0004,  //     setrl  r10,#4
    0x418b0000,  //     setrl  r11,#0
    0x11c00008,  //     ldro  r14, r0, #8
    0x10610104,  //     ldro  r3, r1, #260
    0x58830600,  //     mul  r3, r6
    0x440e0300,  //     add  r14, r3
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0b020004,  //     cmp  r2, #4
    0x0d100002,  //     beq  #2
    0x0d300002,  //     b  #2
    0x900a2001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over8,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x0b040001,  //     cmp  r4, #1
    0x0d100003,  //     beq  #3
    0x12410008,  //     ldro  r18, r1, #8
    0x0d300002,  //     b  #2
    0x1241000c,  //     ldro  r18, r1, #12
    0x40720000,  //     setrh  r18,#0
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96000040,  //     dstm0  slave0,mode=normal,precision=0, slave0,inband=64bit,outband=128bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x61070000,  //     mov  r7, #0
                 // __loop_load_feature:
    0xe041c801,  //     rst  sel=reset,master0,pecore,regtab1,regtab2,regtab3
    0x10a10140,  //     ldro  r5, r1, #320
    0x51050004,  //     lsr  r5, #4
    0x0b050000,  //     cmp  r5, #0
    0x0d100004,  //     beq  #4
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300004,  //     b  #4
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x8a400280,  //     mnts  sel=grp2,lmwr=s0_2
    0x8a6002c0,  //     mnts  sel=grp3,lmwr=s0_3
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x104100ec,  //     ldro  r2, r1, #236
    0x62030200,  //     mov  r3, r2
    0x44820700,  //     sub  r2, r7
    0x11010010,  //     ldro  r8, r1, #16
    0x11210014,  //     ldro  r9, r1, #20
    0x0b030001,  //     cmp  r3, #1
    0x0d10000b,  //     beq  __input_h_branch0_end
    0x11010018,  //     ldro  r8, r1, #24
    0x1121001c,  //     ldro  r9, r1, #28
    0x0a020300,  //     cmp  r2, r3
    0x0d100007,  //     beq  __input_h_branch0_end
    0x11010020,  //     ldro  r8, r1, #32
    0x11210024,  //     ldro  r9, r1, #36
    0x0b020001,  //     cmp  r2, #1
    0x0d100003,  //     beq  __input_h_branch0_end
    0x11010028,  //     ldro  r8, r1, #40
    0x1121002c,  //     ldro  r9, r1, #44
                 // __input_h_branch0_end:
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x11010030,  //     ldro  r8, r1, #48
    0x62090800,  //     mov  r9, r8
    0x40680000,  //     setrh  r8, #0
    0x51090010,  //     lsr  r9, #16
    0x10810134,  //     ldro  r4, r1, #308
    0x4b04ffff,  //     and  r4, #0xffff
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x41890001,  //     setrl  r9, #1
    0x0b043fff,  //     cmp  r4, #16383
    0x0d080003,  //     ble  #3
    0x9000400c,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00001100]
    0x0d300002,  //     b  #2
    0x90004000,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00000000]
    0x11410034,  //     ldro  r10, r1, #52
    0x11810038,  //     ldro  r12, r1, #56
    0x620b0a00,  //     mov  r11, r10
    0x620d0c00,  //     mov  r13, r12
    0x550a0010,  //     lsl  r10, #16
    0x418a0008,  //     setrl  r10,#8
    0x510b0010,  //     lsr  r11, #16
    0x550c0010,  //     lsl  r12, #16
    0x510d0010,  //     lsr  r13, #16
    0x10a10134,  //     ldro  r5, r1, #308
    0x0b053fff,  //     cmp  r5, #16383
    0x0d08000a,  //     ble  #10
    0x51050010,  //     lsr  r5, #16
    0x440b0500,  //     add  r11, r5
    0x620d0400,  //     mov  r13, r4
    0x510d0008,  //     lsr  r13, #8
    0x550d0008,  //     lsl  r13, #8
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x450d0001,  //     add  r13, #1
    0x406d0000,  //     setrh  r13,#0
    0x11c00004,  //     ldro  r14, r0, #4
    0x0a020300,  //     cmp  r2, r3
    0x0d100008,  //     beq  #8
    0x108100f0,  //     ldro  r4, r1, #240
    0x440e0400,  //     add  r14, r4
    0x62050700,  //     mov  r5, r7
    0x45850001,  //     sub  r5, #1
    0x108100f4,  //     ldro  r4, r1, #244
    0x58850400,  //     mul  r5, r4
    0x440e0500,  //     add  r14, r5
    0x90012435,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over4,eac[0100],cas[00110101]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x1241003c,  //     ldro  r18, r1, #60
    0x12610040,  //     ldro  r19, r1, #64
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch1_end
    0x12610044,  //     ldro  r19, r1, #68
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch1_end
    0x12610048,  //     ldro  r19, r1, #72
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch1_end
    0x1261004c,  //     ldro  r19, r1, #76
                 // __input_h_branch1_end:
    0x40730000,  //     setrh  r19,#0
    0x12810050,  //     ldro  r20, r1, #80
    0x62151400,  //     mov  r21, r20
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x96000280,  //     dstm0  slave0,mode=rotate,blk=0,precision=0, slave0,inband=64bit,outband=256bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8613f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1,grp4=1,grp5=1
    0x10810138,  //     ldro  r4, r1, #312
    0x0b040001,  //     cmp  r4, #1
    0x0d100006,  //     beq  #6
    0x10a1013c,  //     ldro  r5, r1, #316
    0x0b050001,  //     cmp  r5, #1
    0x0d100003,  //     beq  #3
    0x882ba980,  //     mnts  sel=mas0-1,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x0d30000a,  //     b  #10
    0x10a10140,  //     ldro  r5, r1, #320
    0x4b05000f,  //     and  r5, #0xf
    0x0b050000,  //     cmp  r5, #0
    0x0d100004,  //     beq  #4
    0x89800003,  //     mnts  sel=io-ahb,rd=3
    0x882ba983,  //     mnts  sel=mas0-1,p0-iodat=ioahb,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x0d300004,  //     b  #4
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x8920000b,  //     mnts  sel=io-rd0,inside=3,outside=prd0
    0x882ba981,  //     mnts  sel=mas0-1,p0-iodat=iord0,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x88400dc0,  //     mnts  sel=mas0-2,p1-lmdat0=grp4,p1-lmdat1=grp5
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x8a40000a,  //     mnts  sel=grp2,lmrd=m0p0_2
    0x8a60000b,  //     mnts  sel=grp3,lmrd=m0p0_3
    0x8a800010,  //     mnts  sel=grp4,lmrd=m0p1_0
    0x8aa00011,  //     mnts  sel=grp5,lmrd=m0p1_1
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x11010054,  //     ldro  r8, r1, #84
    0x11210058,  //     ldro  r9, r1, #88
    0x90050a00,  //     ares  mode=master,sel=select0,chs[0101],sel_row0,cmc[1010],ces[00000000]
    0x1101005c,  //     ldro  r8, r1, #92
    0x11210060,  //     ldro  r9, r1, #96
    0x90054a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row1,cmc[1010],ces[01010100]
    0x108100e8,  //     ldro  r4, r1, #232
    0x44840600,  //     sub  r4, r6
    0x0b040001,  //     cmp  r4, #1
    0x0d100004,  //     beq  #4
    0x11010064,  //     ldro  r8, r1, #100
    0x11210068,  //     ldro  r9, r1, #104
    0x0d300003,  //     b  #3
    0x1101006c,  //     ldro  r8, r1, #108
    0x11210070,  //     ldro  r9, r1, #112
    0x90058a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row2,cmc[1010],ces[01010100]
    0x900950aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line1,elf[0000],efcs[10101010]
    0x9009d0aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line3,elf[0000],efcs[10101010]
    0x11410084,  //     ldro  r10, r1, #132
    0x11610088,  //     ldro  r11, r1, #136
    0x1181008c,  //     ldro  r12, r1, #140
    0x11a10090,  //     ldro  r13, r1, #144
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch3_end
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
                 // __input_h_branch3_end:
    0x9002a055,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over10,eac[0000],cas[01010101]
    0x11410098,  //     ldro  r10, r1, #152
    0x118100a0,  //     ldro  r12, r1, #160
    0x9006a005,  //     ares  mode=master,sel=select2,sel_port0,sel_high8,sel_master_over10,eac[0000],cas[00000101]
    0x114100a8,  //     ldro  r10, r1, #168
    0x116100ac,  //     ldro  r11, r1, #172
    0x118100b0,  //     ldro  r12, r1, #176
    0x11a100b4,  //     ldro  r13, r1, #180
    0x11c100b8,  //     ldro  r14, r1, #184
    0x90082055,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[01010101]
    0x114100bc,  //     ldro  r10, r1, #188
    0x118100c4,  //     ldro  r12, r1, #196
    0x122100cc,  //     ldro  r17, r1, #204
    0x124100d0,  //     ldro  r18, r1, #208
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch2_end
    0x124100d4,  //     ldro  r18, r1, #212
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch2_end
    0x124100d8,  //     ldro  r18, r1, #216
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch2_end
    0x124100dc,  //     ldro  r18, r1, #220
                 // __input_h_branch2_end:
    0x900d2005,  //     ares  mode=master,sel=select2,sel_port1,sel_high8,sel_master_over4,eac[0000],cas[00000101]
    0x418f0009,  //     setrl  r15,#9
    0x90013502,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp0,dcl2
    0x120100e0,  //     ldro  r16, r1, #224
    0x90057502,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line1,sel_data_sour5,sel_dp0,dcl2
    0x41900000,  //     setrl  r16,#0x0000
    0x90013581,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp1,dcl1
    0x418a0000,  //     setrl  r10,#0
    0x11c0000c,  //     ldro  r14, r0, #12
    0x10810108,  //     ldro  r4, r1, #264
    0x58840600,  //     mul  r4, r6
    0x440e0400,  //     add  r14, r4
    0x110100e4,  //     ldro  r8, r1, #228
    0x12e1010c,  //     ldro  r23, r1, #268
    0x12c10110,  //     ldro  r22, r1, #272
    0x41980000,  //     setrl  r24, #0
    0x40780000,  //     setrh  r24, #0
    0x41990000,  //     setrl  r25, #0
    0x40790000,  //     setrh  r25, #0
    0x13610114,  //     ldro  r27, r1, #276
    0x419a4053,  //     setrl  r26, #0x4053
    0x10810148,  //     ldro  r4, r1, #328
    0x0b040020,  //     cmp  r4, #32
    0x0d100004,  //     beq  #4
    0x407a0802,  //     setrh  r26, #0x0802
    0x98c10210,  //     dprc   intgnet=l3,intgnet-mode=1,comshift=0,outlayer=l6,outlayer-mode=2
    0x0d300003,  //     b  #3
    0x407a0822,  //     setrh  r26, #0x0822
    0x98c10010,  //     dprc   intgnet=l3,intgnet-mode=1,comshift=0,outlayer=l6,outlayer-mode=0
    0x12800010,  //     ldro  r20, r0, #16
    0x10810100,  //     ldro  r4, r1, #256
    0x58840600,  //     mul  r4, r6
    0x10a100fc,  //     ldro  r5, r1, #252
    0x58850700,  //     mul  r5, r7
    0x44040500,  //     add  r4, r5
    0x44140400,  //     add  r20, r4
    0x12410118,  //     ldro  r18, r1, #280
    0x108100e8,  //     ldro  r4, r1, #232
    0x44840600,  //     sub  r4, r6
    0x0b040001,  //     cmp  r4, #1
    0x0d100003,  //     beq  #3
    0x12610128,  //     ldro  r19, r1, #296
    0x0d300002,  //     b  #2
    0x1261012c,  //     ldro  r19, r1, #300
    0x12a10130,  //     ldro  r21, r1, #304
    0x10810148,  //     ldro  r4, r1, #328
    0x0b040020,  //     cmp  r4, #32
    0x0d100003,  //     beq  #3
    0xa4000013,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=1,ch0=1
    0x0d300002,  //     b  #2
    0xa4000033,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=1,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x108100ec,  //     ldro  r4, r1, #236
    0x45070001,  //     add  r7, #1
    0x0a070400,  //     cmp  r7, r4
    0x0d18ff0f,  //     bne  __loop_load_feature
    0x10a100e8,  //     ldro  r5, r1, #232
    0x45060001,  //     add  r6, #1
    0x0a060500,  //     cmp  r6, r5
    0x0d18fed0,  //     bne  __loop_load_weight
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_split_depthwise[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x10200000,  //     ldro  r1, r0, #0
    0x10a10140,  //     ldro  r5, r1, #320
    0x4b05000f,  //     and  r5, #0xf
    0x0b050000,  //     cmp  r5, #0
    0x0d100004,  //     beq  #4
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300004,  //     b  #4
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00009,  //     mnts  sel=slv0,mode=1,din=m0l
    0x8a800200,  //     mnts  sel=grp4,lmwr=s0_0
    0x8aa00240,  //     mnts  sel=grp5,lmwr=s0_1
    0x86130000,  //     memc   mode=1,access=1,grp4=1,grp5=1
    0x11010000,  //     ldro  r8, r1, #0
    0x62090800,  //     mov  r9, r8
    0x40680000,  //     setrh  r8, #0
    0x51090010,  //     lsr  r9, #16
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x10410144,  //     ldro  r2, r1, #324
    0x0b020004,  //     cmp  r2, #4
    0x0d100003,  //     beq  #3
    0x418a0008,  //     setrl  r10,#8
    0x0d300002,  //     b  #2
    0x418a0004,  //     setrl  r10,#4
    0x418b0000,  //     setrl  r11,#0
    0x11c00008,  //     ldro  r14, r0, #8
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0b020004,  //     cmp  r2, #4
    0x0d100002,  //     beq  #2
    0x0d300002,  //     b  #2
    0x900a2001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over8,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x12410008,  //     ldro  r18, r1, #8
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96000000,  //     dstm0  slave0,mode=normal,precision=0, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x61070000,  //     mov  r7, #0
                 // __loop_load_feature:
    0xe041c801,  //     rst  sel=reset,master0,pecore,regtab1,regtab2,regtab3
    0x10a10140,  //     ldro  r5, r1, #320
    0x51050004,  //     lsr  r5, #4
    0x0b050000,  //     cmp  r5, #0
    0x0d100004,  //     beq  #4
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300004,  //     b  #4
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x8a400280,  //     mnts  sel=grp2,lmwr=s0_2
    0x8a6002c0,  //     mnts  sel=grp3,lmwr=s0_3
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x104100ec,  //     ldro  r2, r1, #236
    0x62030200,  //     mov  r3, r2
    0x44820700,  //     sub  r2, r7
    0x11010010,  //     ldro  r8, r1, #16
    0x11210014,  //     ldro  r9, r1, #20
    0x0b030001,  //     cmp  r3, #1
    0x0d10000b,  //     beq  __input_h_branch0_end
    0x11010018,  //     ldro  r8, r1, #24
    0x1121001c,  //     ldro  r9, r1, #28
    0x0a020300,  //     cmp  r2, r3
    0x0d100007,  //     beq  __input_h_branch0_end
    0x11010020,  //     ldro  r8, r1, #32
    0x11210024,  //     ldro  r9, r1, #36
    0x0b020001,  //     cmp  r2, #1
    0x0d100003,  //     beq  __input_h_branch0_end
    0x11010028,  //     ldro  r8, r1, #40
    0x1121002c,  //     ldro  r9, r1, #44
                 // __input_h_branch0_end:
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x11010030,  //     ldro  r8, r1, #48
    0x62090800,  //     mov  r9, r8
    0x40680000,  //     setrh  r8, #0
    0x51090010,  //     lsr  r9, #16
    0x10810134,  //     ldro  r4, r1, #308
    0x4b04ffff,  //     and  r4, #0xffff
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x41890001,  //     setrl  r9, #1
    0x0b043fff,  //     cmp  r4, #16383
    0x0d080003,  //     ble  #3
    0x9000400c,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00001100]
    0x0d300002,  //     b  #2
    0x90004000,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00000000]
    0x11410034,  //     ldro  r10, r1, #52
    0x11810038,  //     ldro  r12, r1, #56
    0x620b0a00,  //     mov  r11, r10
    0x620d0c00,  //     mov  r13, r12
    0x550a0010,  //     lsl  r10, #16
    0x418a0008,  //     setrl  r10,#8
    0x510b0010,  //     lsr  r11, #16
    0x550c0010,  //     lsl  r12, #16
    0x510d0010,  //     lsr  r13, #16
    0x10a10134,  //     ldro  r5, r1, #308
    0x0b053fff,  //     cmp  r5, #16383
    0x0d08000a,  //     ble  #10
    0x51050010,  //     lsr  r5, #16
    0x440b0500,  //     add  r11, r5
    0x620d0400,  //     mov  r13, r4
    0x510d0008,  //     lsr  r13, #8
    0x550d0008,  //     lsl  r13, #8
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x450d0001,  //     add  r13, #1
    0x406d0000,  //     setrh  r13,#0
    0x11c00004,  //     ldro  r14, r0, #4
    0x0a020300,  //     cmp  r2, r3
    0x0d100008,  //     beq  #8
    0x108100f0,  //     ldro  r4, r1, #240
    0x440e0400,  //     add  r14, r4
    0x62050700,  //     mov  r5, r7
    0x45850001,  //     sub  r5, #1
    0x108100f4,  //     ldro  r4, r1, #244
    0x58850400,  //     mul  r5, r4
    0x440e0500,  //     add  r14, r5
    0x90012435,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over4,eac[0100],cas[00110101]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x1241003c,  //     ldro  r18, r1, #60
    0x12610040,  //     ldro  r19, r1, #64
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch1_end
    0x12610044,  //     ldro  r19, r1, #68
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch1_end
    0x12610048,  //     ldro  r19, r1, #72
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch1_end
    0x1261004c,  //     ldro  r19, r1, #76
                 // __input_h_branch1_end:
    0x40730000,  //     setrh  r19,#0
    0x12810050,  //     ldro  r20, r1, #80
    0x62151400,  //     mov  r21, r20
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x96000280,  //     dstm0  slave0,mode=rotate,blk=0,precision=0, slave0,inband=64bit,outband=256bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8613f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1,grp4=1,grp5=1
    0x88000020,  //     mnts  sel=mas0-0,mode1=1
    0x10810138,  //     ldro  r4, r1, #312
    0x0b040001,  //     cmp  r4, #1
    0x0d100006,  //     beq  #6
    0x10a1013c,  //     ldro  r5, r1, #316
    0x0b050001,  //     cmp  r5, #1
    0x0d100003,  //     beq  #3
    0x882ba980,  //     mnts  sel=mas0-1,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x0d30000a,  //     b  #10
    0x10a10140,  //     ldro  r5, r1, #320
    0x4b05000f,  //     and  r5, #0xf
    0x0b050000,  //     cmp  r5, #0
    0x0d100004,  //     beq  #4
    0x89800003,  //     mnts  sel=io-ahb,rd=3
    0x882ba983,  //     mnts  sel=mas0-1,p0-iodat=ioahb,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x0d300004,  //     b  #4
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x8920000b,  //     mnts  sel=io-rd0,inside=3,outside=prd0
    0x882ba981,  //     mnts  sel=mas0-1,p0-iodat=iord0,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x88400dc0,  //     mnts  sel=mas0-2,p1-lmdat0=grp4,p1-lmdat1=grp5
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x8a40000a,  //     mnts  sel=grp2,lmrd=m0p0_2
    0x8a60000b,  //     mnts  sel=grp3,lmrd=m0p0_3
    0x8a800010,  //     mnts  sel=grp4,lmrd=m0p1_0
    0x8aa00011,  //     mnts  sel=grp5,lmrd=m0p1_1
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x11010054,  //     ldro  r8, r1, #84
    0x11210058,  //     ldro  r9, r1, #88
    0x90050a00,  //     ares  mode=master,sel=select0,chs[0101],sel_row0,cmc[1010],ces[00000000]
    0x1101005c,  //     ldro  r8, r1, #92
    0x11210060,  //     ldro  r9, r1, #96
    0x90054a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row1,cmc[1010],ces[01010100]
    0x11010064,  //     ldro  r8, r1, #100
    0x11210068,  //     ldro  r9, r1, #104
    0x90058a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row2,cmc[1010],ces[01010100]
    0x1101006c,  //     ldro  r8, r1, #108
    0x11210070,  //     ldro  r9, r1, #112
    0x9005ca54,  //     ares  mode=master,sel=select0,chs[0101],sel_row3,cmc[1010],ces[01010100]
    0x900950aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line1,elf[0000],efcs[10101010]
    0x9009d0aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line3,elf[0000],efcs[10101010]
    0x11410084,  //     ldro  r10, r1, #132
    0x11610088,  //     ldro  r11, r1, #136
    0x1181008c,  //     ldro  r12, r1, #140
    0x11a10090,  //     ldro  r13, r1, #144
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch3_end
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
                 // __input_h_branch3_end:
    0x90032055,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over12,eac[0000],cas[01010101]
    0x11410098,  //     ldro  r10, r1, #152
    0x1161009c,  //     ldro  r11, r1, #156
    0x118100a0,  //     ldro  r12, r1, #160
    0x11a100a4,  //     ldro  r13, r1, #164
    0x90072015,  //     ares  mode=master,sel=select2,sel_port0,sel_high8,sel_master_over12,eac[0000],cas[00010101]
    0x114100a8,  //     ldro  r10, r1, #168
    0x116100ac,  //     ldro  r11, r1, #172
    0x118100b0,  //     ldro  r12, r1, #176
    0x11a100b4,  //     ldro  r13, r1, #180
    0x11c100b8,  //     ldro  r14, r1, #184
    0x90082055,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[01010101]
    0x114100bc,  //     ldro  r10, r1, #188
    0x116100c0,  //     ldro  r11, r1, #192
    0x118100c4,  //     ldro  r12, r1, #196
    0x11a100c8,  //     ldro  r13, r1, #200
    0x122100cc,  //     ldro  r17, r1, #204
    0x124100d0,  //     ldro  r18, r1, #208
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch2_end
    0x124100d4,  //     ldro  r18, r1, #212
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch2_end
    0x124100d8,  //     ldro  r18, r1, #216
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch2_end
    0x124100dc,  //     ldro  r18, r1, #220
                 // __input_h_branch2_end:
    0x900d2011,  //     ares  mode=master,sel=select2,sel_port1,sel_high8,sel_master_over4,eac[0000],cas[00010001]
    0x418f0009,  //     setrl  r15,#9
    0x90013502,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp0,dcl2
    0x120100e0,  //     ldro  r16, r1, #224
    0x90057502,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line1,sel_data_sour5,sel_dp0,dcl2
    0x41910400,  //     setrl  r17,#0x0400
    0x9004b502,  //     ares  mode=master,sel=select3,rcils[0100],sel_counter_line2,sel_data_sour5,sel_dp0,dcl2
    0x41900000,  //     setrl  r16,#0x0000
    0x41910200,  //     setrl  r17,#0x0200
    0x90013580,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp1,dcl0
    0x9004b580,  //     ares  mode=master,sel=select3,rcils[0100],sel_counter_line2,sel_data_sour5,sel_dp1,dcl0
    0x11c0000c,  //     ldro  r14, r0, #12
    0x110100e4,  //     ldro  r8, r1, #228
    0x12e1010c,  //     ldro  r23, r1, #268
    0x12c10110,  //     ldro  r22, r1, #272
    0x41987700,  //     setrl  r24, #0x7700
    0x40780000,  //     setrh  r24, #0
    0x41990000,  //     setrl  r25, #0
    0x40790000,  //     setrh  r25, #0
    0x13610114,  //     ldro  r27, r1, #276
    0x419a4053,  //     setrl  r26, #0x4053
    0x10810148,  //     ldro  r4, r1, #328
    0x0b040020,  //     cmp  r4, #32
    0x0d100005,  //     beq  #5
    0x407a0002,  //     setrh  r26, #0x0002
    0x98000001,  //     dprc   mmac0=1
    0x98c10218,  //     dprc   layer3-mode=2,intgnet=l3,intgnet-mode=1,comshift=0,outlayer=l6,outlayer-mode=2
    0x0d300004,  //     b  #4
    0x407a0022,  //     setrh  r26, #0x0022
    0x98000001,  //     dprc   mmac0=1
    0x98c10018,  //     dprc   layer3-mode=2,intgnet=l3,intgnet-mode=1,comshift=0,outlayer=l6,outlayer-mode=0
    0x12800010,  //     ldro  r20, r0, #16
    0x10a100fc,  //     ldro  r5, r1, #252
    0x58850700,  //     mul  r5, r7
    0x44140500,  //     add  r20, r5
    0x12410118,  //     ldro  r18, r1, #280
    0x12610128,  //     ldro  r19, r1, #296
    0x12a10130,  //     ldro  r21, r1, #304
    0x10810148,  //     ldro  r4, r1, #328
    0x0b040020,  //     cmp  r4, #32
    0x0d100003,  //     beq  #3
    0xa4000013,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=1,ch0=1
    0x0d300002,  //     b  #2
    0xa4000033,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=1,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x108100ec,  //     ldro  r4, r1, #236
    0x45070001,  //     add  r7, #1
    0x0a070400,  //     cmp  r7, r4
    0x0d18ff15,  //     bne  __loop_load_feature
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_split_maxpool[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x10200000,  //     ldro  r1, r0, #0
    0x61070000,  //     mov  r7, #0
                 // __loop_load_feature:
    0xe041c801,  //     rst  sel=reset,master0,pecore,regtab1,regtab2,regtab3
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x8a400280,  //     mnts  sel=grp2,lmwr=s0_2
    0x8a6002c0,  //     mnts  sel=grp3,lmwr=s0_3
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x104100ec,  //     ldro  r2, r1, #236
    0x62030200,  //     mov  r3, r2
    0x44820700,  //     sub  r2, r7
    0x11010010,  //     ldro  r8, r1, #16
    0x11210014,  //     ldro  r9, r1, #20
    0x0b030001,  //     cmp  r3, #1
    0x0d10000b,  //     beq  __input_h_branch0_end
    0x11010018,  //     ldro  r8, r1, #24
    0x1121001c,  //     ldro  r9, r1, #28
    0x0a020300,  //     cmp  r2, r3
    0x0d100007,  //     beq  __input_h_branch0_end
    0x11010020,  //     ldro  r8, r1, #32
    0x11210024,  //     ldro  r9, r1, #36
    0x0b020001,  //     cmp  r2, #1
    0x0d100003,  //     beq  __input_h_branch0_end
    0x11010028,  //     ldro  r8, r1, #40
    0x1121002c,  //     ldro  r9, r1, #44
                 // __input_h_branch0_end:
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x11010030,  //     ldro  r8, r1, #48
    0x62090800,  //     mov  r9, r8
    0x40680000,  //     setrh  r8, #0
    0x51090010,  //     lsr  r9, #16
    0x10810134,  //     ldro  r4, r1, #308
    0x4b04ffff,  //     and  r4, #0xffff
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x41890001,  //     setrl  r9, #1
    0x0b043fff,  //     cmp  r4, #16383
    0x0d080003,  //     ble  #3
    0x9000400c,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00001100]
    0x0d300002,  //     b  #2
    0x90004000,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00000000]
    0x11410034,  //     ldro  r10, r1, #52
    0x11810038,  //     ldro  r12, r1, #56
    0x620b0a00,  //     mov  r11, r10
    0x620d0c00,  //     mov  r13, r12
    0x550a0010,  //     lsl  r10, #16
    0x418a0008,  //     setrl  r10,#8
    0x510b0010,  //     lsr  r11, #16
    0x550c0010,  //     lsl  r12, #16
    0x510d0010,  //     lsr  r13, #16
    0x10a10134,  //     ldro  r5, r1, #308
    0x0b053fff,  //     cmp  r5, #16383
    0x0d08000a,  //     ble  #10
    0x51050010,  //     lsr  r5, #16
    0x440b0500,  //     add  r11, r5
    0x620d0400,  //     mov  r13, r4
    0x510d0008,  //     lsr  r13, #8
    0x550d0008,  //     lsl  r13, #8
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x450d0001,  //     add  r13, #1
    0x406d0000,  //     setrh  r13,#0
    0x11c00004,  //     ldro  r14, r0, #4
    0x0a020300,  //     cmp  r2, r3
    0x0d100008,  //     beq  #8
    0x108100f0,  //     ldro  r4, r1, #240
    0x440e0400,  //     add  r14, r4
    0x62050700,  //     mov  r5, r7
    0x45850001,  //     sub  r5, #1
    0x108100f4,  //     ldro  r4, r1, #244
    0x58850400,  //     mul  r5, r4
    0x440e0500,  //     add  r14, r5
    0x90012435,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over4,eac[0100],cas[00110101]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x1241003c,  //     ldro  r18, r1, #60
    0x12610040,  //     ldro  r19, r1, #64
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch1_end
    0x12610044,  //     ldro  r19, r1, #68
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch1_end
    0x12610048,  //     ldro  r19, r1, #72
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch1_end
    0x1261004c,  //     ldro  r19, r1, #76
                 // __input_h_branch1_end:
    0x40730000,  //     setrh  r19,#0
    0x12810050,  //     ldro  r20, r1, #80
    0x62151400,  //     mov  r21, r20
    0x40740000,  //     setrh  r20,#0
    0x51150010,  //     lsr  r21, #16
    0x45150001,  //     add  r21, #0x1
    0x55150010,  //     lsl  r21, #16
    0x96000280,  //     dstm0  slave0,mode=rotate,blk=0,precision=0, slave0,inband=64bit,outband=256bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x41880101,  //     setrl  r8, #0x0101
    0x40680101,  //     setrh  r8, #0x0101
    0x882ba980,  //     mnts  sel=mas0-1,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x88400660,  //     mnts  sel=mas0-2,p1-lmdat0=r8,p1-lmdat1=r8
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x8a40000a,  //     mnts  sel=grp2,lmrd=m0p0_2
    0x8a60000b,  //     mnts  sel=grp3,lmrd=m0p0_3
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x11010054,  //     ldro  r8, r1, #84
    0x11210058,  //     ldro  r9, r1, #88
    0x90050a00,  //     ares  mode=master,sel=select0,chs[0101],sel_row0,cmc[1010],ces[00000000]
    0x1101005c,  //     ldro  r8, r1, #92
    0x11210060,  //     ldro  r9, r1, #96
    0x90054a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row1,cmc[1010],ces[01010100]
    0x11010064,  //     ldro  r8, r1, #100
    0x11210068,  //     ldro  r9, r1, #104
    0x90058a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row2,cmc[1010],ces[01010100]
    0x1101006c,  //     ldro  r8, r1, #108
    0x11210070,  //     ldro  r9, r1, #112
    0x9005ca54,  //     ares  mode=master,sel=select0,chs[0101],sel_row3,cmc[1010],ces[01010100]
    0x900850aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line1,elf[0000],efcs[10101010]
    0x9008d0aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line3,elf[0000],efcs[10101010]
    0x11410084,  //     ldro  r10, r1, #132
    0x11610088,  //     ldro  r11, r1, #136
    0x1181008c,  //     ldro  r12, r1, #140
    0x11a10090,  //     ldro  r13, r1, #144
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch3_end
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
                 // __input_h_branch3_end:
    0x90032055,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over12,eac[0000],cas[01010101]
    0x11410098,  //     ldro  r10, r1, #152
    0x1161009c,  //     ldro  r11, r1, #156
    0x118100a0,  //     ldro  r12, r1, #160
    0x11a100a4,  //     ldro  r13, r1, #164
    0x90072015,  //     ares  mode=master,sel=select2,sel_port0,sel_high8,sel_master_over12,eac[0000],cas[00010101]
    0x114100a8,  //     ldro  r10, r1, #168
    0x116100ac,  //     ldro  r11, r1, #172
    0x118100b0,  //     ldro  r12, r1, #176
    0x11a100b4,  //     ldro  r13, r1, #180
    0x11c100b8,  //     ldro  r14, r1, #184
    0x90082055,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[01010101]
    0x114100bc,  //     ldro  r10, r1, #188
    0x116100c0,  //     ldro  r11, r1, #192
    0x118100c4,  //     ldro  r12, r1, #196
    0x11a100c8,  //     ldro  r13, r1, #200
    0x41916002,  //     setrl  r17,#0x6002
    0x40710200,  //     setrh  r17,#512
    0x124100d0,  //     ldro  r18, r1, #208
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch2_end
    0x124100d4,  //     ldro  r18, r1, #212
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch2_end
    0x124100d8,  //     ldro  r18, r1, #216
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch2_end
    0x124100dc,  //     ldro  r18, r1, #220
                 // __input_h_branch2_end:
    0x900d2011,  //     ares  mode=master,sel=select2,sel_port1,sel_high8,sel_master_over4,eac[0000],cas[00010001]
    0x418f0009,  //     setrl  r15,#9
    0x90013502,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp0,dcl2
    0x120100e0,  //     ldro  r16, r1, #224
    0x90057502,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line1,sel_data_sour5,sel_dp0,dcl2
    0x41910400,  //     setrl  r17,#0x0400
    0x9004b502,  //     ares  mode=master,sel=select3,rcils[0100],sel_counter_line2,sel_data_sour5,sel_dp0,dcl2
    0x41900000,  //     setrl  r16,#0x0000
    0x41910200,  //     setrl  r17,#0x0200
    0x90013580,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp1,dcl0
    0x9004b580,  //     ares  mode=master,sel=select3,rcils[0100],sel_counter_line2,sel_data_sour5,sel_dp1,dcl0
    0x110100e4,  //     ldro  r8, r1, #228
    0x12e1010c,  //     ldro  r23, r1, #268
    0x12c10110,  //     ldro  r22, r1, #272
    0x41987700,  //     setrl  r24, #0x7700
    0x419a4053,  //     setrl  r26, #0x4053
    0x407a0402,  //     setrh  r26, #0x0402
    0x98000001,  //     dprc   mmac0=1
    0x98c10258,  //     dprc   layer3-mode=2,intgnet=l3,intgnet-mode=5,comshift=0,outlayer=l6,outlayer-mode=2
    0x12800010,  //     ldro  r20, r0, #16
    0x10a100fc,  //     ldro  r5, r1, #252
    0x58850700,  //     mul  r5, r7
    0x44140500,  //     add  r20, r5
    0x12410118,  //     ldro  r18, r1, #280
    0x12610128,  //     ldro  r19, r1, #296
    0x12a10130,  //     ldro  r21, r1, #304
    0xa4000013,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=1,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x108100ec,  //     ldro  r4, r1, #236
    0x45070001,  //     add  r7, #1
    0x0a070400,  //     cmp  r7, r4
    0x0d18ff3c,  //     bne  __loop_load_feature
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_split_meanpool[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x10200000,  //     ldro  r1, r0, #0
    0x61070000,  //     mov  r7, #0
                 // __loop_load_feature:
    0xe041c801,  //     rst  sel=reset,master0,pecore,regtab1,regtab2,regtab3
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x8a400280,  //     mnts  sel=grp2,lmwr=s0_2
    0x8a6002c0,  //     mnts  sel=grp3,lmwr=s0_3
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x104100ec,  //     ldro  r2, r1, #236
    0x62030200,  //     mov  r3, r2
    0x44820700,  //     sub  r2, r7
    0x11010010,  //     ldro  r8, r1, #16
    0x11210014,  //     ldro  r9, r1, #20
    0x0b030001,  //     cmp  r3, #1
    0x0d10000b,  //     beq  __input_h_branch0_end
    0x11010018,  //     ldro  r8, r1, #24
    0x1121001c,  //     ldro  r9, r1, #28
    0x0a020300,  //     cmp  r2, r3
    0x0d100007,  //     beq  __input_h_branch0_end
    0x11010020,  //     ldro  r8, r1, #32
    0x11210024,  //     ldro  r9, r1, #36
    0x0b020001,  //     cmp  r2, #1
    0x0d100003,  //     beq  __input_h_branch0_end
    0x11010028,  //     ldro  r8, r1, #40
    0x1121002c,  //     ldro  r9, r1, #44
                 // __input_h_branch0_end:
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x11010030,  //     ldro  r8, r1, #48
    0x62090800,  //     mov  r9, r8
    0x40680000,  //     setrh  r8, #0
    0x51090010,  //     lsr  r9, #16
    0x10810134,  //     ldro  r4, r1, #308
    0x4b04ffff,  //     and  r4, #0xffff
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x41890001,  //     setrl  r9, #1
    0x0b043fff,  //     cmp  r4, #16383
    0x0d080003,  //     ble  #3
    0x9000400c,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00001100]
    0x0d300002,  //     b  #2
    0x90004000,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00000000]
    0x11410034,  //     ldro  r10, r1, #52
    0x11810038,  //     ldro  r12, r1, #56
    0x620b0a00,  //     mov  r11, r10
    0x620d0c00,  //     mov  r13, r12
    0x550a0010,  //     lsl  r10, #16
    0x418a0008,  //     setrl  r10,#8
    0x510b0010,  //     lsr  r11, #16
    0x550c0010,  //     lsl  r12, #16
    0x510d0010,  //     lsr  r13, #16
    0x10a10134,  //     ldro  r5, r1, #308
    0x0b053fff,  //     cmp  r5, #16383
    0x0d08000a,  //     ble  #10
    0x51050010,  //     lsr  r5, #16
    0x440b0500,  //     add  r11, r5
    0x620d0400,  //     mov  r13, r4
    0x510d0008,  //     lsr  r13, #8
    0x550d0008,  //     lsl  r13, #8
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x450d0001,  //     add  r13, #1
    0x406d0000,  //     setrh  r13,#0
    0x11c00004,  //     ldro  r14, r0, #4
    0x0a020300,  //     cmp  r2, r3
    0x0d100008,  //     beq  #8
    0x108100f0,  //     ldro  r4, r1, #240
    0x440e0400,  //     add  r14, r4
    0x62050700,  //     mov  r5, r7
    0x45850001,  //     sub  r5, #1
    0x108100f4,  //     ldro  r4, r1, #244
    0x58850400,  //     mul  r5, r4
    0x440e0500,  //     add  r14, r5
    0x90012435,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over4,eac[0100],cas[00110101]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x1241003c,  //     ldro  r18, r1, #60
    0x12610040,  //     ldro  r19, r1, #64
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch1_end
    0x12610044,  //     ldro  r19, r1, #68
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch1_end
    0x12610048,  //     ldro  r19, r1, #72
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch1_end
    0x1261004c,  //     ldro  r19, r1, #76
                 // __input_h_branch1_end:
    0x40730000,  //     setrh  r19,#0
    0x12810050,  //     ldro  r20, r1, #80
    0x62151400,  //     mov  r21, r20
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x96000280,  //     dstm0  slave0,mode=rotate,blk=0,precision=0, slave0,inband=64bit,outband=256bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x41880101,  //     setrl  r8,#0x0101
    0x40680101,  //     setrh  r8,#0x0101
    0x882ba980,  //     mnts  sel=mas0-1,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x88400660,  //     mnts  sel=mas0-2,p1-lmdat0=r8,p1-lmdat1=r8
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x8a40000a,  //     mnts  sel=grp2,lmrd=m0p0_2
    0x8a60000b,  //     mnts  sel=grp3,lmrd=m0p0_3
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x11010054,  //     ldro  r8, r1, #84
    0x11210058,  //     ldro  r9, r1, #88
    0x90050a00,  //     ares  mode=master,sel=select0,chs[0101],sel_row0,cmc[1010],ces[00000000]
    0x1101005c,  //     ldro  r8, r1, #92
    0x11210060,  //     ldro  r9, r1, #96
    0x90054a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row1,cmc[1010],ces[01010100]
    0x11010064,  //     ldro  r8, r1, #100
    0x11210068,  //     ldro  r9, r1, #104
    0x90058a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row2,cmc[1010],ces[01010100]
    0x1101006c,  //     ldro  r8, r1, #108
    0x11210070,  //     ldro  r9, r1, #112
    0x9005ca54,  //     ares  mode=master,sel=select0,chs[0101],sel_row3,cmc[1010],ces[01010100]
    0x900950aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line1,elf[0000],efcs[10101010]
    0x9009d0aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line3,elf[0000],efcs[10101010]
    0x11410084,  //     ldro  r10, r1, #132
    0x11610088,  //     ldro  r11, r1, #136
    0x1181008c,  //     ldro  r12, r1, #140
    0x11a10090,  //     ldro  r13, r1, #144
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch3_end
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
                 // __input_h_branch3_end:
    0x90032055,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over12,eac[0000],cas[01010101]
    0x11410098,  //     ldro  r10, r1, #152
    0x1161009c,  //     ldro  r11, r1, #156
    0x118100a0,  //     ldro  r12, r1, #160
    0x11a100a4,  //     ldro  r13, r1, #164
    0x90072015,  //     ares  mode=master,sel=select2,sel_port0,sel_high8,sel_master_over12,eac[0000],cas[00010101]
    0x114100a8,  //     ldro  r10, r1, #168
    0x116100ac,  //     ldro  r11, r1, #172
    0x118100b0,  //     ldro  r12, r1, #176
    0x11a100b4,  //     ldro  r13, r1, #180
    0x11c100b8,  //     ldro  r14, r1, #184
    0x90082055,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[01010101]
    0x114100bc,  //     ldro  r10, r1, #188
    0x116100c0,  //     ldro  r11, r1, #192
    0x118100c4,  //     ldro  r12, r1, #196
    0x11a100c8,  //     ldro  r13, r1, #200
    0x41910001,  //     setrl  r17,#1
    0x40710200,  //     setrh  r17,#512
    0x124100d0,  //     ldro  r18, r1, #208
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __input_h_branch2_end
    0x124100d4,  //     ldro  r18, r1, #212
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __input_h_branch2_end
    0x124100d8,  //     ldro  r18, r1, #216
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __input_h_branch2_end
    0x124100dc,  //     ldro  r18, r1, #220
                 // __input_h_branch2_end:
    0x900d2011,  //     ares  mode=master,sel=select2,sel_port1,sel_high8,sel_master_over4,eac[0000],cas[00010001]
    0x418f0009,  //     setrl  r15,#9
    0x90013502,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp0,dcl2
    0x120100e0,  //     ldro  r16, r1, #224
    0x90057502,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line1,sel_data_sour5,sel_dp0,dcl2
    0x41910400,  //     setrl  r17,#0x0400
    0x9004b502,  //     ares  mode=master,sel=select3,rcils[0100],sel_counter_line2,sel_data_sour5,sel_dp0,dcl2
    0x41900000,  //     setrl  r16,#0x0000
    0x41910200,  //     setrl  r17,#0x0200
    0x90013580,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp1,dcl0
    0x9004b580,  //     ares  mode=master,sel=select3,rcils[0100],sel_counter_line2,sel_data_sour5,sel_dp1,dcl0
    0x110100e4,  //     ldro  r8, r1, #228
    0x12e1010c,  //     ldro  r23, r1, #268
    0x12c10110,  //     ldro  r22, r1, #272
    0x41987700,  //     setrl  r24, #0x7700
    0x40780000,  //     setrh  r24, #0
    0x41990000,  //     setrl  r25, #0
    0x40790000,  //     setrh  r25, #0
    0x13610114,  //     ldro  r27, r1, #276
    0x419a4053,  //     setrl  r26, #0x4053
    0x10810148,  //     ldro  r4, r1, #328
    0x0b040020,  //     cmp  r4, #32
    0x0d100005,  //     beq  #5
    0x407a0002,  //     setrh  r26, #0x0002
    0x98000001,  //     dprc   mmac0=1
    0x98c10218,  //     dprc   layer3-mode=2,intgnet=l3,intgnet-mode=1,comshift=0,outlayer=l6,outlayer-mode=2
    0x0d300004,  //     b  #4
    0x407a0022,  //     setrh  r26, #0x0022
    0x98000001,  //     dprc   mmac0=1
    0x98c10018,  //     dprc   layer3-mode=2,intgnet=l3,intgnet-mode=1,comshift=0,outlayer=l6,outlayer-mode=0
    0x12800010,  //     ldro  r20, r0, #16
    0x10a100fc,  //     ldro  r5, r1, #252
    0x58850700,  //     mul  r5, r7
    0x44140500,  //     add  r20, r5
    0x12410118,  //     ldro  r18, r1, #280
    0x12610128,  //     ldro  r19, r1, #296
    0x12a10130,  //     ldro  r21, r1, #304
    0x10810148,  //     ldro  r4, r1, #328
    0x0b040020,  //     cmp  r4, #32
    0x0d100003,  //     beq  #3
    0xa4000013,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=1,ch0=1
    0x0d300002,  //     b  #2
    0xa4000033,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=1,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x108100ec,  //     ldro  r4, r1, #236
    0x45070001,  //     add  r7, #1
    0x0a070400,  //     cmp  r7, r4
    0x0d18ff2e,  //     bne  __loop_load_feature
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_split_deconv[] = {
    0x10200000,  //     ldro  r1, r0, #0
    0x61060000,  //     mov  r6, #0
                 // __loop_load_weight:
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x10a10140,  //     ldro  r5, r1, #320
    0x0b050000,  //     cmp  r5, #0
    0x0d100004,  //     beq  #4
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300004,  //     b  #4
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x8a800200,  //     mnts  sel=grp4,lmwr=s0_0
    0x8aa00240,  //     mnts  sel=grp5,lmwr=s0_1
    0x86130000,  //     memc   mode=1,access=1,grp4=1,grp5=1
    0x108100e8,  //     ldro  r4, r1, #232
    0x44840600,  //     sub  r4, r6
    0x0b040001,  //     cmp  r4, #1
    0x0d100003,  //     beq  #3
    0x11010000,  //     ldro  r8, r1, #0
    0x0d300002,  //     b  #2
    0x11010004,  //     ldro  r8, r1, #4
    0x62090800,  //     mov  r9, r8
    0x40680000,  //     setrh  r8, #0
    0x51090010,  //     lsr  r9, #16
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x10410144,  //     ldro  r2, r1, #324
    0x0b020004,  //     cmp  r2, #4
    0x0d100003,  //     beq  #3
    0x418a0008,  //     setrl  r10,#8
    0x0d300002,  //     b  #2
    0x418a0004,  //     setrl  r10,#4
    0x418b0000,  //     setrl  r11,#0
    0x11c00008,  //     ldro  r14, r0, #8
    0x10610104,  //     ldro  r3, r1, #260
    0x58830600,  //     mul  r3, r6
    0x440e0300,  //     add  r14, r3
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0b020004,  //     cmp  r2, #4
    0x0d100002,  //     beq  #2
    0x0d300002,  //     b  #2
    0x900a2001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over8,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x0b040001,  //     cmp  r4, #1
    0x0d100003,  //     beq  #3
    0x12410008,  //     ldro  r18, r1, #8
    0x0d300002,  //     b  #2
    0x1241000c,  //     ldro  r18, r1, #12
    0x40720000,  //     setrh  r18,#0
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96000040,  //     dstm0  slave0,mode=normal,precision=0, slave0,inband=64bit,outband=128bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x61070000,  //     mov  r7, #0
                 // __loop_load_feature:
    0xe041d801,  //     rst  sel=reset,master0,pecore,regtab1,regtab2,regtab3,router
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x8a400280,  //     mnts  sel=grp2,lmwr=s0_2
    0x8a6002c0,  //     mnts  sel=grp3,lmwr=s0_3
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x104100ec,  //     ldro  r2, r1, #236
    0x62030200,  //     mov  r3, r2
    0x44820700,  //     sub  r2, r7
    0x11010010,  //     ldro  r8, r1, #16
    0x11210014,  //     ldro  r9, r1, #20
    0x0b030001,  //     cmp  r3, #1
    0x0d100007,  //     beq  __input_h_branch0_end
    0x11010018,  //     ldro  r8, r1, #24
    0x1121001c,  //     ldro  r9, r1, #28
    0x0a020300,  //     cmp  r2, r3
    0x0d100003,  //     beq  __input_h_branch0_end
    0x11010028,  //     ldro  r8, r1, #40
    0x1121002c,  //     ldro  r9, r1, #44
                 // __input_h_branch0_end:
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x11010030,  //     ldro  r8, r1, #48
    0x62090800,  //     mov  r9, r8
    0x40680000,  //     setrh  r8, #0
    0x51090010,  //     lsr  r9, #16
    0x10810134,  //     ldro  r4, r1, #308
    0x4b04ffff,  //     and  r4, #0xffff
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x41890001,  //     setrl  r9, #1
    0x0b043fff,  //     cmp  r4, #16383
    0x0d080003,  //     ble  #3
    0x9000400c,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00001100]
    0x0d300002,  //     b  #2
    0x90004000,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00000000]
    0x11410034,  //     ldro  r10, r1, #52
    0x11810038,  //     ldro  r12, r1, #56
    0x620b0a00,  //     mov  r11, r10
    0x620d0c00,  //     mov  r13, r12
    0x550a0010,  //     lsl  r10, #16
    0x418a0008,  //     setrl  r10,#8
    0x510b0010,  //     lsr  r11, #16
    0x550c0010,  //     lsl  r12, #16
    0x510d0010,  //     lsr  r13, #16
    0x10a10134,  //     ldro  r5, r1, #308
    0x0b053fff,  //     cmp  r5, #16383
    0x0d08000a,  //     ble  #10
    0x51050010,  //     lsr  r5, #16
    0x440b0500,  //     add  r11, r5
    0x620d0400,  //     mov  r13, r4
    0x510d0008,  //     lsr  r13, #8
    0x550d0008,  //     lsl  r13, #8
    0x0b047fff,  //     cmp  r4, #32767
    0x0d080002,  //     ble  #2
    0x450d0001,  //     add  r13, #1
    0x406d0000,  //     setrh  r13,#0
    0x11c00004,  //     ldro  r14, r0, #4
    0x0a020300,  //     cmp  r2, r3
    0x0d100008,  //     beq  #8
    0x108100f0,  //     ldro  r4, r1, #240
    0x440e0400,  //     add  r14, r4
    0x62050700,  //     mov  r5, r7
    0x45850001,  //     sub  r5, #1
    0x108100f4,  //     ldro  r4, r1, #244
    0x58850400,  //     mul  r5, r4
    0x440e0500,  //     add  r14, r5
    0x90012435,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over4,eac[0100],cas[00110101]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x1241003c,  //     ldro  r18, r1, #60
    0x12610040,  //     ldro  r19, r1, #64
    0x0b030001,  //     cmp  r3, #1
    0x0d100005,  //     beq  __input_h_branch1_end
    0x12610044,  //     ldro  r19, r1, #68
    0x0a020300,  //     cmp  r2, r3
    0x0d100002,  //     beq  __input_h_branch1_end
    0x1261004c,  //     ldro  r19, r1, #76
                 // __input_h_branch1_end:
    0x40730000,  //     setrh  r19,#0
    0x12810050,  //     ldro  r20, r1, #80
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0x0000
    0x96000280,  //     dstm0  slave0,mode=rotate,blk=0,precision=0, slave0,inband=64bit,outband=256bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8613f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1,grp4=1,grp5=1
    0x10810138,  //     ldro  r4, r1, #312
    0x0b040001,  //     cmp  r4, #1
    0x0d100006,  //     beq  #6
    0x10a1013c,  //     ldro  r5, r1, #316
    0x0b050001,  //     cmp  r5, #1
    0x0d100003,  //     beq  #3
    0x882ba980,  //     mnts  sel=mas0-1,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x0d30000a,  //     b  #10
    0x10a10140,  //     ldro  r5, r1, #320
    0x0b050000,  //     cmp  r5, #0
    0x0d100004,  //     beq  #4
    0x89800003,  //     mnts  sel=io-ahb,rd=3
    0x882ba983,  //     mnts  sel=mas0-1,p0-iodat=ioahb,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x0d300004,  //     b  #4
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x8920000b,  //     mnts  sel=io-rd0,inside=3,outside=prd0
    0x882ba981,  //     mnts  sel=mas0-1,p0-iodat=iord0,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x88400dc0,  //     mnts  sel=mas0-2,p1-lmdat0=grp4,p1-lmdat1=grp5
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x8a40000a,  //     mnts  sel=grp2,lmrd=m0p0_2
    0x8a60000b,  //     mnts  sel=grp3,lmrd=m0p0_3
    0x8a800010,  //     mnts  sel=grp4,lmrd=m0p1_0
    0x8aa00011,  //     mnts  sel=grp5,lmrd=m0p1_1
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x11010054,  //     ldro  r8, r1, #84
    0x11210058,  //     ldro  r9, r1, #88
    0x90050a00,  //     ares  mode=master,sel=select0,chs[0101],sel_row0,cmc[1010],ces[00000000]
    0x1101005c,  //     ldro  r8, r1, #92
    0x11210060,  //     ldro  r9, r1, #96
    0x90054a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row1,cmc[1010],ces[01010100]
    0x108100e8,  //     ldro  r4, r1, #232
    0x44840600,  //     sub  r4, r6
    0x0b040001,  //     cmp  r4, #1
    0x0d100004,  //     beq  #4
    0x11010064,  //     ldro  r8, r1, #100
    0x11210068,  //     ldro  r9, r1, #104
    0x0d300003,  //     b  #3
    0x1101006c,  //     ldro  r8, r1, #108
    0x11210070,  //     ldro  r9, r1, #112
    0x11410064,  //     ldro  r10, r1, #100
    0x11610068,  //     ldro  r11, r1, #104
    0x0b030001,  //     cmp  r3, #1
    0x0d10000b,  //     beq  __output_h_branch0_end
    0x11410074,  //     ldro  r10, r1, #116
    0x11610078,  //     ldro  r11, r1, #120
    0x0a020300,  //     cmp  r2, r3
    0x0d100007,  //     beq  __output_h_branch0_end
    0x1141007c,  //     ldro  r10, r1, #124
    0x11610080,  //     ldro  r11, r1, #128
    0x0b020001,  //     cmp  r2, #1
    0x0d100003,  //     beq  __output_h_branch0_end
    0x1141006c,  //     ldro  r10, r1, #108
    0x11610070,  //     ldro  r11, r1, #112
                 // __output_h_branch0_end:
    0x41880000,  //     setrl  r8, #0
    0x41890000,  //     setrl  r9, #0
    0x406a0000,  //     setrh  r10, #0
    0x406b0000,  //     setrh  r11, #0
    0x4e080a00,  //     orr  r8, r10
    0x4e090b00,  //     orr  r9, r11
    0x90058a54,  //     ares  mode=master,sel=select0,chs[0101],sel_row2,cmc[1010],ces[01010100]
    0x900950aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line1,elf[0000],efcs[10101010]
    0x9009d0aa,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line3,elf[0000],efcs[10101010]
    0x11410084,  //     ldro  r10, r1, #132
    0x11610088,  //     ldro  r11, r1, #136
    0x1181008c,  //     ldro  r12, r1, #140
    0x11a10090,  //     ldro  r13, r1, #144
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __output_h_branch3_end
    0x11c1014c,  //     ldro  r14, r1, #332
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __output_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __output_h_branch3_end
    0x11c10094,  //     ldro  r14, r1, #148
                 // __output_h_branch3_end:
    0x9002a055,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over10,eac[0000],cas[01010101]
    0x11410098,  //     ldro  r10, r1, #152
    0x118100a0,  //     ldro  r12, r1, #160
    0x9006a005,  //     ares  mode=master,sel=select2,sel_port0,sel_high8,sel_master_over10,eac[0000],cas[00000101]
    0x114100a8,  //     ldro  r10, r1, #168
    0x116100ac,  //     ldro  r11, r1, #172
    0x118100b0,  //     ldro  r12, r1, #176
    0x11a100b4,  //     ldro  r13, r1, #180
    0x11c100b8,  //     ldro  r14, r1, #184
    0x90082055,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[01010101]
    0x114100bc,  //     ldro  r10, r1, #188
    0x118100c4,  //     ldro  r12, r1, #196
    0x122100cc,  //     ldro  r17, r1, #204
    0x124100d0,  //     ldro  r18, r1, #208
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __output_h_branch1_end
    0x124100d4,  //     ldro  r18, r1, #212
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __output_h_branch1_end
    0x124100d8,  //     ldro  r18, r1, #216
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __output_h_branch1_end
    0x124100dc,  //     ldro  r18, r1, #220
                 // __output_h_branch1_end:
    0x900d2005,  //     ares  mode=master,sel=select2,sel_port1,sel_high8,sel_master_over4,eac[0000],cas[00000101]
    0x418f0009,  //     setrl  r15,#9
    0x90013502,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp0,dcl2
    0x40700303,  //     setrh  r16,#0x0303
    0x90057502,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line1,sel_data_sour5,sel_dp0,dcl2
    0x41900000,  //     setrl  r16,#0x0000
    0x90013581,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp1,dcl1
    0x11c0000c,  //     ldro  r14, r0, #12
    0x10810108,  //     ldro  r4, r1, #264
    0x58840600,  //     mul  r4, r6
    0x440e0400,  //     add  r14, r4
    0x110100e4,  //     ldro  r8, r1, #228
    0x12e1010c,  //     ldro  r23, r1, #268
    0x12c10110,  //     ldro  r22, r1, #272
    0x41980000,  //     setrl  r24, #0
    0x40780000,  //     setrh  r24, #0
    0x41990000,  //     setrl  r25, #0
    0x40790000,  //     setrh  r25, #0
    0x13610114,  //     ldro  r27, r1, #276
    0x419a4053,  //     setrl  r26, #0x4053
    0x10810148,  //     ldro  r4, r1, #328
    0x0b040020,  //     cmp  r4, #32
    0x0d100004,  //     beq  #4
    0x407a0802,  //     setrh  r26, #0x0802
    0x98c10210,  //     dprc   intgnet=l3,intgnet-mode=1,comshift=0,outlayer=l6,outlayer-mode=2
    0x0d300003,  //     b  #3
    0x407a0822,  //     setrh  r26, #0x0822
    0x98c10010,  //     dprc   intgnet=l3,intgnet-mode=1,comshift=0,outlayer=l6,outlayer-mode=0
    0x12800010,  //     ldro  r20, r0, #16
    0x10810100,  //     ldro  r4, r1, #256
    0x58840600,  //     mul  r4, r6
    0x44140400,  //     add  r20, r4
    0x0a020300,  //     cmp  r2, r3
    0x0d100008,  //     beq  #8
    0x108100f8,  //     ldro  r4, r1, #248
    0x44140400,  //     add  r20, r4
    0x62050700,  //     mov  r5, r7
    0x45850001,  //     sub  r5, #1
    0x108100fc,  //     ldro  r4, r1, #252
    0x58850400,  //     mul  r5, r4
    0x44140500,  //     add  r20, r5
    0x12410118,  //     ldro  r18, r1, #280
    0x0b030001,  //     cmp  r3, #1
    0x0d100008,  //     beq  __output_h_branch2_end
    0x1241011c,  //     ldro  r18, r1, #284
    0x0a020300,  //     cmp  r2, r3
    0x0d100005,  //     beq  __output_h_branch2_end
    0x12410120,  //     ldro  r18, r1, #288
    0x0b020001,  //     cmp  r2, #1
    0x0d100002,  //     beq  __output_h_branch2_end
    0x12410124,  //     ldro  r18, r1, #292
                 // __output_h_branch2_end:
    0x108100e8,  //     ldro  r4, r1, #232
    0x44840600,  //     sub  r4, r6
    0x0b040001,  //     cmp  r4, #1
    0x0d100003,  //     beq  #3
    0x12610128,  //     ldro  r19, r1, #296
    0x0d300002,  //     b  #2
    0x1261012c,  //     ldro  r19, r1, #300
    0x12a10130,  //     ldro  r21, r1, #304
    0x10810148,  //     ldro  r4, r1, #328
    0x0b040020,  //     cmp  r4, #32
    0x0d100003,  //     beq  #3
    0xa4000013,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=1,ch0=1
    0x0d300002,  //     b  #2
    0xa4000033,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=1,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x108100ec,  //     ldro  r4, r1, #236
    0x45070001,  //     add  r7, #1
    0x0a070400,  //     cmp  r7, r4
    0x0d18fefc,  //     bne  __loop_load_feature
    0x10a100e8,  //     ldro  r5, r1, #232
    0x45060001,  //     add  r6, #1
    0x0a060500,  //     cmp  r6, r5
    0x0d18febe,  //     bne  __loop_load_weight
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};
