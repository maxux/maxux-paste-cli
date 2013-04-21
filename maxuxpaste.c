#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#define PASTE_URL	"https://paste.maxux.net/api/post"
#define CURL_USERAGENT	"maxuxpaste/curl"

typedef struct paste_t {
	char *data;
	char *encoded;
	char *nick;
	char *lang;
	
	size_t size;
	int lines;
	
} paste_t;

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

int main(int argc, char *argv[]) {
	FILE *fp;
	char buffer[4096];
	size_t len;
	unsigned int nballoc = 1;
	CURL *curl;
	struct curl_slist *head = NULL;
	
	paste_t paste = {
		.data    = NULL,
		.encoded = NULL,
		.lang    = "text",
		.size    = 0,
		.lines   = 0,
	};
	
	/* Opening stdin */
	if(!(fp = fopen("/dev/stdin", "r")))
		diep("/dev/stdin");
	
	/* Allocating */
	paste.data = (char *) malloc(sizeof(char) * 10 * 1024);	/* 10 ko */
	strcpy(paste.data, "      ");	/* Write 'paste=' */
	
	/* Nick */
	if(!(paste.nick = getenv("USER"))) {
		fprintf(stderr, "[-] Paste: cannot get username\n");
		return 1;
	}
	
	/* Lang */
	if(argc >= 3) {
		if(!strcmp(argv[1], "-l"))
			paste.lang = argv[2];
	}
	
	printf("[+] Paste: nick: %s, language: %s\n", paste.nick, paste.lang);
	printf("----------8<--------------------------\n");
	
	while(fgets(buffer, sizeof(buffer), fp)) {
		printf("%s", buffer);
		
		paste.lines++;
		
		len = strlen(buffer);
		if(paste.size + len >= (10 * 1024) * nballoc)
			paste.data = (char*) realloc(paste.data, sizeof(char) * 10 * 1024 * ++nballoc);
			
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
	strncpy(paste.encoded, "paste=", 6);
	
	/* Append nick */
	
	// paste=<-- paste -->&nick=__NICK__
	//                 6b <----><-------> strlen(paste.nick)
	paste.encoded = realloc(paste.encoded, strlen(paste.encoded) + strlen(paste.lang) + strlen(paste.nick) + 32);
	sprintf(buffer, "&nick=%s&lang=%s", paste.nick, paste.lang);
	strcat(paste.encoded, buffer);
	
	/* Sending data */
	if(!(curl = curl_easy_init())) {
		fprintf(stderr, "[-] Paste: cannot init curl\n");
		return 1;
	}
	
	curl_easy_setopt(curl, CURLOPT_URL, PASTE_URL);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, paste.encoded);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE , strlen(paste.encoded));
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, CURL_USERAGENT);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
	/* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); */
	
	/* lighttpd fix */
	head = curl_slist_append(head, "Expect:");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head);
	
	/* let's doing it, curl will print the http answer to stdout */
	curl_easy_perform(curl);
		
	/* Cleaning */
	curl_easy_cleanup(curl);
	free(paste.encoded);
	free(paste.data);
	
	return 0;
}
