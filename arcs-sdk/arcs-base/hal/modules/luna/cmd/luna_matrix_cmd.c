#include "luna_matrix_cmd.h"


__luna_cmd_attr__ uint32_t luna_matrix_mul_cmd[] = {
    0x62010000,  //     mov  r1, r0
    0x45010064,  //     add  r1, #100
    0x61020000,  //     mov  r2, #0
                 // __loop_load_left_matrix_start:
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x86103000,  //     memc   mode=1,access=1,grp0=1,grp1=1
    0x10e0004c,  //     ldro  r7, r0, #76
    0x0b070001,  //     cmp  r7, #1
    0x0d100003,  //     beq  __mnts_flash_1
    0x0b070000,  //     cmp  r7, #0
    0x0d100004,  //     beq  __mnts_flash_0
                 // __mnts_flash_1:
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300005,  //     b  __mnts_flash_exit
                 // __mnts_flash_0:
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x0d300001,  //     b  __mnts_flash_exit
                 // __mnts_flash_exit:
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070008,  //     cmp  r7, #8
    0x0d100005,  //     beq  __mnts_slv0_i8
    0x0b070020,  //     cmp  r7, #32
    0x0d100001,  //     beq  __mnts_slv0_i32
                 // __mnts_slv0_i32:
    0x88c00009,  //     mnts  sel=slv0,din=m0l,mode=1
    0x0d300003,  //     b  __mnts_slv0_exit
                 // __mnts_slv0_i8:
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x0d300001,  //     b  __mnts_slv0_exit
                 // __mnts_slv0_exit:
    0x12410000,  //     ldro  r18, r1, #0
    0x12610004,  //     ldro  r19, r1, #4
    0x12810008,  //     ldro  r20, r1, #8
    0x12a1000c,  //     ldro  r21, r1, #12
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070008,  //     cmp  r7, #8
    0x0d100003,  //     beq  __dstm0_slave0_i8
    0x0b070020,  //     cmp  r7, #32
    0x0d100003,  //     beq  __dstm0_slave0_i32
                 // __dstm0_slave0_i8:
    0x96000648,  //     dstm0  slave0,mode=rotate,format=1, slave0,inband=64bit, slave0,blk=1, slave0,precision=0, slave0,outband=128bit
    0x0d300002,  //     b  __dstm0_slave0_exit
                 // __dstm0_slave0_i32:
    0x96002208,  //     dstm0  slave0,mode=rotate,format=1, slave0,inband=64bit, slave0,blk=0, slave0,precision=2, slave0,outband=64bit
                 // __dstm0_slave0_exit:
    0x11010010,  //     ldro  r8, r1, #16
    0x11210014,  //     ldro  r9, r1, #20
    0x10e100b0,  //     ldro  r7, r1, #176
    0x0b070001,  //     cmp  r7,#1
    0x0d100002,  //     beq  __llm_master_s0_i_interval0_1
    0x0d300003,  //     b  __llm_master_s0_i_interval0_0
                 // __llm_master_s0_i_interval0_1:
    0x90020294,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[10010100]
    0x0d300003,  //     b  __llm_master_s0_i_interval0_exit
                 // __llm_master_s0_i_interval0_0:
    0x90020214,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00010100]
    0x0d300001,  //     b  __llm_master_s0_i_interval0_exit
                 // __llm_master_s0_i_interval0_exit:
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x11410018,  //     ldro  r10, r1, #24
    0x10e1001c,  //     ldro  r7, r1, #28
    0x626c0700,  //     movh  r12, r7
    0x11c10020,  //     ldro  r14, r1, #32
    0x10e00034,  //     ldro  r7, r0, #52
    0x58870200,  //     mul  r7, r2
    0x440e0700,  //     add  r14, r7
    0x10e100a8,  //     ldro  r7, r1, #168
    0x0b070001,  //     cmp  r7,#1
    0x0d100002,  //     beq  __master_s2_p0_i_interval0_1
    0x0d300003,  //     b  __master_s2_p0_i_interval0_0
                 // __master_s2_p0_i_interval0_1:
    0x9000e20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over3,eac[0010],cas[00001101]
    0x0d300003,  //     b  __master_s2_p0_i_interval0_exit
                 // __master_s2_p0_i_interval0_0:
    0x9000a20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0010],cas[00001101]
    0x0d300001,  //     b  __master_s2_p0_i_interval0_exit
                 // __master_s2_p0_i_interval0_exit:
    0x10e00058,  //     ldro  r7, r0, #88
    0x0b070001,  //     cmp  r7, #0x1
    0x0d100003,  //     beq  __llm_master_s2_bit4_1
    0x0b070000,  //     cmp  r7, #0x0
    0x0d100003,  //     beq  __llm_master_s2_bit4_0
                 // __llm_master_s2_bit4_1:
    0x900a2001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over8,eac[0000],cas[00000001]
    0x0d300003,  //     b  __llm_master_s2_bit4_exit
                 // __llm_master_s2_bit4_0:
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0d300001,  //     b  __llm_master_s2_bit4_exit
                 // __llm_master_s2_bit4_exit:
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x61030000,  //     mov  r3, #0
                 // __loop_load_right_matrix_start:
    0x610d0000,  //     mov  r13, #0
    0x8613c000,  //     memc   mode=1,access=1,grp2=1,grp3=1,grp4=1,grp5=1
    0x10e00050,  //     ldro  r7, r0, #80
    0x0b070001,  //     cmp  r7, #1
    0x0d100003,  //     beq  __lrm_mnts_flash_1
    0x0b070000,  //     cmp  r7, #0
    0x0d100004,  //     beq  __lrm_mnts_flash_0
                 // __lrm_mnts_flash_1:
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300005,  //     b  __lrm_mnts_flash_exit
                 // __lrm_mnts_flash_0:
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400011,  //     mnts  sel=io-rd1,inside=m0p0,outside=prd1
    0x88200002,  //     mnts  sel=mas0-1,p0-iodat=iord1
    0x0d300001,  //     b  __lrm_mnts_flash_exit
                 // __lrm_mnts_flash_exit:
    0x8a400200,  //     mnts  sel=grp2,lmwr=s0_0
    0x8a600240,  //     mnts  sel=grp3,lmwr=s0_1
    0x8a800280,  //     mnts  sel=grp4,lmwr=s0_2
    0x8aa002c0,  //     mnts  sel=grp5,lmwr=s0_3
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070008,  //     cmp  r7, #8
    0x0d100003,  //     beq  __mnts_slv0_lrm_i8
    0x0b070020,  //     cmp  r7, #32
    0x0d100003,  //     beq  __mnts_slv0_lrm_i32
                 // __mnts_slv0_lrm_i8:
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x0d300003,  //     b  __mnts_slv0_lrm_exit
                 // __mnts_slv0_lrm_i32:
    0x88c00009,  //     mnts  sel=slv0,din=m0l,mode=1
    0x0d300001,  //     b  __mnts_slv0_lrm_exit
                 // __mnts_slv0_lrm_exit:
    0x12410024,  //     ldro  r18, r1, #36
    0x12610028,  //     ldro  r19, r1, #40
    0x1281002c,  //     ldro  r20, r1, #44
    0x12a10030,  //     ldro  r21, r1, #48
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070008,  //     cmp  r7, #8
    0x0d100003,  //     beq  __dstm0_slave0_lrm_i8
    0x0b070020,  //     cmp  r7, #32
    0x0d100003,  //     beq  __dstm0_slave0_lrm_i32
                 // __dstm0_slave0_lrm_i8:
    0x96000280,  //     dstm0  slave0,mode=rotate,blk=0,precision=0, slave0,inband=64bit, slave0,blk=0, slave0,precision=0, slave0,outband=256bit
    0x0d300002,  //     b  __dstm0_slave0_lrm_exit
                 // __dstm0_slave0_lrm_i32:
    0x96002600,  //     dstm0  slave0,mode=rotate,blk=0,precision=0, slave0,inband=64bit, slave0,blk=1, slave0,precision=2, slave0,outband=64bit
                 // __dstm0_slave0_lrm_exit:
    0x11010034,  //     ldro  r8, r1, #52
    0x11210038,  //     ldro  r9, r1, #56
    0x10e100b4,  //     ldro  r7, r1, #180
    0x0b070001,  //     cmp  r7,#1
    0x0d100002,  //     beq  __lrm_master_s0_i_interval1_1
    0x0d300003,  //     b  __lrm_master_s0_i_interval1_0
                 // __lrm_master_s0_i_interval1_1:
    0x90020294,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[10010100]
    0x0d300003,  //     b  __lrm_master_s0_i_interval1_exit
                 // __lrm_master_s0_i_interval1_0:
    0x90020214,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00010100]
    0x0d300001,  //     b  __lrm_master_s0_i_interval1_exit
                 // __lrm_master_s0_i_interval1_exit:
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x1141003c,  //     ldro  r10, r1, #60
    0x10e10040,  //     ldro  r7, r1, #64
    0x626c0700,  //     movh  r12, r7
    0x11c10044,  //     ldro  r14, r1, #68
    0x10e00038,  //     ldro  r7, r0, #56
    0x58870300,  //     mul  r7, r3
    0x440e0700,  //     add  r14, r7
    0x10e100ac,  //     ldro  r7, r1, #172
    0x0b070001,  //     cmp  r7,#1
    0x0d100002,  //     beq  __master_s2_p0_i_interval1_1
    0x0d300003,  //     b  __master_s2_p0_i_interval1_0
                 // __master_s2_p0_i_interval1_1:
    0x9000e20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over3,eac[0010],cas[00001101]
    0x0d300003,  //     b  __master_s2_p0_i_interval1_exit
                 // __master_s2_p0_i_interval1_0:
    0x9000a20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0010],cas[00001101]
    0x0d300001,  //     b  __master_s2_p0_i_interval1_exit
                 // __master_s2_p0_i_interval1_exit:
    0x10e0005c,  //     ldro  r7, r0, #92
    0x0b070001,  //     cmp  r7, #0x1
    0x0d100003,  //     beq  __lrm_master_s2_bit4_1
    0x0b070000,  //     cmp  r7, #0x0
    0x0d100003,  //     beq  __lrm_master_s2_bit4_0
                 // __lrm_master_s2_bit4_1:
    0x900a2001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over8,eac[0000],cas[00000001]
    0x0d300003,  //     b  __lrm_master_s2_bit4_exit
                 // __lrm_master_s2_bit4_0:
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0d300001,  //     b  __lrm_master_s2_bit4_exit
                 // __lrm_master_s2_bit4_exit:
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8613f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1,grp4=1,grp5=1
    0x10e00060,  //     ldro  r7, r0, #96
    0x0b070001,  //     cmp  r7, #0x01
    0x0d100003,  //     beq  __cc_mnts_bias_1
    0x0b070000,  //     cmp  r7, #0x00
    0x0d100015,  //     beq  __cc_mnts_bias_0
                 // __cc_mnts_bias_1:
    0x10e00054,  //     ldro  r7, r0, #84
    0x0b070001,  //     cmp  r7, #0x01
    0x0d100003,  //     beq  __cc_mnts_bias_in_flash_1
    0x0b070000,  //     cmp  r7, #0x00
    0x0d100007,  //     beq  __cc_mnts_bias_in_flash_0
                 // __cc_mnts_bias_in_flash_1:
    0x89800003,  //     mnts  sel=io-ahb,rd=3
    0x89000012,  //     mnts  sel=pe,row=m0h,col=m0l
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x882dcba3,  //     mnts  sel=mas0-1,p0-iodat=ioahb,p0-lmdat0=grp2,p0-lmdat1=grp3,p0-lmdat2=grp4,p0-lmdat3=grp5
    0x0d300008,  //     b  __cc_mnts_bias_in_flash_exit
                 // __cc_mnts_bias_in_flash_0:
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x8920000b,  //     mnts  sel=io-rd0,inside=3,outside=prd0
    0x89000012,  //     mnts  sel=pe,row=m0h,col=m0l
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x882dcba1,  //     mnts  sel=mas0-1,p0-iodat=iord0,p0-lmdat0=grp2,p0-lmdat1=grp3,p0-lmdat2=grp4,p0-lmdat3=grp5
    0x0d300001,  //     b  __cc_mnts_bias_in_flash_exit
                 // __cc_mnts_bias_in_flash_exit:
    0x88400980,  //     mnts  sel=mas0-2,p1-lmdat0=grp0,p1-lmdat1=grp1
    0x0d300007,  //     b  __cc_mnts_bias_exit
                 // __cc_mnts_bias_0:
    0x89000012,  //     mnts  sel=pe,row=m0h,col=m0l
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x882dcba0,  //     mnts  sel=mas0-1,p0-lmdat0=grp2,p0-lmdat1=grp3,p0-lmdat2=grp4,p0-lmdat3=grp5
    0x88400980,  //     mnts  sel=mas0-2,p1-lmdat0=grp0,p1-lmdat1=grp1
    0x0d300001,  //     b  __cc_mnts_bias_exit
                 // __cc_mnts_bias_exit:
    0x8a000010,  //     mnts  sel=grp0,lmrd=m0p1_0
    0x8a200011,  //     mnts  sel=grp1,lmrd=m0p1_1
    0x8a400008,  //     mnts  sel=grp2,lmrd=m0p0_0
    0x8a600009,  //     mnts  sel=grp3,lmrd=m0p0_1
    0x8a80000a,  //     mnts  sel=grp4,lmrd=m0p0_2
    0x8aa0000b,  //     mnts  sel=grp5,lmrd=m0p0_3
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070020,  //     cmp  r7, #0x20
    0x0d180002,  //     bne  __mnts_mas0_cc_exit
    0x88000028,  //     mnts  sel=mas0-0,mode0=1,mode1=1
                 // __mnts_mas0_cc_exit:
    0x11010048,  //     ldro  r8, r1, #72
    0x1121004c,  //     ldro  r9, r1, #76
    0x90000a00,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[1010],ces[00000000]
    0x11010050,  //     ldro  r8, r1, #80
    0x11210054,  //     ldro  r9, r1, #84
    0x90004000,  //     ares  mode=master,sel=select0,chs[0000],sel_row1,cmc[0000],ces[00000000]
    0x10e00060,  //     ldro  r7, r0, #96
    0x0b070001,  //     cmp  r7, #1
    0x0d100003,  //     beq  __cc_master_select1_bias_1
    0x0b070000,  //     cmp  r7, #0
    0x0d100003,  //     beq  __cc_master_select1_bias_0
                 // __cc_master_select1_bias_1:
    0x9009100a,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour1,sel_en_line0,elf[0000],efcs[00001010]
    0x0d300003,  //     b  __cc_master_select1_bias_exit
                 // __cc_master_select1_bias_0:
    0x9008100a,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00001010]
    0x0d300001,  //     b  __cc_master_select1_bias_exit
                 // __cc_master_select1_bias_exit:
    0x11410058,  //     ldro  r10, r1, #88
    0x1161005c,  //     ldro  r11, r1, #92
    0x11810060,  //     ldro  r12, r1, #96
    0x11c10064,  //     ldro  r14, r1, #100
    0x90012015,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over4,eac[0000],cas[00010101]
    0x10e10068,  //     ldro  r7, r1, #104
    0x626a0700,  //     movh  r10, r7
    0x1161006c,  //     ldro  r11, r1, #108
    0x11810070,  //     ldro  r12, r1, #112
    0x11a10074,  //     ldro  r13, r1, #116
    0x10e00060,  //     ldro  r7, r0, #96
    0x0b070001,  //     cmp  r7, #1
    0x41862000,  //     setrl  r6,#8192
    0x40660080,  //     setrh  r6,#128
    0x66510600,  //     cmoveq  r17,r6
    0x90082015,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00010101]
    0x10e10078,  //     ldro  r7, r1, #120
    0x622f0700,  //     movl  r15, r7
    0x10e1007c,  //     ldro  r7, r1, #124
    0x62300700,  //     movl  r16, r7
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070008,  //     cmp  r7, #8
    0x0d100003,  //     beq  __master_select3_0_i8
    0x0b070020,  //     cmp  r7, #32
    0x0d100003,  //     beq  __master_select3_0_i32
                 // __master_select3_0_i8:
    0x90053502,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line0,sel_data_sour5,sel_dp0,dcl2
    0x0d300002,  //     b  __master_select3_0_exit
                 // __master_select3_0_i32:
    0x90053500,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line0,sel_data_sour5,sel_dp0,dcl0
                 // __master_select3_0_exit:
    0x10e10080,  //     ldro  r7, r1, #128
    0x622f0700,  //     movl  r15, r7
    0x10e10084,  //     ldro  r7, r1, #132
    0x62300700,  //     movl  r16, r7
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070008,  //     cmp  r7, #8
    0x0d100003,  //     beq  __master_select3_1_i8
    0x0b070020,  //     cmp  r7, #32
    0x0d100003,  //     beq  __master_select3_1_i32
                 // __master_select3_1_i8:
    0x90013581,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp1,dcl1
    0x0d300002,  //     b  __master_select3_1_exit
                 // __master_select3_1_i32:
    0x90013580,  //     ares  mode=master,sel=select3,rcils[0001],sel_counter_line0,sel_data_sour5,sel_dp1,dcl0
                 // __master_select3_1_exit:
    0x10e00060,  //     ldro  r7, r0, #96
    0x0b070001,  //     cmp  r7, #1
    0x0d180006,  //     bne  __dprc_bias_exit
                 // __dprc_bias_1:
    0x11c00044,  //     ldro  r14, r0, #68
    0x10e00048,  //     ldro  r7, r0, #72
    0x58870200,  //     mul  r7, r2
    0x440e0700,  //     add  r14, r7
    0x0d300001,  //     b  __dprc_bias_exit
                 // __dprc_bias_exit:
    0x10e10088,  //     ldro  r7, r1, #136
    0x62380700,  //     movl  r24, r7
    0x10e1008c,  //     ldro  r7, r1, #140
    0x62770700,  //     movh  r23, r7
    0x13410090,  //     ldro  r26, r1, #144
    0x13610094,  //     ldro  r27, r1, #148
    0x10e00018,  //     ldro  r7, r0, #24
    0x10c0001c,  //     ldro  r6, r0, #28
    0x55070008,  //     lsl  r7, #8
    0x44070600,  //     add  r7, r6
    0x0b072008,  //     cmp  r7, #0x2008
    0x0d100007,  //     beq  __dprc_0_i32_o8
    0x0b072020,  //     cmp  r7, #0x2020
    0x0d100008,  //     beq  __dprc_0_i32_o32
    0x0b070808,  //     cmp  r7, #0x808
    0x0d100009,  //     beq  __dprc_0_i8_o8
    0x0b070820,  //     cmp  r7, #0x820
    0x0d10000a,  //     beq  __dprc_0_i8_o32
                 // __dprc_0_i32_o8:
    0x98000001,  //     dprc   mmac0=1
    0x98e70324,  //     dprc   addtree=l3,intgnet=l5,comshift=l4,outlayer=l6,intgnet-mode=2,outlayer-mode=3,layer3-mode=1
    0x0d30000a,  //     b  __dprc_0_exit
                 // __dprc_0_i32_o32:
    0x98000001,  //     dprc   mmac0=1
    0x98e70124,  //     dprc   addtree=l3,intgnet=l5,comshift=l4,outlayer=l6,intgnet-mode=2,outlayer-mode=1,layer3-mode=1
    0x0d300007,  //     b  __dprc_0_exit
                 // __dprc_0_i8_o8:
    0x98000001,  //     dprc   mmac0=1
    0x98e10310,  //     dprc   intgnet=l3,comshift=l4,outlayer=l6,intgnet-mode=1,outlayer-mode=3
    0x0d300004,  //     b  __dprc_0_exit
                 // __dprc_0_i8_o32:
    0x98000001,  //     dprc   mmac0=1
    0x98e10110,  //     dprc   intgnet=l3,comshift=l4,outlayer=l6,intgnet-mode=1,outlayer-mode=1
    0x0d300001,  //     b  __dprc_0_exit
                 // __dprc_0_exit:
    0x12810098,  //     ldro  r20, r1, #152
    0x10e0003c,  //     ldro  r7, r0, #60
    0x58870200,  //     mul  r7, r2
    0x44140700,  //     add  r20, r7
    0x10e00040,  //     ldro  r7, r0, #64
    0x58870300,  //     mul  r7, r3
    0x44140700,  //     add  r20, r7
    0x1241009c,  //     ldro  r18, r1, #156
    0x126100a0,  //     ldro  r19, r1, #160
    0x12a100a4,  //     ldro  r21, r1, #164
    0x10e00018,  //     ldro  r7, r0, #24
    0x10c0001c,  //     ldro  r6, r0, #28
    0x55070008,  //     lsl  r7, #8
    0x44070600,  //     add  r7, r6
    0x0b072008,  //     cmp  r7, #0x2008
    0x0d100007,  //     beq  __iow_0_i32_o8
    0x0b072020,  //     cmp  r7, #0x2020
    0x0d100007,  //     beq  __iow_0_i32_o32
    0x0b070808,  //     cmp  r7, #0x808
    0x0d100007,  //     beq  __iow_0_i8_o8
    0x0b070820,  //     cmp  r7, #0x820
    0x0d100007,  //     beq  __iow_0_i8_o32
                 // __iow_0_i32_o8:
    0xa4000003,  //     iow  bw=16bit,ch3=0,ch2=0,ch1=1,ch0=1
    0x0d300007,  //     b  __iow_0_exit
                 // __iow_0_i32_o32:
    0xa4000023,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=1,ch0=1
    0x0d300005,  //     b  __iow_0_exit
                 // __iow_0_i8_o8:
    0xa4000013,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=1,ch0=1
    0x0d300003,  //     b  __iow_0_exit
                 // __iow_0_i8_o32:
    0xa4000033,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=1,ch0=1
    0x0d300001,  //     b  __iow_0_exit
                 // __iow_0_exit:
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x10e00030,  //     ldro  r7, r0, #48
    0x45030001,  //     add  r3, #1
    0x0a030700,  //     cmp  r3, r7
    0x0d20ff09,  //     blt  __loop_load_right_matrix_start
                 // __loop_load_right_matrix_exit:
    0x10e0002c,  //     ldro  r7, r0, #44
    0x45020001,  //     add  r2, #1
    0x0a020700,  //     cmp  r2, r7
    0x0d20feb9,  //     blt  __loop_load_left_matrix_start
                 // __loop_load_left_matrix_exit:
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};




