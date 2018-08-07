/* maxuxpaste is a command line remote paste for pipe Linux commands
 * and save it to https://paste.maxux.net service
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include <jansson.h>
#include <getopt.h>
#include "maxuxpaste.h"

static struct option lng_opts[] = {
    {"nick",    required_argument, 0, 'n'},
    {"lang",    required_argument, 0, 'l'},
    {"private", no_argument,       0, 'p'},
    {"help",    no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

void diep(char *str) {
    perror(str);
    exit(EXIT_FAILURE);
}

char to_hex(char code) {
    static char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

char *url_encode(char *str) {
    char *pstr = str;
    char *buf = (char *) malloc((strlen(str) * 3) + 1);
    char *pbuf = buf;

    while(*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
            *pbuf++ = *pstr;

        else if (*pstr == ' ')
            *pbuf++ = '+';

        else
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);

        pstr++;
    }

    *pbuf = '\0';

    return buf;
}

void answer(char *json) {
    json_t *root, *node;
    json_error_t error;

    if(!(root = json_loads(json, 0, &error))) {
        fprintf(stderr, "[-] json errors\n");
        return;
    }

    if(!json_is_object(root)) {
        fprintf(stderr, "[-] json error: not an object\n");
        return;
    }

    node = json_object_get(root, "url");
    if(!json_is_string(node)) {
        node = json_object_get(root, "error");

        if(!json_is_string(node)) {
            fprintf(stderr, "[-] cannot parse json response\n");
            return;

        } else printf("[-] paste: %s\n", json_string_value(node));

    } else printf("[+] paste: %s\n", json_string_value(node));

    json_decref(root);
}

size_t curl_body(char *ptr, size_t size, size_t nmemb, void *userdata) {
    curl_data_t *curl = (curl_data_t*) userdata;
    size_t prev;

    prev = curl->length;
    curl->length += (size * nmemb);

    /* Resize data */
    curl->data  = (char *) realloc(curl->data, (curl->length + 1));

    /* Appending data */
    memcpy(curl->data + prev, ptr, size * nmemb);

    /* Return required by libcurl */
    return size * nmemb;
}

char *paste_send(paste_t *paste) {
    CURL *curl;
    struct curl_slist *head = NULL;
    curl_data_t curldata = {
        .data   = NULL,
        .length = 0,
    };

    /* Sending data */
    if(!(curl = curl_easy_init())) {
        fprintf(stderr, "[-] paste: cannot init curl\n");
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, CURL_API_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, paste->encoded);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE , strlen(paste->encoded));
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, CURL_USERAGENT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curldata);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_body);
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); */

    /* lighttpd fix */
    head = curl_slist_append(head, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head);

    /* let's doing it, curl will print the http answer to stdout */
    curl_easy_perform(curl);

    if(curldata.data)
        curldata.data[curldata.length] = '\0';

    /* Cleaning */
    curl_easy_cleanup(curl);

    return curldata.data;
}

void print_usage(char *argv0) {
    fprintf(stderr, "%s: options\n", argv0);
    fprintf(stderr, " --nick, -n    : nickname for paste\n");
    fprintf(stderr, " --lang, -l    : lang of the paste (default: text)\n");
    fprintf(stderr, " --private, -p : set paste private\n");
    fprintf(stderr, " --help, -h    : this message\n");
}

int main(int argc, char *argv[]) {
    FILE *fp;
    char buffer[4096];
    size_t len;
    char *reply;
    int i, oi = 0;
    paste_t paste = {
        .data    = NULL,
        .nick    = NULL,
        .encoded = NULL,
        .lang    = "text",
        .size    = 0,
        .lines   = 0,
        .private = 0,
    };

    /* parsing options */
    while(1) {
        if((i = getopt_long(argc, argv, "n:l:ph", lng_opts, &oi)) == -1)
            break;

        switch(i) {
            case 'n':
                paste.nick = optarg;
            break;
            case 'l':
                paste.lang = optarg;
            break;
            case 'p':
                paste.private = 1;
            break;

            case 'h':
            case '?':
                print_usage(argv[0]);
                return 1;
            break;

            default:
                abort();
        }
    }

    /* opening stdin */
    if(!(fp = fopen("/dev/stdin", "r")))
        diep("/dev/stdin");

    /* allocating */
    paste.data = (char *) malloc(sizeof(char) * MEMORY_ALLOC);
    strcpy(paste.data, "     "); /* Write 'code=' */

    /* if nick not set on arguments */
    if(!paste.nick) {
        /* default nick from environment */
        if(!(paste.nick = getenv("PASTENICK"))) {
            /* fallback */
            if(!(paste.nick = getenv("USER"))) {
                fprintf(stderr, "[-] Paste: cannot get username\n");
                paste.nick = "curlnick";
            }
        }
    }

    // unbuffered stdout
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("[+] Paste: nick: %s, language: %s, private: %d\n", paste.nick, paste.lang, paste.private);
    printf("----------8<--------------------------\n");

    while(fgets(buffer, sizeof(buffer), fp)) {
        printf("%s", buffer);

        paste.lines++;
        len = strlen(buffer);

        if(paste.size + len + 1 > MEMORY_ALLOC) {
            printf("[-] Paste: %u bytes exceed, stopping here.\n", MEMORY_ALLOC);
            break;
        }

        strcat(paste.data, buffer);

        paste.size += len;
    }

    fclose(fp);

    printf("----------8<--------------------------\n");
    printf("[+] Paste: sending %d lines, %u bytes\n", paste.lines, paste.size);

    /*
     * FIXME: fucking crappy part
     */
    paste.encoded = url_encode(paste.data);

    /* Override header */
    strncpy(paste.encoded, "code=", 5);

    /* Append nick */

    // paste=<-- paste -->&nick=__NICK__
    //                 6b <----><-------> strlen(paste.nick)
    paste.encoded = realloc(paste.encoded, strlen(paste.encoded) + strlen(paste.lang) + strlen(paste.nick) + 32);
    sprintf(buffer, "&nick=%s&lang=%s&private=%d", paste.nick, paste.lang, paste.private);
    strcat(paste.encoded, buffer);

    if((reply = paste_send(&paste)))
        answer(reply);

    else fprintf(stderr, "[-] Paste: wrong answer from server\n");

    /* cleaning */
    free(reply);
    free(paste.encoded);
    free(paste.data);

    return 0;
}
