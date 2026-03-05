if (DEFINED CONFIG_AUDIO_CODEC)
    listenai_library_named(codec)

    # LC3 codec sources
    if (DEFINED CONFIG_AUDIO_CODEC_LC3)
        listenai_library_sources(
            lc3/adjust_global_gain_fx.c
            lc3/al_fec.c
            lc3/app_lc3.c
            lc3/apply_global_gain_fx.c
            lc3/ari_codec.c
            lc3/attack_detector_fx.c
            lc3/basop_mpy.c
            lc3/basop_util.c
            lc3/ccConvert.c
            lc3/codec_exe.c
            lc3/constants.c
            lc3/cutoff_bandwidth.c
            lc3/dct2_fx.c
            lc3/dct4_fx.c
            lc3/dec_entropy.c
            lc3/dec_lc3.c
            lc3/detect_cutoff_warped_fx.c
            lc3/enc_entropy.c
            lc3/enc_lc3.c
            lc3/estimate_global_gain_fx.c
            lc3/fft.c
            lc3/imdct_fx.c
            lc3/lc3.c
            lc3/levinson_fx.c
            lc3/ltpf_coder_fx.c
            lc3/ltpf_decoder_fx.c
            lc3/mdct_fx.c
            lc3/mdct_shaping_fx.c
            lc3/near_nyquist_detector_fx.c
            lc3/noise_factor_fx.c
            lc3/noise_filling_fx.c
            lc3/olpa_fx.c
            lc3/pc_apply_fx.c
            lc3/pc_classify_fx.c
            lc3/pc_main_fx.c
            lc3/pc_update_fx.c
            lc3/per_band_energy_fx.c
            lc3/plc_apply_fx.c
            lc3/plc_classify_fx.c
            lc3/plc_damping_scrambling_fx.c
            lc3/plc_lpc_scaling_fx.c
            lc3/plc_main_fx.c
            lc3/plc_noise_substitution_fx.c
            lc3/plc_phecu_f0_refine_first_fx.c
            lc3/plc_phecu_fec_hq_fx.c
            lc3/plc_phecu_lf_peak_analysis_fx.c
            lc3/plc_phecu_peak_locator_fx.c
            lc3/plc_phecu_setf0hz_fx.c
            lc3/plc_phecu_tools_fx.c
            lc3/plc_tdac_fx.c
            lc3/plc_tdc_inverse_odft_fx.c
            lc3/plc_tdc_lagwin_fx.c
            lc3/plc_tdc_main_fx.c
            lc3/plc_tdc_pre_emphasis_fx.c
            lc3/plc_update_aft_imdct_fx.c
            lc3/plc_update_fx.c
            lc3/plc_xcorr_fx.c
            lc3/pvq_dec_fx.c
            lc3/pvq_enc_fx.c
            lc3/pvq_index_fx.c
            lc3/quantize_spec_fx.c
            lc3/reorder_bitstream_fx.c
            lc3/resamp12k8_fx.c
            lc3/residual_coding_fx.c
            lc3/residual_decoding_fx.c
            lc3/rom_basop_util.c
            lc3/scale_signal24_fx.c
            lc3/setup_dec_lc3.c
            lc3/setup_enc_lc3.c
            lc3/sns_compute_scf_fx.c
            lc3/sns_interpolate_scf_fx.c
            lc3/sns_quantize_scf_fx.c
            lc3/tns_coder_fx.c
            lc3/tns_decoder_fx.c
            lc3/basic_op/basop32.c
            lc3/basic_op/control.c
            lc3/basic_op/count.c
            lc3/basic_op/dynmem.c
            lc3/basic_op/enh1632.c
            lc3/basic_op/enh40.c
            lc3/basic_op/enhUL32.c
        )
        target_include_directories(codec PRIVATE
            lc3
            lc3/basic_op
        )
    endif()

    # SBC codec sources
    if (DEFINED CONFIG_AUDIO_CODEC_SBC)
        listenai_library_sources(
            sbc/app_sbc.c
            sbc/sbc.c
            sbc/sbc_primitives.c
            sbc/sbc_primitives_armv6.c
            sbc/sbc_primitives_iwmmxt.c
            sbc/sbc_primitives_mmx.c
            sbc/sbc_primitives_neon.c
        )
        target_include_directories(codec PRIVATE
            sbc
        )
    endif()

    # mSBC codec sources
    if (DEFINED CONFIG_AUDIO_CODEC_MSBC)
        listenai_library_sources(
            msbc/app_msbc.c
            msbc/msbc.c
            msbc/msbc_primitives.c
            msbc/src.c
        )
        target_include_directories(codec PRIVATE
            msbc
        )
    endif()

    target_include_directories(codec PUBLIC
        api
    )
    target_include_directories(codec PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    listenai_include_directories(api)
endif()
