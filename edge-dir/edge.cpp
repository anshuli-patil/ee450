#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define TCP_PORT "23299"  // the port users will be connecting to
#define UDP_PORT_AND "22299" // the port edge will use a client for the backend servers 
#define UDP_PORT_OR "21299" 
#define UDP_PORT_CLIENT "24299" // edge server uses a static port when it's a client for server_(and|or)
#define LOCALHOST "127.0.0.1"
#define MAXDATASIZE 100 
#define BACKLOG 10   
#define MAXBUFLEN 1500

using namespace std;

// global variables
int andLines, orLines;

void split_jobs() {
  fstream in("edge_file.txt");
  string value;
  int lineNumber = 0;

  ofstream andfile;
  andfile.open ("and.txt");
  ofstream orfile;
  orfile.open ("or.txt");

  while(in) {
    string operatorType = "";

    // if no more text, break out of loop
    if(!getline(in, value, ',')) {
      break;
    } else {
      operatorType = string(value, 0, value.length());

      // check if just newline before EOF
      if(operatorType.compare("\n") == 0) {
        break;
      }

      getline(in, value, ',');
      string operand1_str = string(value, 0, value.length());

      getline(in, value, '\n');
      string operand2_str = string(value, 0, value.length());

      if(operatorType.find("and") != std::string::npos) {
        andLines += 1;
        andfile << lineNumber << "," << operand1_str << "," << operand2_str << endl;
      } else if(operatorType.find("or") != std::string::npos) {
        orLines += 1;
        orfile << lineNumber << "," << operand1_str << "," << operand2_str << endl;
      } else {
        lineNumber -= 1;
      }

      lineNumber += 1; 
    }
  }

  printf("The edge server has received %d lines from the client using TCP over port %s.\n", lineNumber, TCP_PORT);
  andfile.close();
  orfile.close();
  in.close();
}

