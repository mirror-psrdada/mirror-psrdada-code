
set(tests
    test_ascii_header.c
    test_command_parser.c
    test_disk_array.c
    test_ipcbuf.c
    test_median_smooth_zap.c
    test_multilog.c
    test_uint64_t.c
    test_ipcio.c
    test_ipcio_lifecycle.c
)

foreach(test_source_file IN LISTS tests)
    string(REPLACE ".c" "" test_name ${test_source_file})
    add_executable(${test_name} ${test_source_file})
    target_link_libraries(${test_name} psrdada)
    add_test(${test_name} ${test_name})
endforeach(test_source_file IN LISTS tests)

set(manualtests
    test_command_parse_server.c
    test_dada_nexus.c
    test_multilog_server.c
)

foreach(test_source_file IN LISTS manualtests)
    string(REPLACE ".c" "" test_name ${test_source_file})
    add_executable(${test_name} ${test_source_file})
    target_link_libraries(${test_name} psrdada)
endforeach(test_source_file IN LISTS manualtests)

