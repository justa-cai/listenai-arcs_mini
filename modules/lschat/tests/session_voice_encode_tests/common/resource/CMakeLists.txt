set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

generate_inc_file_for_target(app weather.pcm ${gen_dir}/weather.pcm.inc)