#include <iostream>
#include <fstream>
#include <string>

#define UDP_PORT "21299" // the port that the server will listen on, for requests from edge server

using namespace std;

string append_zeroes(string operand, int numZeroes) {
  int i = 0;
  string zeroes = "";
  for(int i = 0; i < numZeroes; i++) {
    zeroes.append("0");
  }

  return zeroes.append(operand);
}

string compute_or(string operand1_str, string operand2_str) {
  string result = "";
  if(operand1_str.length() > operand2_str.length()) {
    operand2_str = append_zeroes(operand2_str, operand1_str.length() - operand2_str.length());
  } else {
    operand1_str = append_zeroes(operand1_str, operand2_str.length() - operand1_str.length());
  }

  int i = 0;
  for(i = 0; i < operand1_str.length(); i++) {
    if(operand1_str.at(i) == '1' || operand2_str.at(i) == '1') {
      result.append("1");
    } else {
      result.append("0");
    }
  }

  return result;
}

void computeResults() {
  fstream queries("or.txt");

  ofstream orfile;
  orfile.open ("or_results.txt");

  string value;

   while(queries) {
    string lineNumber = "";

    // if no more text, break out of loop
    if(!getline(queries, value, ',')) {
      break;
    } else {
      lineNumber = string(value, 0, value.length());

      // check if just newline before EOF
      if(lineNumber.compare("\n") == 0) {
        break;
      }

      getline(queries, value, ',');
      string operand1_str = string(value, 0, value.length());

      getline(queries, value, '\n');
      string operand2_str = string(value, 0, value.length());

      string result = compute_or(operand1_str, operand2_str);
      // output to results file with line number
      orfile << lineNumber << "," << result << endl;
      cout << lineNumber << "," << result << endl;
      //cout << operand1_str << " " <<operand2_str << endl;
      //cout << "------------------------------" << endl;
    }

  }

  orfile.close();
  queries.close();

}

int main(int argc, char* argv[]) {
  computeResults();
  return 0;
}
