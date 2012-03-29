#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#define PASTE_URL	"http://paste.maxux.net/direct.php"
#define CURL_USERAGENT	"Maxux Paste CLI Client"

typedef struct paste_t {
	char *data;
	char *encoded;
	char *nick;
	
	size_t size;
	int lines;
	
} paste_t;

char to_hex(char code) {
	static char hex[] = "0123456789abcdef";
	return hex[code & 15];
}

char *url_encode(char *str) {
	char *pstr = str, *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
	
	while (*pstr) {
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

int main(void) {
	FILE *fp;
	char buffer[4096];
	
	paste_t paste;
	size_t len;
	unsigned int nballoc = 1;
	
	CURL *curl;
	CURLcode res;
	struct curl_slist *head = NULL;
	

	paste.data    = NULL;
	paste.encoded = NULL;
	paste.size    = 0;
	paste.lines   = 0;
	
	/* Opening stdin */
	fp = fopen("/dev/stdin", "r");
	if(!fp) {
		perror("/dev/stdin");
		return 1;
	}
	
	/* Allocating */
	paste.data = (char*) malloc(sizeof(char) * 10 * 1024);	/* 10 ko */
	strcpy(paste.data, "      ");	/* Write 'paste=' */
	
	/* Nick */
	if(!(paste.nick = getenv("USER"))) {
		fprintf(stderr, "[-] Paste: cannot get username\n");
		return 1;
	}
	
	printf("[+] Paste: init paste to paste.maxux.net\n");
	printf("[+] Paste: Nick: %s, Language: text\n", paste.nick);
	printf("----------8<--------------------------\n");
	
	while(fgets(buffer, sizeof(buffer), fp)) {
		/* Print output: transparent mode */
		printf("%s", buffer);
		
		paste.lines++;
		
		len = strlen(buffer);
		if(paste.size + len >= (10 * 1024) * nballoc)
			paste.data = (char*) realloc(paste.data, sizeof(char) * 10 * 1024 * ++nballoc);
			
		strcat(paste.data, buffer);
		
		paste.size += len;
	}
	
	fclose(fp);
	
	printf("-------------------------->8----------\n");
	printf("[+] Paste: %d lines read (%u bytes).\n", paste.lines, paste.size);
	printf("[+] Paste: Connecting to server...\n");
	
	paste.encoded = url_encode(paste.data);
	
	/* Override header */
	strncpy(paste.encoded, "paste=", 6);
	
	/* Append nick */
	
	// paste=<-- paste -->&nick=__NICK__
	//                 6b <----><-------> strlen(paste.nick)
	paste.encoded = realloc(paste.encoded, strlen(paste.encoded) + 6 + strlen(paste.nick) + 2);
	sprintf(buffer, "&nick=%s", paste.nick);
	strcat(paste.encoded, buffer);
	
	/* Sending data */
	curl = curl_easy_init();
	
	if(!curl) {
		fprintf(stderr, "[-] Paste: cannot init curl\n");
		return 1;
	}
	
	/* Setting URL */
	curl_easy_setopt(curl, CURLOPT_URL, PASTE_URL);
	
	/* Setting POST Data */
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, paste.encoded);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE , strlen(paste.encoded));
	
	/* Lighttpd fix */
	head = curl_slist_append(head, "Expect:");
	head = curl_slist_append(head, "Referer: http://paste.maxux.net/");
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head);
	
	/* Header/SSL Settings */
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, CURL_USERAGENT);
	
	/* Timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
	
	/* Debug */
	// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	
	/* Working */
	res = curl_easy_perform(curl);
	
	
	/* Cleaning */
	curl_easy_cleanup(curl);
	free(paste.encoded);
	free(paste.data);
	
	return 0;
}