__luna_cmd_attr__ uint32_t luna_matrix_transpose_cmd[] = {
    0x61030000,  //     mov  r3, #0
                 // __loop2_transpose_start:
    0x61020000,  //     mov  r2, #0
                 // __loop_transpose_start:
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x62010000,  //     mov  r1, r0
    0x45010034,  //     add  r1, #52
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x88c00011,  //     mnts  sel=slv0,din=m0l,mode=2
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x8a400280,  //     mnts  sel=grp2,lmwr=s0_2
    0x8a6002c0,  //     mnts  sel=grp3,lmwr=s0_3
    0x11010000,  //     ldro  r8, r1, #0
    0x11210004,  //     ldro  r9, r1, #4
    0x10e10044,  //     ldro  r7, r1, #68
    0x0b070001,  //     cmp  r7,#1
    0x0d100002,  //     beq  __lm_master_s0_i_interval0_1
    0x0d300003,  //     b  __lm_master_s0_i_interval0_0
                 // __lm_master_s0_i_interval0_1:
    0x90020294,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[10010100]
    0x0d300003,  //     b  __lm_master_s0_i_interval0_exit
                 // __lm_master_s0_i_interval0_0:
    0x90020214,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00010100]
    0x0d300001,  //     b  __lm_master_s0_i_interval0_exit
                 // __lm_master_s0_i_interval0_exit:
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x11410008,  //     ldro  r10, r1, #8
    0x10e1000c,  //     ldro  r7, r1, #12
    0x626c0700,  //     movh  r12, r7
    0x10e00000,  //     ldro  r7, r0, #0
    0x10c00020,  //     ldro  r6, r0, #32
    0x58860200,  //     mul  r6, r2
    0x44070600,  //     add  r7, r6
    0x10c0002c,  //     ldro  r6, r0, #44
    0x58860300,  //     mul  r6, r3
    0x44070600,  //     add  r7, r6
    0x620e0700,  //     mov  r14, r7
    0x10e00010,  //     ldro  r7,r0, #16
    0x55070001,  //     lsl  r7,#1
    0x0b073fff,  //     cmp  r7,#16383
    0x0d080002,  //     ble  __master_s2_p0_i_addr_inv_le_16383
    0x0d300003,  //     b  __master_s2_p0_i_addr_inv_other
                 // __master_s2_p0_i_addr_inv_le_16383:
    0x9000a20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0010],cas[00001101]
    0x0d300003,  //     b  __master_s2_p0_i_addr_inv_exit
                 // __master_s2_p0_i_addr_inv_other:
    0x9000a20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0010],cas[00001101]
    0x0d300001,  //     b  __master_s2_p0_i_addr_inv_exit
                 // __master_s2_p0_i_addr_inv_exit:
    0x10e00000,  //     ldro  r7, r0, #0
    0x10c00020,  //     ldro  r6, r0, #32
    0x58860200,  //     mul  r6, r2
    0x44070600,  //     add  r7, r6
    0x10c0002c,  //     ldro  r6, r0, #44
    0x58860300,  //     mul  r6, r3
    0x44070600,  //     add  r7, r6
    0x620e0700,  //     mov  r14, r7
    0x10e00010,  //     ldro  r7, r0, #16
    0x440e0700,  //     add  r14, r7
    0x10e00010,  //     ldro  r7,r0, #16
    0x55070001,  //     lsl  r7,#1
    0x0b073fff,  //     cmp  r7,#16383
    0x0d080002,  //     ble  __master_s2_p1_i_addr_inv_le_16383
    0x0d300003,  //     b  __master_s2_p1_i_addr_inv_other
                 // __master_s2_p1_i_addr_inv_le_16383:
    0x9008220d,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0010],cas[00001101]
    0x0d300003,  //     b  __master_s2_p1_i_addr_inv_exit
                 // __master_s2_p1_i_addr_inv_other:
    0x9008220d,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0010],cas[00001101]
    0x0d300001,  //     b  __master_s2_p1_i_addr_inv_exit
                 // __master_s2_p1_i_addr_inv_exit:
    0x11e10018,  //     ldro  r15, r1, #24
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x1241001c,  //     ldro  r18, r1, #28
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x12810020,  //     ldro  r20, r1, #32
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070008,  //     cmp  r7, #8
    0x0d100003,  //     beq  __slave0_mode_i8
    0x0b070020,  //     cmp  r7, #32
    0x0d100003,  //     beq  __slave0_mode_i32
                 // __slave0_mode_i8:
    0x96000250,  //     dstm0  slave0,inband=128bit, slave0,outband=128bit, slave0,blk=0, slave0,mode=rotate, slave0,precision=0
    0x0d300002,  //     b  __slave0_mode_exit
                 // __slave0_mode_i32:
    0x96002050,  //     dstm0  slave0,inband=128bit, slave0,outband=128bit, slave0,blk=0, slave0,mode=normal, slave0,precision=2
                 // __slave0_mode_exit:
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x88000010,  //     mnts  sel=mas0-0,mode=2
    0x882ba980,  //     mnts  sel=mas0-1,p0-lmdat0=grp0,p0-lmdat1=grp1,p0-lmdat2=grp2,p0-lmdat3=grp3
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x8a40000a,  //     mnts  sel=grp2,lmrd=m0p0_2
    0x8a60000b,  //     mnts  sel=grp3,lmrd=m0p0_3
    0x89600001,  //     mnts  sel=io-wr,din=m0l
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x11010024,  //     ldro  r8, r1, #36
    0x11210028,  //     ldro  r9, r1, #40
    0x90000200,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0010],ces[00000000]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x10e00018,  //     ldro  r7, r0, #24
    0x0b070008,  //     cmp  r7, #8
    0x0d100003,  //     beq  __master_select2_i8
    0x0b070020,  //     cmp  r7, #32
    0x0d10000b,  //     beq  __master_select2_i32
                 // __master_select2_i8:
    0x1141002c,  //     ldro  r10, r1, #44
    0x10e10030,  //     ldro  r7, r1, #48
    0x626c0700,  //     movh  r12, r7
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x9000a005,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0000],cas[00000101]
    0x418f0007,  //     setrl  r15,#7
    0x41900300,  //     setrl  r16,#768
    0x90053501,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line0,sel_data_sour5,sel_dp0,dcl1
    0x0d300008,  //     b  __master_select2_exit
                 // __master_select2_i32:
    0x1141002c,  //     ldro  r10, r1, #44
    0x11810030,  //     ldro  r12, r1, #48
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x9000a005,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0000],cas[00000101]
    0x418f0000,  //     setrl  r15,#0
    0x90053501,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line0,sel_data_sour5,sel_dp0,dcl1
                 // __master_select2_exit:
    0x10e00004,  //     ldro  r7, r0, #4
    0x10c00024,  //     ldro  r6, r0, #36
    0x58860200,  //     mul  r6, r2
    0x44070600,  //     add  r7, r6
    0x10c00030,  //     ldro  r6, r0, #48
    0x58860300,  //     mul  r6, r3
    0x44070600,  //     add  r7, r6
    0x62140700,  //     mov  r20, r7
    0x12410038,  //     ldro  r18, r1, #56
    0x1261003c,  //     ldro  r19, r1, #60
    0x12a10040,  //     ldro  r21, r1, #64
    0xa4000023,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=1,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x10e0001c,  //     ldro  r7, r0, #28
    0x45020001,  //     add  r2, #1
    0x0a020700,  //     cmp  r2, r7
    0x0d20ff7a,  //     blt  __loop_transpose_start
                 // __loop_transpose_exit:
    0x10e00028,  //     ldro  r7, r0, #40
    0x45030001,  //     add  r3, #1
    0x0a030700,  //     cmp  r3, r7
    0x0d20ff75,  //     blt  __loop2_transpose_start
                 // __loop2_transpose_exit:
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};


