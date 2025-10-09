#include "luna_fft_cmd.h"




__luna_cmd_attr__ uint32_t luna_fft_cmd[] = {
    0x62010000,  //     mov  r1, r0
    0x45010028,  //     add  r1, #40
    0x61030000,  //     mov  r3, #0
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x8610f000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1,grp3=1
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=iord0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x88c00001,  //     mnts  sel=slv0,din=m0l
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x11010000,  //     ldro  r8,r1,#0
    0x11210004,  //     ldro  r9,r1,#4
    0x11410008,  //     ldro  r10,r1,#8
    0x11e1000c,  //     ldro  r15,r1,#12
    0x11c10010,  //     ldro  r14,r1,#16
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x90081000,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000000]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0x12410014,  //     ldro  r18,r1,#20
    0x12610018,  //     ldro  r19,r1,#24
    0x1281001c,  //     ldro  r20,r1,#28
    0x12a10020,  //     ldro  r21,r1,#32
    0x96002640,  //     dstm0  slave0,mode=rotate,blk=1,precision=2, slave0,inband=64bit,outband=128bit
    0xf0000001,  //     init  start-master0
    0x10e00014,  //     ldro  r7,r0,#20
    0x0b070001,  //     cmp  r7,#1
    0x0d100004,  //     beq  __lid_wait_fft_ifft
    0x0b070002,  //     cmp  r7,#2
    0x0d100002,  //     beq  __lid_wait_fft_ifft
    0x0d300003,  //     b  __lid_wait_other
                 // __lid_wait_fft_ifft:
    0x80000004,  //     wait  slave0
    0x0d300027,  //     b  __lid_wait_exit
                 // __lid_wait_other:
    0x80000010,  //     wait  master0
    0x10e00014,  //     ldro  r7,r0,#20
    0x0b070004,  //     cmp  r7,#4
    0x0d180003,  //     bne  __lid_wait_fix0_cr256_ifft_exit
    0x62172600,  //     mov  r23,r38
    0x44031700,  //     add  r3,r23
                 // __lid_wait_fix0_cr256_ifft_exit:
    0x418a00f8,  //     setrl  r10,#0xf8
    0x418c00ff,  //     setrl  r12,#0xff
    0x40714000,  //     setrh  r17,#0x4000
    0x10e10024,  //     ldro  r7,r1,#36
    0x62280700,  //     movl  r8, r7
    0x10e10028,  //     ldro  r7,r1,#40
    0x62290700,  //     movl  r9, r7
    0x11c10034,  //     ldro  r14,r1,#52
    0x90000000,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00000000]
    0x90081000,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000000]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003100,  //     ares  mode=master,sel=select3,sel_data_sour1
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x10e00014,  //     ldro  r7,r0,#20
    0x0b070003,  //     cmp  r7,#3
    0x0d100008,  //     beq  __lid_wait_cr256_fft_cr256_ifft
    0x0b070004,  //     cmp  r7,#4
    0x0d100006,  //     beq  __lid_wait_cr256_fft_cr256_ifft
    0x0b070005,  //     cmp  r7,#5
    0x0d100008,  //     beq  __lid_wait_cr257_fft_cr257_ifft
    0x0b070006,  //     cmp  r7,#6
    0x0d100006,  //     beq  __lid_wait_cr257_fft_cr257_ifft
    0x0d300009,  //     b  __lid_wait_exit
                 // __lid_wait_cr256_fft_cr256_ifft:
    0x1181003c,  //     ldro  r12,r1,#60
    0x5c184000,  //     seti  mem_mode,reg=r12,grp=1,bank=0,addr=0
    0x5c185000,  //     seti  mem_mode,reg=r12,grp=1,bank=1,addr=0
    0x0d300005,  //     b  __lid_wait_exit
                 // __lid_wait_cr257_fft_cr257_ifft:
    0x1181003c,  //     ldro  r12,r1,#60
    0x5c185000,  //     seti  mem_mode,reg=r12,grp=1,bank=1,addr=0
    0x0d300002,  //     b  __lid_wait_exit
    0x0d300001,  //     b  __lid_wait_exit
                 // __lid_wait_exit:
    0x40710000,  //     setrh  r17,#0
    0x418c0000,  //     setrl  r12,#0
    0x406c0000,  //     setrh  r12,#0
    0x418f0000,  //     setrl  r15,#0
    0x4077001e,  //     setrh  r23, #30
    0x41982204,  //     setrl  r24, #0x2204
    0x13210044,  //     ldro  r25,r1,#68
    0x419ac000,  //     setrl  r26, #0xc000
    0x407a012b,  //     setrh  r26, #0x012b
    0x11e1004c,  //     ldro  r15,r1,#76
    0x12410050,  //     ldro  r18,r1,#80
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940002,  //     setrl  r20,#2
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x61020001,  //     mov  r2, #1
                 // __loop_pe_start:
    0x62060200,  //     mov  r6, r2
    0x4b060001,  //     and  r6, #0x1
    0x0b060001,  //     cmp  r6, #1
    0x0d100003,  //     beq  __pe_mnts_mod_1
    0x0b060000,  //     cmp  r6, #0
    0x0d10000b,  //     beq  __pe_mnts_mod_0
                 // __pe_mnts_mod_1:
    0x88200890,  //     mnts  sel=mas0-1,p0-lmdat1=grp0,p0-lmdat0=grp1
    0x88400040,  //     mnts  sel=mas0-2,p1-lmdat0=cosl
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x8b000010,  //     mnts  sel=cos,rd=m0p1_0
    0x8a400200,  //     mnts  sel=grp2,lmwr=s0_0
    0x8a600240,  //     mnts  sel=grp3,lmwr=s0_1
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x0d30000b,  //     b  __pe_mnts_exit
                 // __pe_mnts_mod_0:
    0x88200ab0,  //     mnts  sel=mas0-1,p0-lmdat1=grp2,p0-lmdat0=grp3
    0x88400040,  //     mnts  sel=mas0-2,p1-lmdat0=cosl
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x8b000010,  //     mnts  sel=cos,rd=m0p1_0
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x8a400008,  //     mnts  sel=grp2,lmrd=m0p0_0
    0x8a600009,  //     mnts  sel=grp3,lmrd=m0p0_1
    0x0d300001,  //     b  __pe_mnts_exit
                 // __pe_mnts_exit:
    0x62070200,  //     mov  r7,r2
    0x45870001,  //     sub  r7,#1
    0x61060001,  //     mov  r6,#1
    0x54060700,  //     lsl  r6,r7
    0x62070200,  //     mov  r7,r2
    0x10a00008,  //     ldro  r5,r0,#8
    0x50050700,  //     lsr  r5,r7
    0x62070600,  //     mov  r7,r6
    0x4b0700ff,  //     and  r7,#0xff
    0x62280700,  //     movl  r8,r7
    0x62070500,  //     mov  r7,r5
    0x4b0700ff,  //     and  r7,#0xff
    0x62680700,  //     movh  r8,r7
    0x62070600,  //     mov  r7,r6
    0x51070008,  //     lsr  r7,#8
    0x62290700,  //     movl  r9,r7
    0x62070500,  //     mov  r7,r5
    0x51070008,  //     lsr  r7,#8
    0x62690700,  //     movh  r9,r7
    0x90000200,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0010],ces[00000000]
    0x418a0001,  //     setrl  r10,#1
    0x626a0600,  //     movh  r10,r6
    0x9000a005,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over2,eac[0000],cas[00000101]
    0x418a0010,  //     setrl  r10,#16
    0x406a0000,  //     setrh  r10,#0
    0x90086005,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over1,eac[0000],cas[00000101]
    0x41870004,  //     setrl  r7, #0x0004
    0x0b020001,  //     cmp  r2, #1
    0x65470400,  //     cmoveq  r7, #0x0400
    0x62300700,  //     movl  r16, r7
    0x90053501,  //     ares  mode=master,sel=select3,rcils[0101],sel_counter_line0,sel_data_sour5,sel_dp0,dcl1
    0x10e00014,  //     ldro  r7,r0,#20
    0x0b070004,  //     cmp  r7,#4
    0x0d100002,  //     beq  __pe_fix0_cr256_ifft_1
    0x0d180006,  //     bne  __pe_fix0_cr256_ifft_0
                 // __pe_fix0_cr256_ifft_1:
    0x0b020001,  //     cmp  r2,#1
    0x0d100007,  //     beq  __pe_fix0_cr256_ifft_exit
    0x62172600,  //     mov  r23,r38
    0x44031700,  //     add  r3,r23
    0x0d300004,  //     b  __pe_fix0_cr256_ifft_exit
                 // __pe_fix0_cr256_ifft_0:
    0x62172600,  //     mov  r23,r38
    0x44031700,  //     add  r3,r23
    0x0d300001,  //     b  __pe_fix0_cr256_ifft_exit
                 // __pe_fix0_cr256_ifft_exit:
    0x4077001e,  //     setrh  r23, #30
    0x98000001,  //     dprc   mmac0=1
    0x98b60144,  //     dprc   addtree=l3,comshift=l5,outlayer=l4,outlayer-mode=1,layer3-mode=1,intgnet-mode=4,intgnet=2
    0x96002650,  //     dstm0  slave0,mode=rotate,format=0,blk=1,precision=2, slave0,inband=128bit,outband=128bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x41990c0c,  //     setrl  r25, #0x0c0c
    0x40790c0c,  //     setrh  r25, #0x0c0c
    0x418f0000,  //     setrl  r15,#0x0000
    0x10e0000c,  //     ldro  r7, r0, #12
    0x0a020700,  //     cmp  r2,r7
    0x0d100003,  //     beq  __loop_pe_end
    0x45020001,  //     add  r2,#1
    0x0d30ffae,  //     b  __loop_pe_start
                 // __loop_pe_end:
    0x10e00020,  //     ldro  r7,r0,#32
    0x10c0001c,  //     ldro  r6,r0,#28
    0x61050000,  //     mov  r5,#0
    0x0b060001,  //     cmp  r6,#1
    0x66450300,  //     cmoveq  r5,r3
    0x44070500,  //     add  r7,r5
    0x61080001,  //     mov  r8,#1
    0x54080700,  //     lsl  r8,r7
    0x88400660,  //     mnts  sel=mas0-2,p1-lmdat1=r8,p1-lmdat0=r8
    0x10e00008,  //     ldro  r7, r0, #8
    0x0b070080,  //     cmp  r7, #128
    0x0d100007,  //     beq  __output_mnts_mod_128_512
    0x0b070200,  //     cmp  r7, #512
    0x0d100005,  //     beq  __output_mnts_mod_128_512
    0x0b070040,  //     cmp  r7, #64
    0x0d10000a,  //     beq  __output_mnts_mod_64_256
    0x0b070100,  //     cmp  r7, #256
    0x0d100008,  //     beq  __output_mnts_mod_64_256
                 // __output_mnts_mod_128_512:
    0x88200ba0,  //     mnts  sel=mas0-1,p0-lmdat1=grp3,p0-lmdat0=grp2
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x8a400008,  //     mnts  sel=grp2,lmrd=m0p0_0
    0x8a600009,  //     mnts  sel=grp3,lmrd=m0p0_1
    0x0d300008,  //     b  __output_mnts_exit
                 // __output_mnts_mod_64_256:
    0x88200980,  //     mnts  sel=mas0-1,p0-lmdat1=grp1,p0-lmdat0=grp0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x0d300001,  //     b  __output_mnts_exit
                 // __output_mnts_exit:
    0x10e00018,  //     ldro  r7,r0,#24
    0x0b070001,  //     cmp  r7,#1
    0x0d100004,  //     beq  __output_master_s0_model_normal
    0x0b070003,  //     cmp  r7,#3
    0x0d100006,  //     beq  __output_master_s0_model_realonly
    0x0d300009,  //     b  __output_master_s0_model_other
                 // __output_master_s0_model_normal:
    0x11010060,  //     ldro  r8,r1,#96
    0x11210064,  //     ldro  r9,r1,#100
    0x90000020,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00100000]
    0x0d300009,  //     b  __output_master_s0_model_exit
                 // __output_master_s0_model_realonly:
    0x11010060,  //     ldro  r8,r1,#96
    0x11210064,  //     ldro  r9,r1,#100
    0x90000010,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00010000]
    0x0d300005,  //     b  __output_master_s0_model_exit
                 // __output_master_s0_model_other:
    0x11010060,  //     ldro  r8,r1,#96
    0x11210064,  //     ldro  r9,r1,#100
    0x90000020,  //     ares  mode=master,sel=select0,chs[0000],sel_row0,cmc[0000],ces[00100000]
    0x0d300001,  //     b  __output_master_s0_model_exit
                 // __output_master_s0_model_exit:
    0x10e10068,  //     ldro  r7,r1,#104
    0x622a0700,  //     movl  r10,r7
    0x11c1006c,  //     ldro  r14,r1,#108
    0x10e00018,  //     ldro  r7,r0,#24
    0x0b070003,  //     cmp  r7,#3
    0x0d100002,  //     beq  __output_master_s2_model_realonly
    0x0d300003,  //     b  __output_master_s2_model_other
                 // __output_master_s2_model_realonly:
    0x90006003,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over1,eac[0000],cas[00000011]
    0x0d300003,  //     b  __output_master_s2_model_exit
                 // __output_master_s2_model_other:
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0d300001,  //     b  __output_master_s2_model_exit
                 // __output_master_s2_model_exit:
    0x9008a000,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over2,eac[0000],cas[00000000]
    0x10e10070,  //     ldro  r7,r1,#112
    0x62300700,  //     movl  r16,r7
    0x10e00014,  //     ldro  r7,r0,#20
    0x4b070001,  //     and  r7,#0x01
    0x0b070000,  //     cmp  r7,#0
    0x62270f00,  //     movl  r7,r15
    0x65470040,  //     cmoveq  r7,#0x0040
    0x622f0700,  //     movl  r15,r7
    0x90093501,  //     ares  mode=master,sel=select3,rcils[1001],sel_counter_line0,sel_data_sour5,sel_dp0,dcl1
    0x12e00024,  //     ldro  r23,r0,#36
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x12810078,  //     ldro  r20,r1,#120
    0x1241007c,  //     ldro  r18,r1,#124
    0x12610080,  //     ldro  r19,r1,#128
    0x12a10084,  //     ldro  r21,r1,#132
    0x10e00018,  //     ldro  r7,r0,#24
    0x0b070003,  //     cmp  r7,#3
    0x0d100002,  //     beq  __output_iow_model_realonly
    0x0d300003,  //     b  __output_iow_model_other
                 // __output_iow_model_realonly:
    0xa4000011,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300003,  //     b  __output_iow_model_exit
                 // __output_iow_model_other:
    0xa4000031,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300001,  //     b  __output_iow_model_exit
                 // __output_iow_model_exit:
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x10e00010,  //     ldro  r7,r0,#16
    0x2c018700,  //     str   r3, [r7]
    0x61000000,  //     mov   r0, #0
    0x0cf01e00,  //     bl    lr
};

