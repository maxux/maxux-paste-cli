#ifndef __MAXUX_PASTE_H
    #define __MAXUX_PASTE_H

    typedef struct paste_t {
        char *data;
        char *encoded;
        char *nick;
        char *lang;
        char private;

        size_t size;
        int lines;

    } paste_t;

    typedef struct curl_data_t {
        char *data;
        unsigned int length;
        long code;
        CURLcode curlcode;

    } curl_data_t;

    // #define CURL_API_URL    "https://paste.maxux.net/api/paste"
    #define CURL_API_URL    "http://paste.maxux.net/api/paste"
    #define CURL_USERAGENT  "maxuxpaste/curl"

    #define MEMORY_ALLOC    1024 * 1024 // 1 Mo
#endif
