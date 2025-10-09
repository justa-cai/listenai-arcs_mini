#include "luna_vector_cmd.h"
#include "luna/luna.h"

// api_type, 0:add, 1:sub, 2:offset
__luna_cmd_attr__ uint32_t luna_api_vector_add_new[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x28800080,  //     ldm  r0, {r0-r6}
    0x62070500,  //     mov  r7, r5
    0x51070004,  //     lsr  r7, #4
    0x4b050003,  //     and  r5, #0x03
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x0b060002,  //     cmp  r6, #2
    0x0d100005,  //     beq  __vector_offset_branch0
                 // __vector_add_branch0:
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x0d30000d,  //     b  __vector_branch0_end
                 // __vector_offset_branch0:
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100009,  //     beq  #9
    0x62080100,  //     mov  r8, r1
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x0d300002,  //     b  #2
    0x62080100,  //     mov  r8, r1
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
                 // __vector_branch0_end:
    0x62080300,  //     mov  r8, r3
    0x0b050003,  //     cmp  r5, #0x03
    0x0d10000a,  //     beq  #10
    0x45080007,  //     add  r8, #7
    0x51080003,  //     lsr  r8, #3
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x0b070001,  //     cmp  r7, #0x01
    0x0d100002,  //     beq  #2
    0x45080100,  //     add  r8, #256
    0x51090008,  //     lsr  r9, #8
    0x0d300006,  //     b  #6
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0008,  //     setrl  r10,#8
    0x620e0100,  //     mov  r14, r1
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0x418f0031,  //     setrl  r15,#49
    0x0d300002,  //     b  #2
    0x418f0012,  //     setrl  r15,#18
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62170400,  //     mov  r23, r4
    0x55170010,  //     lsl  r23, #16
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100022,  //     beq  __vector_input_32bit_branch
                 // __vector_input_8bit_branch:
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100007,  //     beq  #7
    0x41987000,  //     setrl  r24, #0x7000
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419a4144,  //     setrl  r26, #0x4144
    0x407a0002,  //     setrh  r26, #0x0002
    0x0d300006,  //     b  #6
    0x41983000,  //     setrl  r24, #0x3000
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac144,  //     setrl  r26, #0xc144
    0x407a0423,  //     setrh  r26, #0x0423
    0x0b060001,  //     cmp  r6, #1
    0x0d100007,  //     beq  __vector_sub_branch1
                 // __vector_add_branch1:
    0x41880101,  //     setrl  r8 , #0x0101
    0x40680101,  //     setrh  r8 , #0x0101
    0x62090800,  //     mov    r9 ,r8
    0x620a0800,  //     mov    r10,r8
    0x620b0800,  //     mov    r11,r8
    0x0d300006,  //     b  __vector_branch1_end
                 // __vector_sub_branch1:
    0x4188ff01,  //     setrl  r8 , #0xff01
    0x4068ff01,  //     setrh  r8 , #0xff01
    0x62090800,  //     mov    r9 ,r8
    0x620a0800,  //     mov    r10,r8
    0x620b0800,  //     mov    r11,r8
                 // __vector_branch1_end:
    0x98000001,  //     dprc   mmac0=1
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100003,  //     beq  #3
    0x98d00200,  //     dprc   comshift=l2,outlayer=l6,outlayer-mode=2
    0x0d300002,  //     b  #2
    0x98e1011c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d30001e,  //     b  __vector_input_branch_end
                 // __vector_input_32bit_branch:
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac122,  //     setrl  r26, #0xc122
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100003,  //     beq  #3
    0x407a0103,  //     setrh  r26, #0x0103
    0x0d300002,  //     b  #2
    0x407a0123,  //     setrh  r26, #0x0123
    0x0b060001,  //     cmp  r6, #1
    0x0d100007,  //     beq  __vector_sub_branch2
                 // __vector_add_branch2:
    0x41880001,  //     setrl  r8 , #0x0001
    0x40680000,  //     setrh  r8 , #0x0000
    0x62090800,  //     mov    r9 ,r8
    0x620a0800,  //     mov    r10,r8
    0x620b0800,  //     mov    r11,r8
    0x0d300007,  //     b  __vector_branch2_end
                 // __vector_sub_branch2:
    0x41880001,  //     setrl  r8 , #0x0001
    0x40680000,  //     setrh  r8 , #0x0000
    0x4189ffff,  //     setrl  r9 , #0xffff
    0x4069ffff,  //     setrh  r9 , #0xffff
    0x620a0800,  //     mov    r10,r8
    0x620b0900,  //     mov    r11,r9
                 // __vector_branch2_end:
    0x98000001,  //     dprc   mmac0=1
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100003,  //     beq  #3
    0x98f40304,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3
    0x0d300002,  //     b  #2
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
                 // __vector_input_branch_end:
    0x62140200,  //     mov  r20, r2
    0x62120300,  //     mov  r18, r3
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100002,  //     beq  #2
    0x0d300002,  //     b  #2
    0x55120002,  //     lsl  r18, #2
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100007,  //     beq  #7
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100003,  //     beq  #3
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300008,  //     b  #8
    0xa4000031,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300006,  //     b  #6
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100003,  //     beq  #3
    0xa4000001,  //     iow  bw=16bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  #2
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

// api_type, 0:mul, 1:scale, 2:dot, 3:sum
__luna_cmd_attr__ uint32_t luna_api_vector_mul_new[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x28800080,  //     ldm  r0, {r0-r6}
    0x62070500,  //     mov  r7, r5
    0x51070004,  //     lsr  r7, #4
    0x4b050003,  //     and  r5, #0x03
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x0b060001,  //     cmp  r6, #1
    0x0d100007,  //     beq  __vector_scale_branch0
    0x0b060003,  //     cmp  r6, #3
    0x0d100012,  //     beq  __vector_sum_branch0
                 // __vector_mul_branch0:
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x0d300016,  //     b  __vector_branch0_end
                 // __vector_scale_branch0:
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100009,  //     beq  #9
    0x62080100,  //     mov  r8, r1
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x0d300002,  //     b  #2
    0x62080100,  //     mov  r8, r1
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x0d300009,  //     b  __vector_branch0_end
                 // __vector_sum_branch0:
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100004,  //     beq  #4
    0x41880101,  //     setrl  r8, #0x0101
    0x40680101,  //     setrh  r8, #0x0101
    0x0d300003,  //     b  #3
    0x41880001,  //     setrl  r8, #0x1
    0x40680000,  //     setrh  r8, #0x0
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
                 // __vector_branch0_end:
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x62080300,  //     mov  r8, r3
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100008,  //     beq  #8
    0x45080007,  //     add  r8, #7
    0x51080003,  //     lsr  r8, #3
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x45080100,  //     add  r8, #256
    0x51090008,  //     lsr  r9, #8
    0x0d300006,  //     b  #6
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0008,  //     setrl  r10,#8
    0x620e0100,  //     mov  r14, r1
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x620f0300,  //     mov  r15, r3
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0x550f0003,  //     lsl  r15, #3
    0x0d300002,  //     b  #2
    0x550f0005,  //     lsl  r15, #5
    0x4b0f003f,  //     and  r15, #0x3f
    0x510f0003,  //     lsr  r15, #3
    0x550f000c,  //     lsl  r15, #12
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62170400,  //     mov  r23, r4
    0x55170010,  //     lsl  r23, #16
    0x0b060002,  //     cmp  r6, #2
    0x0d10001f,  //     beq  __vector_dot_sum_branch1
    0x0b060003,  //     cmp  r6, #3
    0x0d10001d,  //     beq  __vector_dot_sum_branch1
                 // __vector_mul_branch1:
    0x0b050003,  //     cmp  r5, #0x03
    0x0d10000e,  //     beq  #14
    0x41987700,  //     setrl  r24, #0x7700
    0x41990000,  //     setrl  r25, #0x0000
    0x419a4044,  //     setrl  r26, #0x4044
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100005,  //     beq  #5
    0x407a0402,  //     setrh  r26, #0x0402
    0x98000001,  //     dprc   mmac0=1
    0x98e1031c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=3,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d300004,  //     b  #4
    0x407a0422,  //     setrh  r26, #0x0422
    0x98000001,  //     dprc   mmac0=1
    0x98e1011c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d30000d,  //     b  #13
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100005,  //     beq  #5
    0x407a0003,  //     setrh  r26, #0x0003
    0x98000001,  //     dprc   mmac0=1
    0x98f40304,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3
    0x0d300004,  //     b  #4
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x0d30002a,  //     b  __vector_branch1_end
                 // __vector_dot_sum_branch1:
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100012,  //     beq  #18
    0x41980000,  //     setrl  r24, #0x0000
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac044,  //     setrl  r26, #0xc044
    0x621b0300,  //     mov  r27, r3
    0x451b0007,  //     add  r27, #7
    0x511b0003,  //     lsr  r27, #3
    0x459b0001,  //     sub  r27, #1
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100005,  //     beq  #5
    0x407a0002,  //     setrh  r26, #0x0002
    0x98000001,  //     dprc   mmac0=1
    0x98e10310,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=3,layer3-mode=0,intgnet=l3,intgnet-mode=1
    0x0d300004,  //     b  #4
    0x407a0022,  //     setrh  r26, #0x0022
    0x98000001,  //     dprc   mmac0=1
    0x98e10110,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=0,intgnet=l3,intgnet-mode=1
    0x0d300017,  //     b  #23
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x621b0300,  //     mov  r27, r3
    0x451b0001,  //     add  r27, #1
    0x511b0001,  //     lsr  r27, #1
    0x459b0001,  //     sub  r27, #1
    0x0b070004,  //     cmp  r7, #0x04
    0x0d10000b,  //     beq  #11
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100005,  //     beq  #5
    0x407a0103,  //     setrh  r26, #0x0103
    0x98000001,  //     dprc   mmac0=1
    0x98e70324,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3,intgnet=l5,intgnet-mode=2
    0x0d300008,  //     b  #8
    0x407a0123,  //     setrh  r26, #0x0123
    0x98000001,  //     dprc   mmac0=1
    0x98e70124,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3,intgnet=l5,intgnet-mode=2
    0x0d300004,  //     b  #4
    0x407a0133,  //     setrh  r26, #0x0133
    0x98000001,  //     dprc   mmac0=1
    0x98e70024,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=0,layer3-mode=1,addtree=l3,intgnet=l5,intgnet-mode=2
                 // __vector_branch1_end:
    0x62140200,  //     mov  r20, r2
    0x419201a4,  //     setrl  r18,#420
    0x40720000,  //     setrh  r18,#0
    0x0b060002,  //     cmp  r6, #2
    0x0d100009,  //     beq  __vector_dot_sum_branch2
    0x0b060003,  //     cmp  r6, #3
    0x0d100007,  //     beq  __vector_dot_sum_branch2
                 // __vector_mul_branch2:
    0x62120300,  //     mov  r18, r3
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100002,  //     beq  #2
    0x0d300002,  //     b  #2
    0x55120002,  //     lsl  r18, #2
    0x0d30000b,  //     b  __vector_branch2_end
                 // __vector_dot_sum_branch2:
    0x0b070004,  //     cmp  r7, #0x04
    0x0d100007,  //     beq  #7
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100003,  //     beq  #3
    0x41920001,  //     setrl  r18, #1
    0x0d300004,  //     b  #4
    0x41920004,  //     setrl  r18, #4
    0x0d300002,  //     b  #2
    0x41920008,  //     setrl  r18, #8
    0x40720000,  //     setrh  r18, #0
                 // __vector_branch2_end:
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100007,  //     beq  #7
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100003,  //     beq  #3
    0xa4000011,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d30000c,  //     b  #12
    0xa4000031,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d30000a,  //     b  #10
    0x0b070004,  //     cmp  r7, #0x04
    0x0d100007,  //     beq  #7
    0x0b070003,  //     cmp  r7, #0x03
    0x0d100003,  //     beq  #3
    0xa4000001,  //     iow  bw=16bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300004,  //     b  #4
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  #2
    0xa4000031,  //     iow  bw=128bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_vector_cmp[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x288000c0,  //     ldm  r0, {r0-r5}
    0x62060500,  //     mov  r6, r5
    0x51050004,  //     lsr  r5, #4
    0x4b06000f,  //     and  r6, #0x0f
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x0906000b,  //     cmp  r6, #0x0b
    0x0d100005,  //     beq  #5
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x0d30000b,  //     b  #11
    0x62080100,  //     mov  r8, r1
    0x09050003,  //     cmp  r5, #0x03
    0x0d100007,  //     beq  #7
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x55010008,  //     lsl  r1, #8
    0x4e080100,  //     orr  r8, r1
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x62080300,  //     mov  r8, r3
    0x09050003,  //     cmp  r5, #0x03
    0x0d100008,  //     beq  #8
    0x45080007,  //     add  r8, #7
    0x51080003,  //     lsr  r8, #3
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x45080100,  //     add  r8, #256
    0x51090008,  //     lsr  r9, #8
    0x0d300006,  //     b  #6
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0906000b,  //     cmp  r6, #0x0b
    0x0d100004,  //     beq  #4
    0x418a0008,  //     setrl  r10,#8
    0x620e0100,  //     mov  r14, r1
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x09050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0x418f0031,  //     setrl  r15,#49
    0x0d300002,  //     b  #2
    0x418f0012,  //     setrl  r15,#18
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x40770000,  //     setrh  r23, #0
    0x09040004,  //     cmp  r4, #0x04
    0x0d100009,  //     beq  #9
    0x09040002,  //     cmp  r4, #0x02
    0x0d100005,  //     beq  #5
    0x09040000,  //     cmp  r4, #0x00
    0x0d100003,  //     beq  #3
    0x41970010,  //     setrl  r23, #0x10
    0x0d300004,  //     b  #4
    0x41970020,  //     setrl  r23, #0x20
    0x0d300002,  //     b  #2
    0x41970030,  //     setrl  r23, #0x30
    0x09050003,  //     cmp  r5, #0x03
    0x0d100015,  //     beq  #21
    0x41983000,  //     setrl  r24, #0x3000
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac144,  //     setrl  r26, #0xc144
    0x407a0403,  //     setrh  r26, #0x0403
    0x09040000,  //     cmp  r4, #0x00
    0x0d100006,  //     beq  #6
    0x09040003,  //     cmp  r4, #0x03
    0x0d100004,  //     beq  #4
    0x4188ff01,  //     setrl  r8 , #0xff01
    0x4068ff01,  //     setrh  r8 , #0xff01
    0x0d300003,  //     b  #3
    0x418801ff,  //     setrl  r8 , #0x01ff
    0x406801ff,  //     setrh  r8 , #0x01ff
    0x62090800,  //     mov    r9 ,r8
    0x620a0800,  //     mov    r10,r8
    0x620b0800,  //     mov    r11,r8
    0x98000001,  //     dprc   mmac0=1
    0x98e1031c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=3,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d300017,  //     b  #23
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac122,  //     setrl  r26, #0xc122
    0x407a0123,  //     setrh  r26, #0x0123
    0x09040000,  //     cmp  r4, #0x00
    0x0d100008,  //     beq  #8
    0x09040003,  //     cmp  r4, #0x03
    0x0d100006,  //     beq  #6
    0x41880001,  //     setrl  r8 , #0x0001
    0x40680000,  //     setrh  r8 , #0x0000
    0x4189ffff,  //     setrl  r9 , #0xffff
    0x4069ffff,  //     setrh  r9 , #0xffff
    0x0d300005,  //     b  #5
    0x4188ffff,  //     setrl  r8 , #0xffff
    0x4068ffff,  //     setrh  r8 , #0xffff
    0x41890001,  //     setrl  r9 , #0x0001
    0x40690000,  //     setrh  r9 , #0x0000
    0x620a0800,  //     mov    r10,r8
    0x620b0900,  //     mov    r11,r9
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62140200,  //     mov  r20, r2
    0x62120300,  //     mov  r18, r3
    0x09050003,  //     cmp  r5, #0x03
    0x0d100002,  //     beq  #2
    0x0d300002,  //     b  #2
    0x55120002,  //     lsl  r18, #2
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x09050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0xa4000011,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  #2
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_vector_maxmin[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x288000c0,  //     ldm  r0, {r0-r5}
    0x62060500,  //     mov  r6, r5
    0x51050004,  //     lsr  r5, #4
    0x4b06000f,  //     and  r6, #0x0f
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x62080300,  //     mov  r8, r3
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100007,  //     beq  #7
    0x45080007,  //     add  r8, #7
    0x51080003,  //     lsr  r8, #3
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x0d300006,  //     b  #6
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100009,  //     beq  #9
    0x0b06000a,  //     cmp  r6, #0x0a
    0x0d100004,  //     beq  #4
    0x41887f7f,  //     setrl  r8,  #0x7f7f
    0x40687f7f,  //     setrh  r8,  #0x7f7f
    0x0d30000b,  //     b  #11
    0x41888080,  //     setrl  r8,  #0x8080
    0x40688080,  //     setrh  r8,  #0x8080
    0x0d300008,  //     b  #8
    0x0b06000a,  //     cmp  r6, #0x0a
    0x0d100004,  //     beq  #4
    0x4188ffff,  //     setrl  r8,  #0xffff
    0x40687fff,  //     setrh  r8,  #0x7fff
    0x0d300003,  //     b  #3
    0x41880000,  //     setrl  r8,  #0x0000
    0x40688000,  //     setrh  r8,  #0x8000
    0x620f0300,  //     mov  r15, r3
    0x0b050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0x550f0003,  //     lsl  r15, #3
    0x0d300002,  //     b  #2
    0x550f0005,  //     lsl  r15, #5
    0x4b0f003f,  //     and  r15, #0x3f
    0x510f0003,  //     lsr  r15, #3
    0x450f0008,  //     add  r15, #8
    0x550f000c,  //     lsl  r15, #12
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x40770000,  //     setrh  r23, #0x0
    0x41970000,  //     setrl  r23, #0
    0x0b050003,  //     cmp  r5, #0x03
    0x0d10001b,  //     beq  #27
    0x41985540,  //     setrl  r24, #0x5540
    0x40780005,  //     setrh  r24, #0x0005
    0x621a0300,  //     mov  r26, r3
    0x551a0003,  //     lsl  r26, #3
    0x4b1a003f,  //     and  r26, #0x3f
    0x511a0003,  //     lsr  r26, #3
    0x621b1a00,  //     mov  r27, r26
    0x511a0002,  //     lsr  r26, #2
    0x451a0002,  //     add  r26, #2
    0x551a000c,  //     lsl  r26, #12
    0x4f1a0044,  //     orr  r26, #0x0044
    0x4b1b0003,  //     and  r27, #0x03
    0x551b000a,  //     lsl  r27, #10
    0x4e1a1b00,  //     orr  r26, r27
    0x0b06000a,  //     cmp  r6, #0x0a
    0x0d100003,  //     beq  #3
    0x407a8400,  //     setrh  r26, #0x8400
    0x0d300002,  //     b  #2
    0x407a0400,  //     setrh  r26, #0x0400
    0x621b0300,  //     mov  r27, r3
    0x451b0007,  //     add  r27, #7
    0x511b0003,  //     lsr  r27, #3
    0x459b0001,  //     sub  r27, #1
    0x98000001,  //     dprc   mmac0=1
    0x98810050,  //     dprc   layer3-mode=0,intgnet=l3,intgnet-mode=5,outlayer=l4
    0x0d30001a,  //     b  #26
    0x41983343,  //     setrl  r24, #0x3343
    0x40780003,  //     setrh  r24, #0x0003
    0x621a0300,  //     mov  r26, r3
    0x551a0005,  //     lsl  r26, #5
    0x4b1a003f,  //     and  r26, #0x3f
    0x511a0005,  //     lsr  r26, #5
    0x621b1a00,  //     mov  r27, r26
    0x511a0002,  //     lsr  r26, #2
    0x451a0006,  //     add  r26, #6
    0x551a000c,  //     lsl  r26, #12
    0x4f1a0022,  //     orr  r26, #0x0022
    0x4b1b0003,  //     and  r27, #0x03
    0x551b000a,  //     lsl  r27, #10
    0x4e1a1b00,  //     orr  r26, r27
    0x0b06000a,  //     cmp  r6, #0x0a
    0x0d100003,  //     beq  #3
    0x407a8002,  //     setrh  r26, #0x8002
    0x0d300002,  //     b  #2
    0x407a0002,  //     setrh  r26, #0x0002
    0x621b0300,  //     mov  r27, r3
    0x451b0001,  //     add  r27, #1
    0x511b0001,  //     lsr  r27, #1
    0x459b0001,  //     sub  r27, #1
    0x98000001,  //     dprc   mmac0=1
    0x9881005c,  //     dprc   layer3-mode=3,intgnet=l3,intgnet-mode=5,outlayer=l4
    0x62140200,  //     mov  r20, r2
    0x41920008,  //     setrl  r18,#8
    0x40720000,  //     setrh  r18,#0
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_vector_conj[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x288000c0,  //     ldm  r0, {r0-r5}
    0x62060500,  //     mov  r6, r5
    0x51050004,  //     lsr  r5, #4
    0x4b060003,  //     and  r6, #0x03
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x09060003,  //     cmp  r6, #0x03
    0x0d100005,  //     beq  #5
    0x4188ff01,  //     setrl  r8, #0xff01
    0x4068ff01,  //     setrh  r8, #0xff01
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x0d300006,  //     b  #6
    0x41880001,  //     setrl  r8, #0x1
    0x40680000,  //     setrh  r8, #0x0
    0x4189ffff,  //     setrl  r9, #0xffff
    0x4069ffff,  //     setrh  r9, #0xffff
    0x88400007,  //     mnts  sel=mas0-2,p1-iodat=r8r9
    0x62080300,  //     mov  r8, r3
    0x09060003,  //     cmp  r6, #0x03
    0x0d100008,  //     beq  #8
    0x45080003,  //     add  r8, #3
    0x51080002,  //     lsr  r8, #2
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x45080100,  //     add  r8, #256
    0x51090008,  //     lsr  r9, #8
    0x0d300004,  //     b  #4
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62170400,  //     mov  r23, r4
    0x55170010,  //     lsl  r23, #16
    0x09060003,  //     cmp  r6, #0x03
    0x0d100008,  //     beq  #8
    0x41987700,  //     setrl  r24, #0x7700
    0x41990000,  //     setrl  r25, #0x0000
    0x419a4044,  //     setrl  r26, #0x4044
    0x407a0402,  //     setrh  r26, #0x0402
    0x98000001,  //     dprc   mmac0=1
    0x98e1031c,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=3,layer3-mode=3,intgnet=l3,intgnet-mode=1
    0x0d300007,  //     b  #7
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62140200,  //     mov  r20, r2
    0x62120300,  //     mov  r18, r3
    0x09050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0x55120001,  //     lsl  r18, #1
    0x0d300002,  //     b  #2
    0x55120003,  //     lsl  r18, #3
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x09060003,  //     cmp  r6, #0x03
    0x0d100003,  //     beq  #3
    0xa4000011,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  #2
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_vec_cplx_mul[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x288000c0,  //     ldm  r0, {r0-r5}
    0x62060500,  //     mov  r6, r5
    0x51050004,  //     lsr  r5, #4
    0x4b06000f,  //     and  r6, #0x0f
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x62080300,  //     mov  r8, r3
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0008,  //     setrl  r10,#8
    0x620e0100,  //     mov  r14, r1
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62170400,  //     mov  r23, r4
    0x55170010,  //     lsl  r23, #16
    0x41982204,  //     setrl  r24, #0x2204
    0x0906000a,  //     cmp  r6, #0x0a
    0x0d100004,  //     beq  #4
    0x4199c0c0,  //     setrl  r25, #0xc0c0
    0x4079c0c0,  //     setrh  r25, #0xc0c0
    0x0d300003,  //     b  #3
    0x41990c0c,  //     setrl  r25, #0x0c0c
    0x40790c0c,  //     setrh  r25, #0x0c0c
    0x419ac000,  //     setrl  r26, #0xc000
    0x09050003,  //     cmp  r5, #0x03
    0x0d100005,  //     beq  #5
    0x407a0103,  //     setrh  r26, #0x0103
    0x98000001,  //     dprc   mmac0=1
    0x98f40304,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3
    0x0d300004,  //     b  #4
    0x407a0123,  //     setrh  r26, #0x0123
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62140200,  //     mov  r20, r2
    0x62120300,  //     mov  r18, r3
    0x09050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0x55120001,  //     lsl  r18, #1
    0x0d300002,  //     b  #2
    0x55120003,  //     lsl  r18, #3
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x09050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0xa4000001,  //     iow  bw=16bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  #2
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_vec_cplx_mul_real[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x288000c0,  //     ldm  r0, {r0-r5}
    0x62060500,  //     mov  r6, r5
    0x51050004,  //     lsr  r5, #4
    0x4b06000f,  //     and  r6, #0x0f
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x62080300,  //     mov  r8, r3
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0004,  //     setrl  r10,#4
    0x620e0100,  //     mov  r14, r1
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62170400,  //     mov  r23, r4
    0x55170010,  //     lsl  r23, #16
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x41990000,  //     setrl  r25, #0x0000
    0x419ac012,  //     setrl  r26, #0xc012
    0x09050003,  //     cmp  r5, #0x03
    0x0d100005,  //     beq  #5
    0x407a0003,  //     setrh  r26, #0x0003
    0x98000001,  //     dprc   mmac0=1
    0x98f40304,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3
    0x0d300004,  //     b  #4
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62140200,  //     mov  r20, r2
    0x62120300,  //     mov  r18, r3
    0x09050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0x55120001,  //     lsl  r18, #1
    0x0d300002,  //     b  #2
    0x55120003,  //     lsl  r18, #3
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x09050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0xa4000001,  //     iow  bw=16bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  #2
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_vec_cplx_mul_ou_real[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x288000c0,  //     ldm  r0, {r0-r5}
    0x62060500,  //     mov  r6, r5
    0x51050004,  //     lsr  r5, #4
    0x4b06000f,  //     and  r6, #0x0f
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x62080300,  //     mov  r8, r3
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0008,  //     setrl  r10,#8
    0x620e0100,  //     mov  r14, r1
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62170400,  //     mov  r23, r4
    0x55170010,  //     lsl  r23, #16
    0x41982202,  //     setrl  r24, #0x2202
    0x4199cccc,  //     setrl  r25, #0xcccc
    0x4079cccc,  //     setrh  r25, #0xcccc
    0x419ac022,  //     setrl  r26, #0xc022
    0x09050003,  //     cmp  r5, #0x03
    0x0d100005,  //     beq  #5
    0x407a0103,  //     setrh  r26, #0x0103
    0x98000001,  //     dprc   mmac0=1
    0x98f40304,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=3,layer3-mode=1,addtree=l3
    0x0d300004,  //     b  #4
    0x407a0123,  //     setrh  r26, #0x0123
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62140200,  //     mov  r20, r2
    0x62120300,  //     mov  r18, r3
    0x09050003,  //     cmp  r5, #0x03
    0x0d100002,  //     beq  #2
    0x0d300002,  //     b  #2
    0x55120002,  //     lsl  r18, #2
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000011,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_vec_cplx_modulus[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x288000c0,  //     ldm  r0, {r0-r5}
    0x62060500,  //     mov  r6, r5
    0x51050004,  //     lsr  r5, #4
    0x4b06000f,  //     and  r6, #0x0f
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x88400001,  //     mnts  sel=mas0-2,p1-iodat=iord0
    0x62080300,  //     mov  r8, r3
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,sel_hold,or,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x62170400,  //     mov  r23, r4
    0x55170010,  //     lsl  r23, #16
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x09050004,  //     cmp  r5, #0x04
    0x0d100005,  //     beq  #5
    0x407a0123,  //     setrh  r26, #0x0123
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x0d300004,  //     b  #4
    0x407a0133,  //     setrh  r26, #0x0133
    0x98000001,  //     dprc   mmac0=1
    0x98f40004,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=0,layer3-mode=1,addtree=l3
    0x62140200,  //     mov  r20, r2
    0x62120300,  //     mov  r18, r3
    0x09050003,  //     cmp  r5, #0x03
    0x0d100003,  //     beq  #3
    0x55120003,  //     lsl  r18, #3
    0x0d300002,  //     b  #2
    0x55120002,  //     lsl  r18, #2
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x09050004,  //     cmp  r5, #0x04
    0x0d100003,  //     beq  #3
    0xa4000011,  //     iow  bw=32bit,ch3=0,ch2=0,ch1=0,ch0=1
    0x0d300002,  //     b  #2
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};

__luna_cmd_attr__ uint32_t luna_api_vector_div[] = {
    0xe041d813,  //     rst  sel=reset,master0,slave0,pecore,regtab1,regtab2,regtab3,iowr,router
    0x28800080,  //     ldm  r0, {r0-r6}
    0x86103000,  //     memc   mode=1,access=1,grp0=1,grp1=1
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x89200009,  //     mnts  sel=io-rd0,inside=m0p0,outside=prd0
    0x88200001,  //     mnts  sel=mas0-1,p0-iodat=iord0
    0x41880000,  //     setrl  r8, #0x0000
    0x40688000,  //     setrh  r8, #0x8000
    0x88400006,  //     mnts  sel=mas0-2,p1-iodat=r8
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x8a000200,  //     mnts  sel=grp0,lmwr=s0_0
    0x8a200240,  //     mnts  sel=grp1,lmwr=s0_1
    0x62080300,  //     mov  r8, r3
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0008,  //     setrl  r10,#8
    0x620e0000,  //     mov  r14, r0
    0x40710800,  //     setrh  r17,#2048
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x620f0300,  //     mov  r15, r3
    0x550f0005,  //     lsl  r15, #5
    0x4b0f003f,  //     and  r15, #0x3f
    0x510f0003,  //     lsr  r15, #3
    0x550f000c,  //     lsl  r15, #12
    0x90003400,  //     ares  mode=master,sel=select3,sel_data_sour4
    0x40770000,  //     setrh  r23, #0
    0x4197a080,  //     setrl  r23, #0xa080
    0x41982202,  //     setrl  r24, #0x2202
    0x41990f0f,  //     setrl  r25, #0x0f0f
    0x40790f0f,  //     setrh  r25, #0x0f0f
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62120300,  //     mov  r18, r3
    0x45120001,  //     add  r18, #1
    0x51120001,  //     lsr  r18, #1
    0x55120002,  //     lsl  r18, #2
    0x40720000,  //     setrh  r18,#0
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750010,  //     setrh  r21,#0x10
    0x96002050,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=128bit,outband=128bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x86107000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1
    0x88200080,  //     mnts  sel=mas0-1,p0-lmdat0=grp0
    0x88400090,  //     mnts  sel=mas0-2,p1-lmdat0=grp1
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x8a400200,  //     mnts  sel=grp2,lmwr=s0_0
    0x62080300,  //     mov  r8, r3
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x40710000,  //     setrh  r17,#0
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418f0000,  //     setrl  r15,#0
    0x90003500,  //     ares  mode=master,sel=select3,sel_data_sour5
    0x4077001e,  //     setrh  r23, #30
    0x41970040,  //     setrl  r23, #0x0040
    0x41982202,  //     setrl  r24, #0x2202
    0x41990f0f,  //     setrl  r25, #0x0f0f
    0x40790f0f,  //     setrh  r25, #0x0f0f
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x41880000,  //     setrl  r8, #0x0000
    0x40680000,  //     setrh  r8, #0x0000
    0x41890000,  //     setrl  r9, #0x0000
    0x40692000,  //     setrh  r9, #0x2000
    0x98000001,  //     dprc   mmac0=1
    0x98e70124,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3,intgnet=l5,intgnet-mode=2
    0x62120300,  //     mov  r18, r3
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8610e000,  //     memc   mode=1,access=1,grp1=1,grp2=1,grp3=1
    0x88200090,  //     mnts  sel=mas0-1,p0-lmdat0=grp1
    0x884000a0,  //     mnts  sel=mas0-2,p1-lmdat0=grp2
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x8a200008,  //     mnts  sel=grp1,lmrd=m0p0_0
    0x8a400009,  //     mnts  sel=grp2,lmrd=m0p0_1
    0x8a600200,  //     mnts  sel=grp3,lmwr=s0_0
    0x62080300,  //     mov  r8, r3
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x40710000,  //     setrh  r17,#0
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418f0000,  //     setrl  r15,#0
    0x90003500,  //     ares  mode=master,sel=select3,sel_data_sour5
    0x4077001e,  //     setrh  r23, #30
    0x41970000,  //     setrl  r23, #0x0000
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62120300,  //     mov  r18, r3
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8610d000,  //     memc   mode=1,access=1,grp0=1,grp3=1,grp2=1
    0x88200080,  //     mnts  sel=mas0-1,p0-lmdat0=grp0
    0x884000b0,  //     mnts  sel=mas0-2,p1-lmdat0=grp3
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a600009,  //     mnts  sel=grp3,lmrd=m0p0_1
    0x8a400200,  //     mnts  sel=grp2,lmwr=s0_0
    0x62080300,  //     mov  r8, r3
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x40710000,  //     setrh  r17,#0
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418f0000,  //     setrl  r15,#0
    0x90003500,  //     ares  mode=master,sel=select3,sel_data_sour5
    0x4077001e,  //     setrh  r23, #30
    0x41970040,  //     setrl  r23, #0x0040
    0x41982202,  //     setrl  r24, #0x2202
    0x41990f0f,  //     setrl  r25, #0x0f0f
    0x40790f0f,  //     setrh  r25, #0x0f0f
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x41880000,  //     setrl  r8, #0x0000
    0x40680000,  //     setrh  r8, #0x0000
    0x41890000,  //     setrl  r9, #0x0000
    0x40692000,  //     setrh  r9, #0x2000
    0x98000001,  //     dprc   mmac0=1
    0x98e70124,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3,intgnet=l5,intgnet-mode=2
    0x62120300,  //     mov  r18, r3
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8610e000,  //     memc   mode=1,access=1,grp3=1,grp2=1,grp1=1
    0x882000b0,  //     mnts  sel=mas0-1,p0-lmdat0=grp3
    0x884000a0,  //     mnts  sel=mas0-2,p1-lmdat0=grp2
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x8a600008,  //     mnts  sel=grp3,lmrd=m0p0_0
    0x8a400009,  //     mnts  sel=grp2,lmrd=m0p0_1
    0x8a200200,  //     mnts  sel=grp1,lmwr=s0_0
    0x62080300,  //     mov  r8, r3
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x40710000,  //     setrh  r17,#0
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418f0000,  //     setrl  r15,#0
    0x90003500,  //     ares  mode=master,sel=select3,sel_data_sour5
    0x4077001e,  //     setrh  r23, #30
    0x41970000,  //     setrl  r23, #0x0000
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62120300,  //     mov  r18, r3
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x86107000,  //     memc   mode=1,access=1,grp0=1,grp1=1,grp2=1
    0x88200080,  //     mnts  sel=mas0-1,p0-lmdat0=grp0
    0x88400090,  //     mnts  sel=mas0-2,p1-lmdat0=grp1
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x8a000008,  //     mnts  sel=grp0,lmrd=m0p0_0
    0x8a200009,  //     mnts  sel=grp1,lmrd=m0p0_1
    0x8a400200,  //     mnts  sel=grp2,lmwr=s0_0
    0x62080300,  //     mov  r8, r3
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x40710000,  //     setrh  r17,#0
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418f0000,  //     setrl  r15,#0
    0x90003500,  //     ares  mode=master,sel=select3,sel_data_sour5
    0x4077001e,  //     setrh  r23, #30
    0x41970040,  //     setrl  r23, #0x0040
    0x41982202,  //     setrl  r24, #0x2202
    0x41990f0f,  //     setrl  r25, #0x0f0f
    0x40790f0f,  //     setrh  r25, #0x0f0f
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x41880000,  //     setrl  r8, #0x0000
    0x40680000,  //     setrh  r8, #0x0000
    0x41890000,  //     setrl  r9, #0x0000
    0x40692000,  //     setrh  r9, #0x2000
    0x98000001,  //     dprc   mmac0=1
    0x98e70124,  //     dprc   comshift=l4,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3,intgnet=l5,intgnet-mode=2
    0x62120300,  //     mov  r18, r3
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x8610e000,  //     memc   mode=1,access=1,grp1=1,grp2=1,grp3=1
    0x88200090,  //     mnts  sel=mas0-1,p0-lmdat0=grp1
    0x884000a0,  //     mnts  sel=mas0-2,p1-lmdat0=grp2
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x88c00007,  //     mnts  sel=slv0,din=pe
    0x8a200008,  //     mnts  sel=grp1,lmrd=m0p0_0
    0x8a400009,  //     mnts  sel=grp2,lmrd=m0p0_1
    0x8a600200,  //     mnts  sel=grp3,lmwr=s0_0
    0x62080300,  //     mov  r8, r3
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x40710000,  //     setrh  r17,#0
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418f0000,  //     setrl  r15,#0
    0x90003500,  //     ares  mode=master,sel=select3,sel_data_sour5
    0x4077001e,  //     setrh  r23, #30
    0x41970000,  //     setrl  r23, #0x0000
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62120300,  //     mov  r18, r3
    0x41930001,  //     setrl  r19,#1
    0x40730000,  //     setrh  r19,#0
    0x41940001,  //     setrl  r20,#1
    0x40740000,  //     setrh  r20,#0
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0x96002000,  //     dstm0  slave0,mode=normal,precision=2, slave0,inband=64bit,outband=64bit
    0xf0000001,  //     init  start-master0
    0x80000004,  //     wait  slave0
    0x86108000,  //     memc   mode=1,access=1,grp3=1
    0x8a600008,  //     mnts  sel=grp3,lmrd=m0p0_0
    0x882000b1,  //     mnts  sel=mas0-1,p0-iodat=iord0,p0-lmdat0=grp3
    0x89a00001,  //     mnts  sel=p-rd0,rd=iord0
    0x8920000b,  //     mnts  sel=io-rd0,inside=3,outside=prd0
    0x89c00002,  //     mnts  sel=p-rd1,rd=iord1
    0x89400012,  //     mnts  sel=io-rd1,inside=m0p1,outside=prd1
    0x88400002,  //     mnts  sel=mas0-2,p1-iodat=iord1
    0x89000021,  //     mnts  sel=pe,row=m0l,col=m0h
    0x89600007,  //     mnts  sel=io-wr,din=pe
    0x89e00003,  //     mnts  sel=p-wr,wr=iowr
    0x62080300,  //     mov  r8, r3
    0x45080001,  //     add  r8, #1
    0x51080001,  //     lsr  r8, #1
    0x62090800,  //     mov  r9, r8
    0x4b0800ff,  //     and  r8, #0xff
    0x51090008,  //     lsr  r9, #8
    0x418a0001,  //     setrl  r10,#1
    0x418e0000,  //     setrl  r14,#0
    0x406e0000,  //     setrh  r14,#0
    0x90020204,  //     ares  mode=master,sel=select0,chs[0010],sel_row0,cmc[0010],ces[00000100]
    0x90081002,  //     ares  mode=master,sel=select1,or,sel_hold,en_from_sour0,sel_en_line0,elf[0000],efcs[00000010]
    0x90002001,  //     ares  mode=master,sel=select2,sel_port0,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x418a0008,  //     setrl  r10,#8
    0x620e0100,  //     mov  r14, r1
    0x41910000,  //     setrl  r17,#0
    0x40711800,  //     setrh  r17,#6144
    0x90082001,  //     ares  mode=master,sel=select2,sel_port1,sel_low8,sel_master_over0,eac[0000],cas[00000001]
    0x620e0000,  //     mov  r14, r0
    0x90003600,  //     ares  mode=master,sel=select3,sel_data_sour6
    0x4188001d,  //     setrl  r8,  #29
    0x62170400,  //     mov  r23, r4
    0x55170010,  //     lsl  r23, #16
    0x4197a088,  //     setrl  r23, #0xa088
    0x41982202,  //     setrl  r24, #0x2202
    0x41990000,  //     setrl  r25, #0x0000
    0x40790000,  //     setrh  r25, #0x0000
    0x419ac022,  //     setrl  r26, #0xc022
    0x407a0023,  //     setrh  r26, #0x0023
    0x98000001,  //     dprc   mmac0=1
    0x98f40104,  //     dprc   comshift=l5,outlayer=l6,outlayer-mode=1,layer3-mode=1,addtree=l3
    0x62140200,  //     mov  r20, r2
    0x62120300,  //     mov  r18, r3
    0x55120002,  //     lsl  r18, #2
    0x41930001,  //     setrl  r19,#1
    0x41950000,  //     setrl  r21,#0
    0x40750000,  //     setrh  r21,#0
    0xa4000021,  //     iow  bw=64bit,ch3=0,ch2=0,ch1=0,ch0=1
    0xf0000001,  //     init  start-master0
    0x80000020,  //     wait  iow
    0x61000000,  //     mov  r0, #0
    0x0cf01e00,  //     bl  lr
};
