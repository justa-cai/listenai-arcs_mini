#if defined(__cplusplus)
extern "C" {
#endif

#if CONFIG_LINK_CPP_RUNTIME_SECTIONS

// 定义缺失的__dso_handle符号
void* __dso_handle = 0;

#endif //CONFIG_LINK_CPP_RUNTIME_SECTIONS

#if defined(__cplusplus)
}
#endif

void cpp_init(void)
{
#if CONFIG_LINK_CPP_RUNTIME_SECTIONS
    extern void atexit(void (*function)(void));
    extern void __libc_init_array(void);
    extern void __libc_fini_array(void);

    atexit(__libc_fini_array);
    __libc_init_array();
#endif

#if CONFIG_CPP_EXCEPTIONS
    struct object {
        long placeholder[ 10 ];
    };
    void __register_frame_info(const void *begin, struct object * ob);
    extern char __eh_frame[];

    static struct object ob;
    __register_frame_info(__eh_frame, &ob);
#endif
}
