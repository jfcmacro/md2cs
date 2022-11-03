add_executable(md2cs main.cpp)

target_link_libraries(md2cs git2 pthread ssh2)