void sigchld_handler(int s)
{
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while(waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_response_backend(string backendType) {
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

  if ((rv = getaddrinfo(NULL, UDP_PORT_CLIENT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      continue;
    }

    break;
  }

  if (p == NULL) {
    return 2;
  }

  freeaddrinfo(servinfo);

  addr_len = sizeof their_addr;
  if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
    exit(1);
  }

  /*
  printf("listener: got packet from %s\n",
    inet_ntop(their_addr.ss_family,
      get_in_addr((struct sockaddr *)&their_addr),
      s, sizeof s));
  printf("listener: packet is %d bytes long\n", numbytes);
  */

  buf[numbytes] = '\0';
  //printf("listener: packet contains \"%s\"\n", buf);

  close(sockfd);

  ofstream requestfile;
  requestfile.open(backendType + "_results_edge.txt");
  requestfile << buf << endl;
  requestfile.close();

  return 0;
}

int send_queries_backend(string backendType) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  int linesCount = 0;

  ifstream requestfile;
  if(backendType.compare("and") == 0) {
    requestfile.open("and.txt");
  } else if(backendType.compare("or") == 0) {
    requestfile.open("or.txt");
  }

  string request_str;
  string value;
  while(requestfile) {
    if(!getline(requestfile, value, '\n')) {
      break;
    } else {
      request_str.append(string(value, 0, value.length()));
      request_str.append("\n");
    }
  }
  const char *request = request_str.c_str();
  requestfile.close();

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if(backendType.compare("and") == 0) {
    if ((rv = getaddrinfo(LOCALHOST, UDP_PORT_AND, &hints, &servinfo)) != 0) {
      return 1;
    }
  } else if(backendType.compare("or") == 0) {
      if ((rv = getaddrinfo(LOCALHOST, UDP_PORT_OR, &hints, &servinfo)) != 0) {
      return 1;
    }
  }

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

  if ((numbytes = sendto(sockfd, request, strlen(request), 0,
       p->ai_addr, p->ai_addrlen)) == -1) {
    exit(1);
  }

  freeaddrinfo(servinfo);

  //printf("talker: sent %d bytes to %s\n", numbytes, request);
  close(sockfd);
  return linesCount;
}

int send_queries_backend_bak(string backendType) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  int linesCount = 0;

  ifstream requestfile;
  if(backendType.compare("and") == 0) {
    requestfile.open("and.txt");
  } else if(backendType.compare("or") == 0) {
    requestfile.open("or.txt");
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if(backendType.compare("and") == 0) {
    if ((rv = getaddrinfo(LOCALHOST, UDP_PORT_AND, &hints, &servinfo)) != 0) {
      return 1;
    }
  } else if(backendType.compare("or") == 0) {
      if ((rv = getaddrinfo(LOCALHOST, UDP_PORT_OR, &hints, &servinfo)) != 0) {
      return 1;
    }
  }

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

  string value;

  while(requestfile) {
    if(!getline(requestfile, value, '\n')) {
        break;
      } else {
      value.append("\n");
      //cout << value << endl;
      if ((numbytes = sendto(sockfd, &value, value.length(), 0, p->ai_addr, p->ai_addrlen)) == -1) {
          perror("send");
      }
    }
  }
  freeaddrinfo(servinfo);

  //printf("talker: sent %d bytes to %s\n", numbytes, request);
  close(sockfd);
  return linesCount;
}
/*
string read_combined_results() {
  ifstream responsefile;
  responsefile.open("results.txt");
  
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
  
  responsefile.close();
  return response_str;
}
*/

int start_server() {
  int sockfd, new_fd;  
  int numbytes;  
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; 
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;
  int i;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; 

  if ((rv = getaddrinfo(NULL, TCP_PORT, &hints, &servinfo)) != 0) {
    return 1;
  }

  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
        sizeof(int)) == -1) {
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo); 

  if (p == NULL || listen(sockfd, BACKLOG) == -1)  {
    exit(1);
  }

  sa.sa_handler = sigchld_handler; 
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    exit(1);
  }

  printf("The edge server is up and running.\n");

  while(1) {  
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      continue;
    }

    inet_ntop(their_addr.ss_family,
      get_in_addr((struct sockaddr *)&their_addr),
      s, sizeof s);
    //printf("server: got connection from %s\n", s);

    if (!fork()) { 
      //child process

      close(sockfd); 
      ofstream queriesfile;
      queriesfile.open ("edge_file.txt");

      //cout << "Receiving data from client" << endl;
      while ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) != -1) {
        //cout << numbytes << endl;
        if(numbytes == -1) {
          return 1;
        }

        buf[numbytes] = '\0';
        //cout << "printing to file " << endl;
        //cout << buf;
        queriesfile << buf;

        if(numbytes < MAXDATASIZE - 1) {
          //cout << "Finished received queries" << endl;
          break;
        }
      }
      
      //queriesfile << buf << endl;
      
      queriesfile.close();

      orLines = 0;
      andLines = 0;
      split_jobs();
      /*
      pid_t and_proc, or_proc;
      and_proc = fork();

      if (and_proc == 0) {
          // AND Processing 
          send_queries_backend("and");
          printf("The edge has successfully sent %d lines to Backend-Server AND.\n", andLines);
          get_response_backend("and");
      } else {
          or_proc = fork();

          if (or_proc == 0) {
            // OR Processing
            send_queries_backend("or");
            printf("The edge has successfully sent %d lines to Backend-Server OR.\n", orLines);
            get_response_backend("or");
          } else {
            // Parent 
            wait(NULL);
          }
      }
      */
      send_queries_backend("and");
      printf("The edge has successfully sent %d lines to Backend-Server AND.\n", andLines);
      get_response_backend("and");

      send_queries_backend("or");
      printf("The edge has successfully sent %d lines to Backend-Server OR.\n", orLines);
      get_response_backend("or");

      printf("The edge server start receiving the computation results from Backend-Server OR and Backend-Server AND using UDP over port %s.\nThe computation results are:\n", UDP_PORT_CLIENT);
      // TODO print the lines received from AND and OR servers 

      printf("The edge server has successfully finished receiving all computation results from Backend-Server OR and Backend-Server AND.\n");
      
      /*
      string final_results = read_combined_results();
      const char *response = final_results.c_str();
      
      if (send(new_fd, response, final_results.length(), 0) == -1) {
        perror("send");
      }
      */

      string value;

      // send the computations for AND
      ifstream in_and("and_results_edge.txt");
      while(in_and) {
        if(!getline(in_and, value, '\n')) {
            break;
          } else {
          value.append("*");
          //cout << value << endl;
          if (send(new_fd, &value, value.length() + 1, 0) == -1) {
            perror("send");
          } 
        }
      }
      in_and.close();

      // done with sending AND computations
      value = "Q";
      if (send(new_fd, &value, value.length() + 1, 0) == -1) {
        perror("send");
      }

      // send the computations for OR
      ifstream in_or("or_results_edge.txt");
      while(in_or) {
        if(!getline(in_or, value, '\n')) {
            break;
          } else {
          value.append("*");
          //cout << value << endl;
          if (send(new_fd, &value, value.length() + 1, 0) == -1) {
            perror("send");
          } 
        }
      }
      in_or.close();

      // done with sending OR computations
      value = "Q";
      if (send(new_fd, &value, value.length() + 1, 0) == -1) {
        perror("send");
      }
      
      
      printf("The edge server has successfully finished sending all computation results to the client.\n");

      close(new_fd);
      exit(0);
    }
    //close(new_fd);  
  }

  return 0;
}

int main(void) {
  /*
  send_queries_backend("and");
  get_response_backend("and");
  */
  start_server();
  //split_jobs();
}
