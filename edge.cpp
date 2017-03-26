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
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define TCP_PORT "23299"  // the port users will be connecting to
#define UDP_PORT_AND "22299" // the port edge will use a client for the backend servers 
#define UDP_PORT_OR "21299" 
#define LOCALHOST "127.0.0.1"
#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define BACKLOG 10   // how many pending connections queue will hold

using namespace std;

// global variables
string orResultStr;
string andResultStr;

int getNextLine(fstream &resultFile, int lineNumPrevious, string operatorType) {
  int lineNumber = -1;
  string value;

  if(lineNumPrevious == -1) {
      getline(resultFile, value, '\n');
      //cout << "[" << value << "]" << endl;
      if(resultFile && value.length() != 0 && value.find(',') != -1) { 
        string resultStr = string(value, value.find(',') + 1, value.length());
        //cout << operatorType << endl;
        if(operatorType.compare("and") == 0) {
          andResultStr = resultStr;
        } else if (operatorType.compare("or") == 0) {
          orResultStr = resultStr;
        }

        lineNumber = stoi(string(value, 0, value.find(',')));
      } else {
        // -2 indicates that the file has been completely read
        lineNumber = -2;
      }
  } else {
    lineNumber = lineNumPrevious;
  }
  
  return lineNumber;
}

void combineResults() {
  fstream andfile("and_results.txt");
  fstream orfile("or_results.txt");
  ofstream result;
  result.open ("results.txt");

  int lineNumber = 0;
  // -1 indicates next lineNumber in file isn't read yet
  int nextLineAnd = -1;
  int nextLineOr = -1;

  string value;

  // merge the two files using line numbers
  while(1) { 
    nextLineOr = getNextLine(orfile, nextLineOr, "or");
    nextLineAnd = getNextLine(andfile, nextLineAnd, "and");

    // terminate when both result files are done
    if(nextLineAnd == -2 && nextLineOr == -2) {
      break;
    }
    //cout << "nextLine " << nextLineOr << " " << nextLineAnd << endl;
    //cout << "nextLineStr " << orResultStr << " " << andResultStr << endl;
    if((nextLineOr != -2 && nextLineOr < nextLineAnd) || nextLineAnd == -2) {
      result << orResultStr << endl;
      cout << orResultStr << " or" << endl;
      nextLineOr = -1;
    } else if((nextLineAnd != -2 && nextLineOr > nextLineAnd) || nextLineOr == -2){
      result << andResultStr << endl;
      cout << andResultStr << " and" << endl;
      nextLineAnd = -1;
    }
  }

  andfile.close();
  orfile.close();
  result.close();

}

void splitJobs() {
  fstream in("edge_file.txt");
  string value;
  int lineNumber = 0;

  ofstream andfile;
  andfile.open ("and.txt");
  ofstream orfile;
  orfile.open ("or.txt");

  //cout << compute_or("101", "10010") << endl;

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

      if(operatorType.compare("and") == 0) {
        andfile << lineNumber << "," << operand1_str << "," << operand2_str << endl;
      } else if(operatorType.compare("or") == 0) {
        orfile << lineNumber << "," << operand1_str << "," << operand2_str << endl;
      } 
    }
    lineNumber += 1;

  }

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

void sendRequest(char *request, string port) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(LOCALHOST, port, &hints, &servinfo)) != 0) {
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

  if ((numbytes = sendto(sockfd, request, strlen(request), 0,
       p->ai_addr, p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  freeaddrinfo(servinfo);

  printf("talker: sent %d bytes to %s\n", numbytes, request);
  close(sockfd);
}

int main(void)
{
  int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
  int numbytes;  
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes=1;
  char s[INET6_ADDRSTRLEN];
  int rv;
  int i;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  // TODO connection using the port number assigned to client
  if ((rv = getaddrinfo(NULL, TCP_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
        sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL)  {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");

  while(1) {  // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family,
      get_in_addr((struct sockaddr *)&their_addr),
      s, sizeof s);
    printf("server: got connection from %s\n", s);

    if (!fork()) { // this is the child process
      
      close(sockfd); // child doesn't need the listener
      ofstream queriesfile;
      queriesfile.open ("edge_file.txt");

      if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
      }

      buf[numbytes] = '\0';
      printf("client: received '%s'\n",buf);

      queriesfile << buf << endl;
      splitJobs();

      //sendRequest();

      for(i = 0; i < 5; i++) {
        if (send(new_fd, ":-)", 3, 0) == -1) {
          perror("send");
        }
      }
      
      close(new_fd);
      queriesfile.close();
      exit(0);
    }
    close(new_fd);  // parent doesn't need this
  }

  return 0;
}
