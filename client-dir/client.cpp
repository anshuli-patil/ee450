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
#include <cstring>

#define TCP_PORT "23299" // the port on edge that clients will be connecting to 
#define LOCALHOST "127.0.0.1"

#define MAXDATASIZE 1500 // max number of bytes we can get at once 

using namespace std;

// global variables
string orResultStr;
string andResultStr;

int getNextLine(ifstream &resultFile, int lineNumPrevious, string operatorType) {
  int lineNumber = -1;
  string value;

  if(lineNumPrevious == -1) {
      getline(resultFile, value, '*');
      value = value.substr(1, value.length()); // gets rid of CR/LF whatever chars

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

void combine_results() {
  fstream andfile("and_results_edge.txt");
  fstream orfile("or_results_edge.txt");
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
      //cout << orResultStr << " or" << endl;
      nextLineOr = -1;
    } else if((nextLineAnd != -2 && nextLineOr > nextLineAnd) || nextLineOr == -2){
      result << andResultStr << endl;
      //cout << andResultStr << " and" << endl;
      nextLineAnd = -1;
    }
  }

  andfile.close();
  orfile.close();
  result.close();

}

void *get_in_addr(struct sockaddr *addr) {
  if (addr->sa_family == AF_INET) {
    // IPv4
    return &((( struct sockaddr_in* )addr)->sin_addr);
  } else {
    // IPv6
    return &((( struct sockaddr_in6* )addr)->sin6_addr);
  }
}

void print_results() {
	string output;
    ifstream resultsfile("results.txt");

    if(resultsfile.is_open()) {

        while(!resultsfile.eof()) {
            getline(resultsfile, output);
            cout << output << endl;
        }
    }
}

int start_server(char *filename) {
	ifstream in(filename); // the queries file

	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(LOCALHOST, TCP_PORT, &hints, &servinfo)) != 0) {
		return 1;
	}

	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		return 2;
	} else {
		printf("The client is up and running.\n");
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

	freeaddrinfo(servinfo); 

	string value;
	int queriesCount = 0;
	string query;
	while(getline(in, value, '\n')) {
		value.append("\n");
		query.append(value);
		//cout << value << endl;
		if(value.length() >= 1) {
			queriesCount += 1;
		}
	}
	
	if (send(sockfd, &query, query.length() + 1, 0) == -1) {
		perror("ERROR : couldn't send query to edge server.");
	}
	printf("The client has successfully finished sending %d lines to the edge server.\n", queriesCount);
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

	// receiving AND Computations from edge server:
	ofstream result_and;
  	result_and.open("and_results_edge.txt");
	while ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) != -1) {
		//cout << numbytes << endl;
		if(numbytes == -1) {
		  return 1;
		}

		buf[numbytes] = '\0';
		/*
		if(numbytes < MAXDATASIZE - 1) {
			break;
		}*/
		string quit_check(buf);
		std::size_t found = quit_check.find('Q');
  		if (found != std::string::npos) {
  			result_and << quit_check.substr(0, found);
  			break;
  		} else {
			result_and << buf;
  		}
	}
	result_and.close();

	//receiving OR computations from edge server: 
	ofstream result_or;
  	result_or.open("or_results_edge.txt");
	while ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) != -1) {
		//cout << numbytes << endl;
		if(numbytes == -1) {
		  return 1;
		}

		/*
		if(numbytes < MAXDATASIZE - 1) {
			//printf("Q found! buf[0] = %c\n", buf[0]);
			break;
		} */

		buf[numbytes] = '\0';

		string quit_check(buf);
		std::size_t found = quit_check.find('Q');
  		if (found != std::string::npos) {
  			result_or << quit_check.substr(0, found);
  			break;
  		} else {
			result_or << buf;
  		}
	}
	result_or.close();
	printf("The client has successfully finished receiving all computation results from the edge server.\nThe final computation results are:\n");
	close(sockfd);

	//call combine_results and print final output
	combine_results();
	print_results();

	return 0;
}

int main(int argc, char *argv[]) {
	start_server(argv[1]);
}
