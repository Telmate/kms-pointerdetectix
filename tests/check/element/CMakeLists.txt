set (SUPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/../valgrind.supp")

 add_test_program (test_pointerdetectix pointerdetectix.c)
 add_dependencies(test_pointerdetectix pointerdetectix)
 target_include_directories(test_pointerdetectix PRIVATE
         ${KMSCORE_INCLUDE_DIRS}
                            ${GSTREAMER_INCLUDE_DIRS}
                            ${GSTREAMER_CHECK_INCLUDE_DIRS}
                            "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/gst-plugins")
 target_link_libraries(test_pointerdetectix
                       ${GSTREAMER_LIBRARIES}
                       ${GSTREAMER_CHECK_LIBRARIES}
                       kmstestutils)
