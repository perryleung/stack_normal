#ifndef _TOOLS_H_
#define _TOOLS_H_

/*
 * path : the dir you want to make
 * return : 0 for success
 *          -1 for fail
 *
 */
int mkdirfun(const char* path);

/*
 * return the num from [0, 100)
*/
int rand_num(int min = 0, int max = 100);

#endif
