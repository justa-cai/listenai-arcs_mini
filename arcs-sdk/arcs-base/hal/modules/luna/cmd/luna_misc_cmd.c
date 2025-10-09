#include "luna_misc_cmd.h"

__luna_cmd_attr__ uint32_t luna_api_memset[] = {
     0x288000e0,  //     ldm  r0, {r0-r4}
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x09040010,  //     cmp  r4, #16
    0x0d10000a,  //     beq  __luna_api_memset_inbit_16bit
    0x09040020,  //     cmp  r4, #32
    0x0d10000e,  //     beq  __luna_api_memset_inbit_32bit
                 // __luna_api_memset_inbit_8bit:
    0x62050300,  //     mov  r5, r3
    0x55050008,  //     lsl  r5, #8
    0x4e050300,  //     orr  r5, r3
    0x62080500,  //     mov  r8, r5
    0x55080010,  //     lsl  r8, #16
    0x4e080500,  //     orr  r8, r5
    0x0d300009,  //     b  __luna_api_memset_inbit_exit
                 // __luna_api_memset_inbit_16bit:
    0x55020001,  //     lsl  r2, #1
    0x62050300,  //     mov  r5, r3
    0x55050010,  //     lsl  r5, #16
    0x4e050300,  //     orr  r5, r3
    0x62080500,  //     mov  r8, r5
    0x0d300003,  //     b  __luna_api_memset_inbit_exit
                 // __luna_api_memset_inbit_32bit:
    0x55020002,  //     lsl  r2, #2
    0x62080300,  //     mov  r8, r3
                 // __luna_api_memset_inbit_exit:
    0x88200006,  //     mnts   sel=mas0-1,p0-iodat=r8
    0x89600001,  //     mnts   sel=io-wr,din=m0l
    0x89e00003,  //     mnts   sel=p-wr,wr=iowr
    0x62040200,  //     mov  r4, r2
    0x45040007,  //     add  r4, #7
    0x51040003,  //     lsr  r4, #3
    0x62050400,  //     mov  r5, r4
    0x51050008,  //     lsr  r5, #8
    0x4b0400ff,  //     and  r4, #0xff
    0x62080400,  //     mov  r8, r4
    0x62090500,  //     mov  r9, r5
    0x418a0008,  //     setrl  r10,#8
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x62140100,  //     mov  r20, r1
    0x62120200,  //     mov  r18, r2
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
 };

__luna_cmd_attr__ uint32_t luna_api_memcpy[] = {
	0x288000f8,  //     ldm  r0, {r0-r2}
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x89a00001,  //     mnts   sel=p-rd0,rd=iord0
    0x89200009,  //     mnts   sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts   sel=mas0-1,p0-iodat=iord0
    0x89600001,  //     mnts   sel=io-wr,din=m0l
    0x89e00003,  //     mnts   sel=p-wr,wr=iowr
    0x62040200,  //     mov  r4, r2
    0x45040007,  //     add  r4, #7
    0x51040003,  //     lsr  r4, #3
    0x62050400,  //     mov  r5, r4
    0x51050008,  //     lsr  r5, #8
    0x4b0400ff,  //     and  r4, #0xff
    0x62080400,  //     mov  r8, r4
    0x62090500,  //     mov  r9, r5
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x62140100,  //     mov  r20, r1
    0x62120200,  //     mov  r18, r2
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};

