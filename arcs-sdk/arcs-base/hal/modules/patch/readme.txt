
--version 1.0

原始代码中的使用：
1. 把patch_inf.ld内容放到原始代码的链接脚本里面
2. 调用patch_init进行初始化，参见patch.h
3，在需要patch的函数内，把PTCH宏放置在最前面，参见patch.h


补丁代码中的使用：
1. 根据原始代码的符号表文件，用sym_to_ld.py生成patch_sym.ld
   arm-none-eabi-nm -n -l -C org.elf > org.symbol  #生成symbol文件
   python sym_to_ld.py org.symbol patch_sym.ld
2. 根据原始代码的符号表文件，用sym_to_h.py生成patch_def.h
   arm-none-eabi-readelf -p .patch.entries org.elf | grep "^ " | sed 's/.*\] *//' > patch_funcs.lst  #生成所有可patch函数文件
   python sym_to_h.py org.symbol patch_funcs.lst patch_def.h
3. 补丁中需要实现patch_init接口，参见patch.h
4. 补丁中需要实现patch_func_ptr接口，参见patch.h
5. 补丁编译时使用patch.ld进行链接，patch.ld中包含了上面生成的patch_sym.ld文件