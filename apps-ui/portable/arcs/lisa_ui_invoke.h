#ifndef __LISA_UI_INVOKE_H__
#define __LISA_UI_INVOKE_H__

int lisa_ui_invoke_init(void);
int lisa_ui_invoke_ui_delayed(void (*worker)(void *, uint32_t), void *arg, uint32_t len, uint32_t delay_ms);

#define LISA_UI_INVOKE_UI(_f, _a, _l)             lisa_ui_invoke_ui_delayed(_f, _a, _l, 0)
#define LISA_UI_INVOKE_UI_DELAYED(_f, _a, _l, _d) lisa_ui_invoke_ui_delayed(_f, _a, _l, _d)

#define LISA_UI_INVOKE_BN(_f, _a, _l)             lisa_ui_invoke_bn_delayed(_f, _a, _l, 0)
// #define LISA_UI_INVOKE_BN_DELAYED(_f, _a, _l, _d) lisa_ui_invoke_bn_delayed(_f, _a, _l, _d)

#define LISA_INVOKE_NO_ARG_IMPL(__invoke_func, __code_block_) \
    do { \
        void __invoke_wrapper(void *arg, uint32_t len) { \
            __code_block_ \
        } \
        __invoke_func(__invoke_wrapper, NULL, 0, 0); \
    } while (0)

#define LISA_INVOKE_BASE_TYPE_IMPL(__invoke_func, _a, __code_block_) \
    do { \
        typeof(_a) __arg_copy = (_a); \
        void __invoke_wrapper(void *arg, uint32_t len) { \
            typeof(_a) _invoke_##_a = *(typeof(_a) *)arg; \
            __code_block_ \
        } \
        __invoke_func(__invoke_wrapper, &__arg_copy, sizeof(__arg_copy), 0); \
    } while (0)

#define LISA_INVOKE_PTR_TYPE_IMPL(__invoke_func, _a, _l, __code_block_) \
    do { \
        void __invoke_wrapper(void *arg, uint32_t _invoke_len) { \
            typeof(_a) _invoke_##_a = (typeof(_a))arg; \
            __code_block_ \
        } \
        __invoke_func(__invoke_wrapper, _a, _l, 0); \
    } while (0)

/* 用于将代码块交换到业务线程执行, 无传递参数 */
#define LISA_UI_INVOKE_BN_ARG_NONE(__code_block_) \
    LISA_INVOKE_NO_ARG_IMPL(lisa_ui_invoke_bn_delayed, __code_block_)

/* 用于将代码块交换到业务线程执行, 传递一个基础类型参数 */
#define LISA_UI_INVOKE_BN_ARG_BASE(_a, __code_block_) \
    LISA_INVOKE_BASE_TYPE_IMPL(lisa_ui_invoke_bn_delayed, _a, __code_block_)

/* 用于将代码块交换到业务线程执行, 传递一个指针以及长度, 数据指针的数据将被浅拷贝 */
#define LISA_UI_INVOKE_BN_ARG_PTR(_a, _l, __code_block_) \
    LISA_INVOKE_PTR_TYPE_IMPL(lisa_ui_invoke_bn_delayed, _a, _l, __code_block_)

/* 用于将代码块交换到UI执行, 无传递参数 */
#define LISA_UI_INVOKE_UI_ARG_NONE(__code_block_) \
    LISA_INVOKE_NO_ARG_IMPL(lisa_ui_invoke_ui_delayed, __code_block_)

/* 用于将代码块交换到UI线程执行, 传递一个基础类型参数 */
#define LISA_UI_INVOKE_UI_ARG_BASE(_a, __code_block_) \
    LISA_INVOKE_BASE_TYPE_IMPL(lisa_ui_invoke_ui_delayed, _a, __code_block_)

/* 用于将代码块交换到UI线程执行, 传递一个指针以及长度, 数据指针的数据将被浅拷贝 */
#define LISA_UI_INVOKE_UI_ARG_PTR(_a, _l, __code_block_) \
    LISA_INVOKE_PTR_TYPE_IMPL(lisa_ui_invoke_ui_delayed, _a, _l, __code_block_)

#endif