__luna_cmd_attr__ uint32_t luna_api_activate[] = {
    0x288000c0,  //     ldm  r0, {r0-r5}
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=iord0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x62070500,  //     mov  r7, r5
    0x4b0700ff,  //     and  r7, #0xff
    0x09070008,  //     cmp  r7, #8
    0x0d100005,  //     beq   __luna_api_activate_mnts_8bit
    0x09070010,  //     cmp  r7, #16
    0x0d100006,  //     beq   __luna_api_activate_mnts_16bit
    0x09070020,  //     cmp  r7, #32
    0x0d100007,  //     beq   __luna_api_activate_mnts_32bit
                 // __luna_api_activate_mnts_8bit:
    0x41880101,  //     setrl  r8, #0x0101
    0x40680101,  //     setrh  r8, #0x0101
    0x0d300006,  //     b   __luna_api_activate_mnts_exit
                 // __luna_api_activate_mnts_16bit:
    0x41880001,  //     setrl  r8, #0x0001
    0x40680001,  //     setrh  r8, #0x0001
    0x0d300003,  //     b   __luna_api_activate_mnts_exit
                 // __luna_api_activate_mnts_32bit:
    0x41880001,  //     setrl  r8, #0x0001
    0x40680000,  //     setrh  r8, #0x0000
                 // __luna_api_activate_mnts_exit:
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x62070200,  //     mov  r7, r2
    0x62060500,  //     mov  r6, r5
    0x4b0600ff,  //     and  r6, #0xff
    0x58870600,  //     mul  r7, r6
    0x4507003f,  //     add  r7, #63
    0x51070006,  //     lsr  r7, #6
    0x62060500,  //     mov  r6, r5
    0x4b0600ff,  //     and  r6, #0xff
    0x4b060008,  //     and  r6, #8
    0x51060003,  //     lsr  r6, #3
    0x620a0600,  //     mov  r10, r6
    0x4b0a00ff,  //     and  r10, #0xff
    0x550a0008,  //     lsl  r10, #8
    0x62080700,  //     mov  r8, r7
    0x4b0800ff,  //     and  r8, #0xff
    0x4e080a00,  //     orr  r8, r10
    0x620a0600,  //     mov  r10, r6
    0x510a0008,  //     lsr  r10, #8
    0x550a0008,  //     lsl  r10, #8
    0x62090700,  //     mov  r9, r7
    0x51090008,  //     lsr  r9, #8
    0x4e090a00,  //     orr  r9, r10
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14,r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62070400,  //     mov  r7, r4
    0x55070008,  //     lsl  r7, #8
    0x61170001,  //     mov  r23, #1
    0x5517000f,  //     lsl  r23, #15
    0x44170700,  //     add  r23, r7
    0x44170300,  //     add  r23, r3
    0x55170010,  //     lsl  r23, #16
    0x09050808,  //     cmp  r5, #0x0808
    0x0d100007,  //     beq  __luna_api_activate_master_i8_o8
    0x09052008,  //     cmp  r5, #0x2008
    0x0d10000c,  //     beq  __luna_api_activate_master_i8_o32
    0x09050820,  //     cmp  r5, #0x0820
    0x0d100011,  //     beq  __luna_api_activate_master_i32_o8
    0x09052020,  //     cmp  r5, #0x2020
    0x0d100016,  //     beq  __luna_api_activate_master_i32_o32
                 // __luna_api_activate_master_i8_o8:
    0x41987700,  //     setrl  r24, #0x7700
    0x41990000,  //     setrl  r25, #0x0000
    0x419a4044,  //     setrl  r26, #0x4044
    0x407a0402,  //     setrh  r26, #0x0402
    0x98000001,  //     dprc   mmac0=1
    0x98e1031c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=3,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d300015,  //     b  __luna_api_activate_master_exit
                 // __luna_api_activate_master_i8_o32:
    0x41987700,  //     setrl  r24, #0x7700
    0x41990000,  //     setrl  r25, #0x0000
    0x419a4044,  //     setrl  r26, #0x4044
    0x407a0422,  //     setrh  r26, #0x0422
    0x98000001,  //     dprc   mmac0=1
    0x98e1011c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d30000e,  //     b  __luna_api_activate_master_exit
                 // __luna_api_activate_master_i32_o8:
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0003,  //     setrh  r26, #0x0003
    0x98000001,  //     dprc   mmac0=1
    0x98f40304,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3
    0x0d300007,  //     b  __luna_api_activate_master_exit
                 // __luna_api_activate_master_i32_o32:
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
                 // __luna_api_activate_master_exit:
    0x62140100,  //     mov  r20,r1
    0x62060500,  //     mov  r6, r5
    0x51060008,  //     lsr  r6, #8
    0x62120200,  //     mov  r18, r2
		//0x0cf01e00,  //     bl    lr  ok
    0x58920600,  //     mul  r18, r6
	//0x55120003,  //     lsl  r18,#3
	//0x55120005,  //     lsl  r18,#5
		//0x0cf01e00,  //     bl    lr err
    0x51120003,  //     lsr  r18, #3
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x09050808,  //     cmp  r5, #0x0808
    0x0d100007,  //     beq  __luna_api_activate_iow_i8_o8
    0x09052008,  //     cmp  r5, #0x2008
    0x0d100007,  //     beq  __luna_api_activate_iow_i8_o32
    0x09050820,  //     cmp  r5, #0x0820
    0x0d100007,  //     beq  __luna_api_activate_iow_i32_o8
    0x09052020,  //     cmp  r5, #0x2020
    0x0d100007,  //     beq  __luna_api_activate_iow_i32_o32
                 // __luna_api_activate_iow_i8_o8:
    0xa4000011,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300006,  //     b  __luna_api_activate_iow_exit
                 // __luna_api_activate_iow_i8_o32:
    0xa4000031,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300004,  //     b  __luna_api_activate_iow_exit
                 // __luna_api_activate_iow_i32_o8:
    0xa4000001,  //     iow  bw=16bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  __luna_api_activate_iow_exit
                 // __luna_api_activate_iow_i32_o32:
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
                 // __luna_api_activate_iow_exit:
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};

