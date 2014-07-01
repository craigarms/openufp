/* openufp server
 *
 * author: Jeroen Nijhof
 * license: GPL v3.0
 *
 * blacklist.h: blacklist backend
 */

#define BLACKLIST_MAXSIZE 10000

extern int blacklist_load(const char *blacklist_file, char blacklist[BLACKLIST_MAXSIZE][URL_SIZE]);
extern int blacklist_backend(char blacklist[BLACKLIST_MAXSIZE][URL_SIZE], char url[URL_SIZE], int debug);

