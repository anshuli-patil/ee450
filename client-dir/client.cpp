#include <iostream>
#include <fstream>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

#define TCP_PORT "23299" // the port on edge that clients will be connecting to 
#define LOCALHOST "127.0.0.1"

#define MAXDATASIZE 100 // max number of bytes we can get at once 

using namespace std;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int start_server(char *filename) {
	fstream in(filename); // the queries file

	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(LOCALHOST, TCP_PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); 
	string value;

	while(in) {
		if(!getline(in, value, '\n')) {
	      break;
	    } else {
			value.append("\n");
			cout << value << endl;
			if (send(sockfd, &value, value.length() + 1, 0) == -1) {
		        perror("send");
			}
		}
	}
	/*
	value = "Q";
	if (send(sockfd, &value, value.length() + 1, 0) == -1) {
        perror("send");
	}
	cout << "End of transmission " << value << endl;
	*/

	in.close();
	/*
	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	buf[numbytes] = '\0';

	printf("client: received '%s'\n",buf);
	*/

	cout << "Receiving data from server: " << endl;
	while ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) != -1) {
		//cout << numbytes << endl;
		if(numbytes == -1) {
		  return 1;
		}

		buf[numbytes] = '\0';
		//cout << "printing to file " << endl;
		if(buf[0] == 'Q') {
			break;
		}
		cout << buf;
		/*
		if(numbytes < MAXDATASIZE - 1) {
		  cout << "Finished received queries" << endl;
		  break;
		}*/
		if()
	}

	close(sockfd);

	return 0;
}

int main(int argc, char *argv[]) {
	start_server(argv[1]);
}