__luna_cmd_attr__ uint32_t luna_api_lut[] = {
    0x288000c0,  //     ldm  r0, {r0-r5}
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x0b050000,  //     cmp  r5,#0
    0x0d100002,  //     beq  __lut_src_sel_1
    0x0d300005,  //     b  __lut_src_sel_0
                 // __lut_src_sel_1:
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x0d300004,  //     b  __lut_src_sel_exit
                 // __lut_src_sel_0:
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300001,  //     b  __lut_src_sel_exit
                 // __lut_src_sel_exit:
    0x62070300,  //     mov  r7, r3
    0x0b070000,  //     cmp  r7, #0
    0x0d100003,  //     beq  __luna_api_lut_cmd_mnts_sigmoid
    0x0b070001,  //     cmp  r7, #1
    0x0d100004,  //     beq  __luna_api_lut_cmd_mnts_tanh
                 // __luna_api_lut_cmd_mnts_sigmoid:
    0x8b200001,  //     mnts  sel=sigmoid,din=m0l
    0x89000088,  //     mnts  sel=pe,row=sigmoid,col=sigmoid
    0x0d300003,  //     b  __luna_api_lut_cmd_mnts_exit
                 // __luna_api_lut_cmd_mnts_tanh:
    0x8b400001,  //     mnts  sel=tanh,din=m0l
    0x89000099,  //     mnts  sel=pe,row=tanh,col=tanh
                 // __luna_api_lut_cmd_mnts_exit:
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x62070200,  //     mov  r7, r2
    0x55070005,  //     lsl  r7, #5
    0x4507003f,  //     add  r7, #63
    0x51070006,  //     lsr  r7, #6
    0x62060700,  //     mov  r6, r7
    0x4b0600ff,  //     and  r6, #0xff
    0x62280600,  //     movl  r8, r6
    0x62060700,  //     mov  r6, r7
    0x51060008,  //     lsr  r6, #8
    0x62290600,  //     movl  r9, r6
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x62070400,  //     mov  r7,r4
    0x51070008,  //     lsr  r7,#8
    0x0b070008,  //     cmp  r7,#8
    0x0d100003,  //     beq  __luna_api_lut_cmd_pe_iow_o8
    0x0b070020,  //     cmp  r7,#32
    0x0d10000f,  //     beq  __luna_api_lut_cmd_pe_iow_o32
                 // __luna_api_lut_cmd_pe_iow_o8:
    0x40770018,  //     setrh  r23, #24
    0x41982212,  //     setrl  r24, #0x2212
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0103,  //     setrh  r26, #0x0103
    0x98000001,  //     dprc   mmac0=1
    0x98f40304,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3
    0x62140100,  //     mov  r20, r1
    0x62070200,  //     mov  r7, r2
    0x62120700,  //     mov  r18, r7
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000001,  //     iow  bw=16bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300010,  //     b  __luna_api_lut_cmd_pe_iow_exit
                 // __luna_api_lut_cmd_pe_iow_o32:
    0x40770000,  //     setrh  r23, #0
    0x41982212,  //     setrl  r24, #0x2212
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0123,  //     setrh  r26, #0x0123
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62140100,  //     mov  r20, r1
    0x62070200,  //     mov  r7, r2
    0x55070002,  //     lsl  r7, #2
    0x62120700,  //     mov  r18, r7
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300001,  //     b  __luna_api_lut_cmd_pe_iow_exit
                 // __luna_api_lut_cmd_pe_iow_exit:
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};

