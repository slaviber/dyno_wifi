idf_component_register(SRCS "dyno_main.c" "net.c" "data.c" "filter_tables.c" "pid.c" "init.c" "logic.c" "cJSON.c"
                    INCLUDE_DIRS ".")
#component_compile_options(-Wno-error=format= -Wno-format)
component_compile_options(-Wdouble-promotion -fsingle-precision-constant)
