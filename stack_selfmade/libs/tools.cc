#include "tools.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <string>

// #define FENGEFU ('/')
// #define R_OK (4) /* Test for read permission. */
// #define W_OK (2) /* Test for write permission. */
// #define X_OK (1) /* Test for execute permission. */
// #define F_OK (0) /* Test for existence. */

using std::vector;
using std::string;

/*
typedef enum {
  F_OK = 0,
  X_OK = 1,
  W_OK = 2,
  R_OK = 4
}FilePermission;
*/

typedef enum {
  HAVEFILE = 0,
  HAVENOFILE,
  CREATEDONE,
  CREATEFAIL
}MkdirResult;

const char FENGEFU = '/';

MkdirResult mkdirimpl(string path){
  if (access(path.c_str(), F_OK) == -1) {
    if (mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO) == 0) {
      return CREATEDONE;
    }else{
      return CREATEFAIL;
    }
  }
    return HAVEFILE;
}

int mkdirfun(const char* userpath)
{
  int start_pos, next_pos;
  bool START_FROM_ROOT = false;
  vector<string> path_element;
  string path(userpath);

  if(path.find_first_of(FENGEFU) == 0){
    START_FROM_ROOT = true;
    start_pos = 1;
  }else{
    START_FROM_ROOT = false;
    start_pos = 0;
  }

  if (path.rfind(FENGEFU) != path.size() - 1) {
    path += string("/");
  }


  while ((next_pos = path.find(FENGEFU, start_pos)) != string::npos) {
    path_element.push_back(path.substr(start_pos, next_pos - start_pos));
    start_pos = next_pos + 1;
    if (start_pos == path.size()) {
      break;
    }
  }

  string new_path;
  if (START_FROM_ROOT) {
    new_path = "/";
  }else{
    new_path = "";
  }
  for (vector<string>::iterator i = path_element.begin(); i != path_element.end(); i++) {
    new_path += (*i);
    new_path += "/";
    if(mkdirimpl(new_path) == CREATEFAIL){
      return -1;
    }
  }
  return 0;
}

int rand_num(int Min, int Max)
{
    if(Max == 0){
        return 0;
    }
    return Min + rand() % (Max - Min);
}
