set(unit_file_parser_SRCS tokeniser.cpp)

add_library(unit_file_parser OBJECT ${unit_file_parser_SRCS})

set_public_target_object_vars(unit_file_parser Qt5::Core)