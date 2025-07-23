# This CMake file will automatically add an executable for every .cpp file in this folder
# where each file is expected to have its own main() function.

file(GLOB TEST_SOURCES "*.cpp")

foreach(test_src ${TEST_SOURCES})

    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    
    include_directories($ENV{PWD})
    include_directories($ENV{PWD}/include)
    
    target_include_directories(${test_name} PRIVATE ${PQXX_INCLUDE_DIRS})
    target_link_directories(${test_name} PRIVATE ${PQXX_LIBRARY_DIRS})
    target_link_libraries(${test_name} PRIVATE ${PQXX_LIBRARIES} pq pthread)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()