__luna_cmd_attr__ uint32_t luna_api_exp[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x62070000,  //     mov  r7, r0
    0x4507000c,  //     add  r7, #12
    0x8613f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1,grp4=1,grp5=1
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00009,  //     mnts  sel=slv0,din=m0l,mode=1
    0x8a800200,  //     mnts  sel=grp4,lmwr=s0_0
    0x8aa00240,  //     mnts  sel=grp5,lmwr=s0_1
    0x10c70000,  //     ldro  r6, r7, #0
    0x62280600,  //     movl  r8, r6
    0x10c70004,  //     ldro  r6, r7, #4
    0x62290600,  //     movl  r9, r6
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x418a0008,  //     setrl  r10,#8
    0x418b0000,  //     setrl  r11,#0
    0x11c70008,  //     ldro  r14, r7, #8
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x10c00008,  //     ldro  r6,r0,#8
    0x62320600,  //     movl  r18,r6
    0x40720000,  //     setrh  r18,#0
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x611b0005,  //     mov   r27,#5
    0x61361000,  //     movl  r22,#0x1000
    0x61760017,  //     movh  r22,#23
                 // __loop_vec_exp:
    0x88000028,  //     mnts  sel=mas0-0,mode0=1,mode1=1
    0x88200dc0,  //     mnts  sel=mas0-1,p0-lmdat0=grp4,p0-lmdat1=grp5
    0x8a800008,  //     mnts  sel=grp4,lmrd=m0p0_0
    0x8aa00009,  //     mnts  sel=grp5,lmrd=m0p0_1
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x0b1b0005,  //     cmp  r27,#5
    0x0d10000a,  //     beq  __exp_1st_mnt
    0x0b1b0004,  //     cmp  r27,#4
    0x0d10000c,  //     beq  __exp_2nd_4th_mnt
    0x0b1b0002,  //     cmp  r27,#2
    0x0d10000a,  //     beq  __exp_2nd_4th_mnt
    0x88400ba0,  //     mnts  sel=mas0-2,p1-lmdat0=grp2,p1-lmdat1=grp3
    0x8a400010,  //     mnts  sel=grp2,lmrd=m0p1_0
    0x8a600011,  //     mnts  sel=grp3,lmrd=m0p1_1
    0x0b1b0001,  //     cmp  r27,#1
    0x0d10000c,  //     beq  __exp_5th_mnt
                 // __exp_1st_mnt:
    0x88c0000f,  //     mnts  sel=slv0,din=pe,mode=1
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x0d30000a,  //     b  __exp_cal
                 // __exp_2nd_4th_mnt:
    0x88400980,  //     mnts  sel=mas0-2,p1-lmdat0=grp0,p1-lmdat1=grp1
    0x8a000010,  //     mnts  sel=grp0,lmrd=m0p1_0
    0x8a200011,  //     mnts  sel=grp1,lmrd=m0p1_1
    0x88c0000f,  //     mnts  sel=slv0,din=pe,mode=1
    0x8a400200,  //     mnts  sel=grp2,lmwr=s0_0
    0x8a600240,  //     mnts  sel=grp3,lmwr=s0_1
    0x0d300003,  //     b  __exp_cal
                 // __exp_5th_mnt:
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
                 // __exp_cal:
    0x10c70000,  //     ldro  r6, r7, #0
    0x62280600,  //     movl  r8, r6
    0x10c70004,  //     ldro  r6, r7, #4
    0x62290600,  //     movl  r9, r6
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003500,  //     ares  mode=master,sel=select3,sel_data_sour5
    0x62171600,  //     mov  r23, r22
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac222,  //     setrl  r26, #0xc222
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x0b1b0001,  //     cmp  r27,#1
    0x0d10000e,  //     beq  __wait_for_iow
    0x10c00008,  //     ldro  r6,r0,#8
    0x62320600,  //     movl  r18,r6
    0x40720000,  //     setrh  r18,#0
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x0d300009,  //     b  __wait_end
                 // __wait_for_iow:
    0x12870014,  //     ldro  r20, r7, #20
    0x12470018,  //     ldro  r18, r7, #24
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
                 // __wait_end:
    0x0b1b0001,  //     cmp  r27,#1
    0x0d100004,  //     beq  __loop_end
    0x45160100,  //     add  r22,#0x100
    0x459b0001,  //     sub  r27,#1
    0x0d30ffb5,  //     b  __loop_vec_exp
                 // __loop_end:
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};



