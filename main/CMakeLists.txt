idf_component_register(SRCS "main.c" "audio.c" "storage.c" "ulp_controller/ulp_controller.c"
                       INCLUDE_DIRS "." "./ulp_controller/."
                       REQUIRES esp-libhelix-mp3 fatfs soc nvs_flash ulp)

set(ulp_app_name ulp_${COMPONENT_NAME})
set(ulp_s_sources "ulp_controller/ulp_controller.S")
set(ulp_exp_dep_srcs "ulp_controller/ulp_controller.c")
ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")