__luna_cmd_attr__ uint32_t luna_matrix_transpose_3d_wch_cmd[] = {
    0x62010000,  //     mov  r1, r0
    0x45010028,  //     add  r1, #40
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x12c10000,  //     ldro  r22, r1, #0
    0x10a10004,  //     ldro  r5, r1, #4
    0x12e10008,  //     ldro  r23, r1, #8
    0x1301000c,  //     ldro  r24, r1, #12
    0x10c10010,  //     ldro  r6, r1, #16
    0x13410014,  //     ldro  r26, r1, #20
                 // __loop_start:
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x89600001,  //     mnts  sel=io-wr,din=m0l
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x11010018,  //     ldro  r8,r1,#24
    0x1121001c,  //     ldro  r9,r1,#28
    0x10e10040,  //     ldro  r7, r1, #64
    0x0b070001,  //     cmp  r7,#1
    0x0d100002,  //     beq  __lm_master_s0_i_interval0_1
    0x0d300003,  //     b  __lm_master_s0_i_interval0_0
                 // __lm_master_s0_i_interval0_1:
    0x90000294,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0010],ces[10010100]
    0x0d300003,  //     b  __lm_master_s0_i_interval0_exit
                 // __lm_master_s0_i_interval0_0:
    0x90000214,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0010],ces[00010100]
    0x0d300001,  //     b  __lm_master_s0_i_interval0_exit
                 // __lm_master_s0_i_interval0_exit:
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x11410020,  //     ldro  r10,r1,#32
    0x11810024,  //     ldro  r12,r1,#36
    0x620e1600,  //     mov  r14,r22
    0x41920000,  //     setrl  r18,#0
    0x9000a20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0010],cas[00001101]
    0x418f0000,  //     setrl  r15,#0
    0x90003102,  //     ares  mode=master,sel=select3,sel_data_sour1,sel_dp0,dcl2
    0x62141800,  //     mov  r20,r24
    0x12410034,  //     ldro  r18,r1,#52
    0x12610038,  //     ldro  r19,r1,#56
    0x12a1003c,  //     ldro  r21,r1,#60
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x0b1a0001,  //     cmp  r26,#1
    0x0d100006,  //     beq  __loop_end
    0x44161700,  //     add  r22,r23
    0x44051700,  //     add  r5,r23
    0x44180600,  //     add  r24,r6
    0x459a0001,  //     sub  r26,#1
    0x0d30ffd8,  //     b  __loop_start
                 // __loop_end:
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};