__luna_cmd_attr__ uint32_t luna_api_softmax[] = {
    0x62040000,  //     mov  r4,r0
    0x62050400,  //     mov  r5,r4
    0x45050024,  //     add  r5, #36
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x10e4003c,  //     ldro  r7, r4, #60
    0x0b070000,  //     cmp  r7,#0
    0x0d100002,  //     beq  __maxmin_src_sel_1
    0x0d300005,  //     b  __maxmin_src_sel_0
                 // __maxmin_src_sel_1:
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x0d300004,  //     b  __maxmin_src_sel_exit
                 // __maxmin_src_sel_0:
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300001,  //     b  __maxmin_src_sel_exit
                 // __maxmin_src_sel_exit:
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x10e40018,  //     ldro  r7, r4, #24
    0x62280700,  //     movl  r8, r7
    0x10e4001c,  //     ldro  r7, r4, #28
    0x62290700,  //     movl  r9, r7
    0x418a0008,  //     setrl  r10,#8
    0x11c40000,  //     ldro  r14,r4,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x41880000,  //     setrl  r8,  #0x0000
    0x40688000,  //     setrh  r8,  #0x8000
    0x10e40014,  //     ldro  r7,r4,#20
    0x45070008,  //     add  r7,#8
    0x5507000c,  //     lsl  r7,#12
    0x622f0700,  //     movl  r15,r7
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x40770000,  //     setrh  r23, #0x0
    0x41970000,  //     setrl  r23, #0
    0x41983343,  //     setrl  r24, #0x3343
    0x40780003,  //     setrh  r24, #0x0003
    0x13450000,  //     ldro  r26,r5,#0
    0x13650004,  //     ldro  r27,r5,#4
    0x98000001,  //     dprc   mmac0=1
    0x9881005c,  //     dprc   layer3-mode=3,intgnet=l3,intgnet-mode=5,outlayer=l4
    0x12840004,  //     ldro  r20,r4,#4
    0x41920008,  //     setrl  r18,#8
    0x40720000,  //     setrh  r18,#0
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x8613f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1,grp4=1,grp5=1
    0x10e4003c,  //     ldro  r7, r4, #60
    0x0b070001,  //     cmp  r7,#1
    0x0d100002,  //     beq  __offset_src_sel_1
    0x0d300004,  //     b  __offset_src_sel_0
                 // __offset_src_sel_1:
    0x89800001,  //     mnts  sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts  sel=mas0-1,p0-iodat=ioahb
    0x0d300005,  //     b  __offset_src_sel_exit
                 // __offset_src_sel_0:
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x0d300001,  //     b  __offset_src_sel_exit
                 // __offset_src_sel_exit:
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x88c0000f,  //     mnts  sel=slv0,din=pe,mode=1
    0x8a800200,  //     mnts  sel=grp4,lmwr=s0_0
    0x8aa00240,  //     mnts  sel=grp5,lmwr=s0_1
    0x11040004,  //     ldro  r8,r4,#4
    0x45282005,  //     addm   r8, #0x2005
    0x28040800,  //     ldr    r8, [r8]
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x10e40018,  //     ldro  r7, r4, #24
    0x62280700,  //     movl  r8, r7
    0x10e4001c,  //     ldro  r7, r4, #28
    0x62290700,  //     movl  r9, r7
    0x418a0008,  //     setrl  r10,#8
    0x11c40000,  //     ldro  r14,r4,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418f0012,  //     setrl  r15,#18
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x40770000,  //     setrh  r23, #0
    0x41970000,  //     setrl  r23, #0
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac122,  //     setrl  r26, #0xc122
    0x407a0123,  //     setrh  r26, #0x0123
    0x41880001,  //     setrl  r8 , #0x0001
    0x40680000,  //     setrh  r8 , #0x0000
    0x4189ffff,  //     setrl  r9 , #0xffff
    0x4069ffff,  //     setrh  r9 , #0xffff
    0x620a0800,  //     mov    r10,r8
    0x620b0900,  //     mov    r11,r9
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x12440008,  //     ldro  r18,r4,#8
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x611b0005,  //     mov   r27,#5
    0x61361000,  //     movl  r22,#0x1000
    0x61760017,  //     movh  r22,#23
                 // __loop_vec_exp:
    0x88000028,  //     mnts  sel=mas0-0,mode0=1,mode1=1
    0x88200dc0,  //     mnts  sel=mas0-1,p0-lmdat0=grp4,p0-lmdat1=grp5
    0x8a800008,  //     mnts  sel=grp4,lmrd=m0p0_0
    0x8aa00009,  //     mnts  sel=grp5,lmrd=m0p0_1
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x0b1b0005,  //     cmp  r27,#5
    0x0d10000a,  //     beq  __exp_1st_mnt
    0x0b1b0004,  //     cmp  r27,#4
    0x0d10000c,  //     beq  __exp_2nd_4th_mnt
    0x0b1b0002,  //     cmp  r27,#2
    0x0d10000a,  //     beq  __exp_2nd_4th_mnt
    0x88400ba0,  //     mnts  sel=mas0-2,p1-lmdat0=grp2,p1-lmdat1=grp3
    0x8a400010,  //     mnts  sel=grp2,lmrd=m0p1_0
    0x8a600011,  //     mnts  sel=grp3,lmrd=m0p1_1
    0x0b1b0001,  //     cmp  r27,#1
    0x0d10000c,  //     beq  __exp_5th_mnt
                 // __exp_1st_mnt:
    0x88c0000f,  //     mnts  sel=slv0,din=pe,mode=1
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x0d30000a,  //     b  __exp_cal
                 // __exp_2nd_4th_mnt:
    0x88400980,  //     mnts  sel=mas0-2,p1-lmdat0=grp0,p1-lmdat1=grp1
    0x8a000010,  //     mnts  sel=grp0,lmrd=m0p1_0
    0x8a200011,  //     mnts  sel=grp1,lmrd=m0p1_1
    0x88c0000f,  //     mnts  sel=slv0,din=pe,mode=1
    0x8a400200,  //     mnts  sel=grp2,lmwr=s0_0
    0x8a600240,  //     mnts  sel=grp3,lmwr=s0_1
    0x0d300003,  //     b  __exp_cal
                 // __exp_5th_mnt:
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
                 // __exp_cal:
    0x10e40018,  //     ldro  r7, r4, #24
    0x62280700,  //     movl  r8, r7
    0x10e4001c,  //     ldro  r7, r4, #28
    0x62290700,  //     movl  r9, r7
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003500,  //     ares  mode=master,sel=select3,sel_data_sour5
    0x62171600,  //     mov  r23, r22
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac222,  //     setrl  r26, #0xc222
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x0b1b0001,  //     cmp  r27,#1
    0x0d10000c,  //     beq  __wait_for_iow
    0x12440008,  //     ldro  r18,r4,#8
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x0d300009,  //     b  __wait_end
                 // __wait_for_iow:
    0x12840004,  //     ldro  r20,r4,#4
    0x12440020,  //     ldro  r18,r4,#32
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
                 // __wait_end:
    0x0b1b0001,  //     cmp  r27,#1
    0x0d100004,  //     beq  __exp_loop_end
    0x45160100,  //     add  r22,#0x100
    0x459b0001,  //     sub  r27,#1
    0x0d30ffb7,  //     b  __loop_vec_exp
                 // __exp_loop_end:
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x41880001,  //     setrl  r8, #0x1
    0x40680000,  //     setrh  r8, #0x0
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x10e40018,  //     ldro  r7, r4, #24
    0x62280700,  //     movl  r8, r7
    0x10e4001c,  //     ldro  r7, r4, #28
    0x62290700,  //     movl  r9, r7
    0x418a0008,  //     setrl  r10,#8
    0x11c40004,  //     ldro  r14,r4,#4
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x10e50008,  //     ldro  r7,r5,#8
    0x622f0700,  //     movl  r15,r7
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x40770000,  //     setrh  r23, #0
    0x41970000,  //     setrl  r23, #0
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0123,  //     setrh  r26, #0x0123
    0x1365000c,  //     ldro  r27,r5,#12
    0x98000001,  //     dprc   mmac0=1
    0x98e70124,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3,intgnet=l5,intgnet-mode=2
    0x12840004,  //     ldro  r20,r4,#4
    0x41920004,  //     setrl  r18,#4
    0x40720000,  //     setrh  r18,#0
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x10e40004,  //     ldro  r7,r4,#4
    0x45272005,  //     addm   r7, #0x2005
    0x28030700,  //     ldr  r6,[r7]
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x280a8700,  //     ldr  r21,[r7]
    0x2c030700,  //     str  r6,[r7]
    0x61020000,  //     mov  r2,#0
    0x61200000,  //     movl  r0,#0x00
    0x61600080,  //     movh  r0,#0x80
    0x6101001e,  //     mov  r1,#30
    0x61030001,  //     mov  r3,#1
    0x0d30000b,  //     b  __loop_main
                 // __loop_r0ger21:
    0x45020001,  //     add  r2,#1
    0x54020300,  //     lsl  r2,r3
    0x44801500,  //     sub  r0,r21
    0x54000300,  //     lsl  r0,r3
    0x45810001,  //     sub  r1,#1
    0x0d300005,  //     b  __loop_main
                 // __loop_r0ltr21:
    0x54020300,  //     lsl  r2,r3
    0x54000300,  //     lsl  r0,r3
    0x45810001,  //     sub  r1,#1
    0x0d300001,  //     b  __loop_main
                 // __loop_main:
    0x0b010000,  //     cmp  r1,#0
    0x0d100005,  //     beq  __div_loop_end
    0x0a001500,  //     cmp  r0,r21
    0x0d28fff3,  //     bge  __loop_r0ger21
    0x0a001500,  //     cmp  r0,r21
    0x0d20fff7,  //     blt  __loop_r0ltr21
                 // __div_loop_end:
    0x62080200,  //     mov  r8,r2
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x10e40018,  //     ldro  r7, r4, #24
    0x62280700,  //     movl  r8, r7
    0x10e4001c,  //     ldro  r7, r4, #28
    0x62290700,  //     movl  r9, r7
    0x418a0008,  //     setrl  r10,#8
    0x11c40004,  //     ldro  r14,r4,#4
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x10e50010,  //     ldro  r7,r5,#16
    0x622f0700,  //     movl  r15,r7
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x10e50014,  //     ldro  r7,r5,#20
    0x62770700,  //     movh  r23,r7
    0x41970000,  //     setrl  r23, #0
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x12840004,  //     ldro  r20,r4,#4
    0x12440020,  //     ldro  r18,r4,#32
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};


