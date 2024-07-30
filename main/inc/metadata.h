    #ifndef METADATA_H
    #define METADATA_H

    #include "cJSON.h"
    extern cJSON *media_json;

    // Function to parse JSON response and store metadata
    void parse_and_store_metadata(const char *response_buffer, size_t response_buffer_size);

    #endif // METADATA_H
