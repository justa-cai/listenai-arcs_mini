/* 模型 blob 以二进制嵌入固件（.incbin 不经过编译器，无大小限制）。
 * 路径相对于本 .S 文件所在目录。 */
    .section .rodata.llm_model, "a"
    .global llm_model_start
    .global llm_model_end
llm_model_start:
    .incbin "model.bin"
llm_model_end:

    .section .rodata.llm_tok, "a"
    .global llm_tok_start
    .global llm_tok_end
llm_tok_start:
    .incbin "tok.bin"
llm_tok_end:
