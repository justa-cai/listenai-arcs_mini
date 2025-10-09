if ACOMP_WSP
    config ACOMP_WSP_RES_ENCODER_ADDRESS
        hex "mlp encoder address"
        default 0xd200000
    config ACOMP_WSP_RES_ENCODER_LENGTH
        int "mlp encoder length"
        default 3546864
    config ACOMP_WSP_RES_DECODER_ADDRESS
        hex "mlp decoder address"
        default 0xcd00000
    config ACOMP_WSP_RES_DECODER_LENGTH
        int "mlp decoder length"
        default 419904
endif
