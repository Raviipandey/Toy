idf_component_register(SRCS "./src/main.cpp"
                            "./src/wifi.cpp"
                            "./src/box_login.cpp"
                            "./src/download_master_json.cpp"
                            "./src/sdcard.cpp"
                            "./src/direction.cpp"
                            "./src/download_mp3.cpp"
                    INCLUDE_DIRS "./inc")

set(PARTITION_CSV_FILE ${CMAKE_CURRENT_LIST_DIR}/partitions.csv)