/*
** talker.c -- a datagram "client" demo
* (c) Beej's N/w Prg Guide
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVERPORT 6100	// the port users will be connecting to

int main(int argc, char *argv[])
{
	int sockfd;
//	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in dest_addr;
	int numbytes;

	if (argc != 4) {
		fprintf(stderr,"usage: %s interface-to-bind-to DEST-IP-address message\n", argv[0]);
		exit(1);
	}

  if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ) { 
        perror("socket");
        exit(1);
    }   

	// Should be running as root!
    if (setsockopt (sockfd, SOL_SOCKET, SO_BINDTODEVICE, argv[1], strlen(argv[1])+1) < 0) {
        printf("\n%s:setsockopt failed...\n",argv[0]);
        close(sockfd);
        exit(1);
    }
    printf ("%s: successfully bound to interface '%s'\n", argv[0], argv[1]);

    dest_addr.sin_family = AF_INET; // host byte order
    dest_addr.sin_port = htons(SERVERPORT); // short, network byte order
    dest_addr.sin_addr.s_addr = inet_addr(argv[2]); // dest IP
    memset(&(dest_addr.sin_zero), '\0', 8); 

	printf ("Sending message over datagram socket to %s:%d now...\n", 
		argv[2], SERVERPORT);
	if ((numbytes = sendto(sockfd, argv[3], strlen(argv[3]), 0,
			 (struct sockaddr *)&dest_addr, sizeof(dest_addr))) == -1) {
		perror("talker: sendto");
		exit(1);
	}

	printf("talker: sent %d bytes.\n", numbytes);
	close(sockfd);

	return 0;
}
