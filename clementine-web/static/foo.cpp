#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <string.h>

using namespace std;


vector<string> Split(string s, char delim) {
  stringstream stream(s);
  vector<string> split;
  string item;
  while (std::getline(stream, item, delim)) {
    split.push_back(item);
    cout << item << endl;
  }

  return split;
}

int main(int argc, char** argv) {

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "Range: bytes=%d-%d", 1234, 12345);

  cout << buffer << endl;
  cout << strlen(buffer) << endl;

  return 0;
}
