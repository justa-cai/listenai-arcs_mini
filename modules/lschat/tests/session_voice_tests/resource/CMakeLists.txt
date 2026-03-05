set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

generate_inc_file_for_target(app play_music.pcm ${gen_dir}/play_music.pcm.inc)
generate_inc_file_for_target(app last_music.pcm ${gen_dir}/last_music.pcm.inc)
generate_inc_file_for_target(app next_music.pcm ${gen_dir}/next_music.pcm.inc)
generate_inc_file_for_target(app resume_music.pcm ${gen_dir}/resume_music.pcm.inc)
generate_inc_file_for_target(app pause_music.pcm ${gen_dir}/pause_music.pcm.inc)
generate_inc_file_for_target(app weather.pcm ${gen_dir}/weather.pcm.inc)