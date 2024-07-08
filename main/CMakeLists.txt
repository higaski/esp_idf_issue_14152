idf_component_register(SRCS spiffs_example_main.cpp INCLUDE_DIRS .)

spiffs_create_partition_image(storage ${CMAKE_SOURCE_DIR}/spiffs
                              FLASH_IN_PROJECT)
