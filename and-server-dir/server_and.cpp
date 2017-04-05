#include <iostream>
#include <fstream>
#include <string>
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

#define UDP_PORT "22299" // the port that the server will listen on, for requests from edge server
#define UDP_PORT_EDGE "24299"
#define MAXBUFLEN 100
#define LOCALHOST "127.0.0.1"

using namespace std;

string append_zeroes(string operand, int numZeroes) {
  int i = 0;
  string zeroes = "";
  for(int i = 0; i < numZeroes; i++) {
    zeroes.append("0");
  }

  return zeroes.append(operand);
}

string ltrim_zeroes(string result) {
  int i = 0;
  bool is_leading = true;
  string final_result = "";
  for(int i = 0; i < result.length(); i++) {
    if(is_leading == true && result.at(i) == '0') {
      continue;
    } else {
      is_leading = false;
      final_result += result.at(i);
    }
  }
  if(final_result.length() == 0) {
    final_result += '0';
  }

  return final_result;
}

string compute_and(string operand1_str, string operand2_str) {
  string result = "";
  if(operand1_str.length() > operand2_str.length()) {
    operand2_str = append_zeroes(operand2_str, operand1_str.length() - operand2_str.length());
  } else {
    operand1_str = append_zeroes(operand1_str, operand2_str.length() - operand1_str.length());
  }

  int i = 0;
  for(i = 0; i < operand1_str.length(); i++) {
    if(operand1_str.at(i) == '1' && operand2_str.at(i) == '1') {
      result.append("1");
    } else {
      result.append("0");
    }
  }

  return ltrim_zeroes(result);

}

void compute_results() {
  fstream queries("and_temp.txt");

  ofstream andfile;
  andfile.open("and_results.txt");
  int countOperations = 0;
  string value;

   while(queries) {
    string lineNumber = "";

    // if no more text, break out of loop
    if(!getline(queries, value, ',')) {
      break;
    } else {
      countOperations += 1;
      lineNumber = string(value, 0, value.length());

      // check if just newline before EOF
      if(lineNumber.compare("\n") == 0) {
        break;
      }

      getline(queries, value, ',');
      string operand1_str = string(value, 0, value.length());

      getline(queries, value, '\n');
      string operand2_str = string(value, 0, value.length());

      string result = compute_and(operand1_str, operand2_str);
      // output to results file with line number
      andfile << lineNumber << "," << result << endl;
      //cout << lineNumber << "," << result << endl;
      cout << operand1_str << " and " << operand2_str << " = " << result << endl;

      //cout << operand1_str << " " <<operand2_str << endl;
      //cout << "------------------------------" << endl;
    }

  }

  printf("The Server AND has successfully received %d lines from the edge server and finished all AND computations.\n", countOperations - 1);

  andfile.close();
  queries.close();

}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int send_results() {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;

  ifstream responsefile;
  responsefile.open("and_results.txt");
  
  string response_str;
  string value;
  while(responsefile) {
    if(!getline(responsefile, value, '\n')) {
      break;
    } else {
      response_str.append(string(value, 0, value.length()));
      response_str.append("\n");
    }
  }
  const char *response = response_str.c_str();
  responsefile.close();

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(LOCALHOST, UDP_PORT_EDGE, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }
  
  // loop through all the results and make a socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
      perror("talker: socket");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "talker: failed to create socket\n");
    return 2;
  }

  if ((numbytes = sendto(sockfd, response, strlen(response), 0,
       p->ai_addr, p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  freeaddrinfo(servinfo);

  printf("The Server AND has successfully finished sending all computation results to the edge server.\n");
  //printf("talker: sent %d bytes to %s\n", numbytes, response);
  
  close(sockfd);
  return 0;
}

int start_server() {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  struct sockaddr_storage their_addr;
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, UDP_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
      perror("listener: socket");
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("listener: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "listener: failed to bind socket\n");
    return 2;
  }

  freeaddrinfo(servinfo);

  //printf("listener: waiting to recvfrom...\n");
  printf("The Server AND is up and running using UDP on port %s\n", UDP_PORT);

  addr_len = sizeof their_addr;


  while(true) {
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
      (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }

    printf("The Server AND start receiving lines from the edge server for AND computation.\nThe computation results are:\n");

    //printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';

    ofstream requestfile;
    requestfile.open("and_temp.txt");
    requestfile << buf << endl;
    requestfile.close();

    compute_results();
    send_results();
    
  }
  close(sockfd);

  return 0;
}

int main(void) {
  start_server();
}