__luna_cmd_attr__ uint32_t luna_api_activate_relux[] = {
    0x288000c0,  //     ldm  r0, {r0-r5}
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x62070500,  //     mov  r7, r5
    0x4b0700ff,  //     and  r7, #0xff
    0x0b070008,  //     cmp  r7, #8
    0x0d100005,  //     beq   __luna_api_activate_relux_cmd_mnts_8bit
    0x0b070010,  //     cmp  r7, #16
    0x0d100006,  //     beq   __luna_api_activate_relux_cmd_mnts_16bit
    0x0b070020,  //     cmp  r7, #32
    0x0d100007,  //     beq   __luna_api_activate_relux_cmd_mnts_32bit
                 // __luna_api_activate_relux_cmd_mnts_8bit:
    0x41880101,  //     setrl  r8, #0x0101
    0x40680101,  //     setrh  r8, #0x0101
    0x0d300006,  //     b   __luna_api_activate_relux_cmd_mnts_exit
                 // __luna_api_activate_relux_cmd_mnts_16bit:
    0x41880001,  //     setrl  r8, #0x0001
    0x40680001,  //     setrh  r8, #0x0001
    0x0d300003,  //     b   __luna_api_activate_relux_cmd_mnts_exit
                 // __luna_api_activate_relux_cmd_mnts_32bit:
    0x41880001,  //     setrl  r8, #0x0001
    0x40680000,  //     setrh  r8, #0x0000
                 // __luna_api_activate_relux_cmd_mnts_exit:
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x62070200,  //     mov  r7, r2
    0x62060500,  //     mov  r6, r5
    0x4b0600ff,  //     and  r6, #0xff
    0x58870600,  //     mul  r7, r6
    0x4507003f,  //     add  r7, #63
    0x51070006,  //     lsr  r7, #6
    0x62060500,  //     mov  r6, r5
    0x4b0600ff,  //     and  r6, #0xff
    0x4b060008,  //     and  r6, #8
    0x51060003,  //     lsr  r6, #3
    0x620a0600,  //     mov  r10, r6
    0x4b0a00ff,  //     and  r10, #0xff
    0x550a0008,  //     lsl  r10, #8
    0x62080700,  //     mov  r8, r7
    0x4b0800ff,  //     and  r8, #0xff
    0x4e080a00,  //     orr  r8, r10
    0x620a0600,  //     mov  r10, r6
    0x510a0008,  //     lsr  r10, #8
    0x550a0008,  //     lsl  r10, #8
    0x62090700,  //     mov  r9, r7
    0x51090008,  //     lsr  r9, #8
    0x4e090a00,  //     orr  r9, r10
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14,r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62170300,  //     mov  r23, r3
    0x62080400,  //     mov  r8, r4
    0x0b050808,  //     cmp  r5, #0x0808
    0x0d100007,  //     beq  __luna_api_activate_relux_cmd_master_i8_o8
    0x0b052008,  //     cmp  r5, #0x2008
    0x0d10000c,  //     beq  __luna_api_activate_relux_cmd_master_i8_o32
    0x0b050820,  //     cmp  r5, #0x0820
    0x0d100011,  //     beq  __luna_api_activate_relux_cmd_master_i32_o8
    0x0b052020,  //     cmp  r5, #0x2020
    0x0d100016,  //     beq  __luna_api_activate_relux_cmd_master_i32_o32
                 // __luna_api_activate_relux_cmd_master_i8_o8:
    0x41987700,  //     setrl  r24, #0x7700
    0x41990000,  //     setrl  r25, #0x0000
    0x419a4044,  //     setrl  r26, #0x4044
    0x407a0402,  //     setrh  r26, #0x0402
    0x98000001,  //     dprc   mmac0=1
    0x98e1031c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=3,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d300015,  //     b  __luna_api_activate_relux_cmd_master_exit
                 // __luna_api_activate_relux_cmd_master_i8_o32:
    0x41987700,  //     setrl  r24, #0x7700
    0x41990000,  //     setrl  r25, #0x0000
    0x419a4044,  //     setrl  r26, #0x4044
    0x407a0422,  //     setrh  r26, #0x0422
    0x98000001,  //     dprc   mmac0=1
    0x98e1011c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d30000e,  //     b  __luna_api_activate_relux_cmd_master_exit
                 // __luna_api_activate_relux_cmd_master_i32_o8:
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0003,  //     setrh  r26, #0x0003
    0x98000001,  //     dprc   mmac0=1
    0x98f40304,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3
    0x0d300007,  //     b  __luna_api_activate_relux_cmd_master_exit
                 // __luna_api_activate_relux_cmd_master_i32_o32:
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
                 // __luna_api_activate_relux_cmd_master_exit:
    0x62140100,  //     mov  r20,r1
    0x62060500,  //     mov  r6, r5
    0x51060008,  //     lsr  r6, #8
    0x62120200,  //     mov  r18, r2
    0x58920600,  //     mul  r18, r6
    0x51120003,  //     lsr  r18, #3
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x0b050808,  //     cmp  r5, #0x0808
    0x0d100007,  //     beq  __luna_api_activate_relux_cmd_iow_i8_o8
    0x0b052008,  //     cmp  r5, #0x2008
    0x0d100007,  //     beq  __luna_api_activate_relux_cmd_iow_i8_o32
    0x0b050820,  //     cmp  r5, #0x0820
    0x0d100007,  //     beq  __luna_api_activate_relux_cmd_iow_i32_o8
    0x0b052020,  //     cmp  r5, #0x2020
    0x0d100007,  //     beq  __luna_api_activate_relux_cmd_iow_i32_o32
                 // __luna_api_activate_relux_cmd_iow_i8_o8:
    0xa4000011,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300006,  //     b  __luna_api_activate_relux_cmd_iow_exit
                 // __luna_api_activate_relux_cmd_iow_i8_o32:
    0xa4000031,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300004,  //     b  __luna_api_activate_relux_cmd_iow_exit
                 // __luna_api_activate_relux_cmd_iow_i32_o8:
    0xa4000001,  //     iow  bw=16bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  __luna_api_activate_relux_cmd_iow_exit
                 // __luna_api_activate_relux_cmd_iow_i32_o32:
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
                 // __luna_api_activate_relux_cmd_iow_exit:
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};


