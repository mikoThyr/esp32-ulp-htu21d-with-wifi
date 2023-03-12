idf_component_register(	SRCS    "main.c"
                                "wifi.c"
                                "http.c"
						SRC_DIRS "ulp"
						INCLUDE_DIRS "../include")

set(ulp_app_name ulp_${COMPONENT_NAME})
set(ulp_s_sources 	"ulp/ulp_main_file.S"
			        "ulp/macro.S"
                    "ulp/i2c_label.S")
set(ulp_exp_dep_srcs "main.c")

ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")



