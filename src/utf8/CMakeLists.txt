
set(utf8_SRCS utf8_reader.cpp)

add_library(utf8 OBJECT ${utf8_SRCS})

set_public_target_object_vars(utf8 Qt5::Core)