__luna_cmd_attr__ uint32_t luna_api_psrammemcpy[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x62040000,  //     mov  r4,r0
    0x62050400,  //     mov  r5,r4
    0x45050014,  //     add  r5, #20
    0x10e40010,  //     ldro  r7, r4, #16
    0x0b070000,  //     cmp  r7, #0
    0x0d100006,  //     beq  #6
    0x89800001,  //     mnts   sel=io-ahb,rd=m0p0
    0x88200003,  //     mnts   sel=mas0-1,p0-iodat=ioahb
    0x89600001,  //     mnts   sel=io-wr,din=m0l
    0x89e00003,  //     mnts   sel=p-wr,wr=iowr
    0x0d300006,  //     b  #6
    0x89a00001,  //     mnts   sel=p-rd0,rd=iord0
    0x89200009,  //     mnts   sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts   sel=mas0-1,p0-iodat=iord0
    0x89600001,  //     mnts   sel=io-wr,din=m0l
    0x89e00003,  //     mnts   sel=p-wr,wr=iowr
    0x11050000,  //     ldro  r8, r5, #0
    0x11250004,  //     ldro  r9, r5, #4
    0x11450008,  //     ldro  r10, r5, #8
    0x1185000c,  //     ldro  r12, r5, #12
    0x11c50010,  //     ldro  r14, r5, #16
    0x10e50014,  //     ldro  r7, r5, #20
    0x0b070000,  //     cmp  r7, #0
    0x0d100003,  //     beq  #3
    0x90020294,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[10010100]
    0x0d300002,  //     b  #2
    0x90020258,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[01011000]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x10e5001c,  //     ldro  r7, r5, #28
    0x0b070000,  //     cmp  r7, #0
    0x0d100003,  //     beq  #3
    0x9000a20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0010],cas[00001101]
    0x0d300002,  //     b  #2
    0x9000a20d,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0010],cas[00001101]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x12850020,  //     ldro  r20, r5, #32
    0x12450024,  //     ldro  r18, r5, #36
    0x12650028,  //     ldro  r19, r5, #40
    0x12a50030,  //     ldro  r21, r5, #48
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

