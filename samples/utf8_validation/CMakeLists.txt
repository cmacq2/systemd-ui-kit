set(utf8_valid_SRCS utf8_valid.cpp)

add_executable(utf8_valid ${utf8_valid_SRCS} $<TARGET_OBJECTS:utf8>)
target_link_libraries(utf8_valid Qt5::Core)