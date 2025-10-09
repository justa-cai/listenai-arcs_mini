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
