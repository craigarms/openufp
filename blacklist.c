/* openufp server
 *
 * author: Jeroen Nijhof
 * license: GPL v3.0
 *
 * blacklist.c: blacklist backend
 */

#include "openufp.h"

int blacklist_load(const char *blacklist_file, char blacklist[BLACKLIST_MAXSIZE][URL_SIZE]) {
    char line[URL_SIZE];
    FILE *fd = NULL;
    int linenum = 0;
    int index = 0;

    fd = fopen(blacklist_file, "r");
    if (fd == NULL) {
        syslog(LOG_WARNING, "blacklist: could not open file %s.", blacklist_file);
        return -1;
    }

    while (fgets(line, URL_SIZE, fd) != NULL) {
        linenum++;
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%s", blacklist[index]) != 1) {
            syslog(LOG_WARNING, "blacklist: syntax error, skipping line %d.", linenum);
            continue;
        }
        index++;
        if (index == BLACKLIST_MAXSIZE) {
          syslog(LOG_WARNING, "blacklist: file too big.");
          return -1;
        }
    }
    fclose(fd);
    return 0;
}

int blacklist_backend(char blacklist[BLACKLIST_MAXSIZE][URL_SIZE], char url[URL_SIZE], int debug) {
    int index;

    for (index = 0; index < BLACKLIST_MAXSIZE && strlen(blacklist[index]) > 0; index++) {
        if (debug > 2)
            syslog(LOG_INFO, "blacklist: checking if url contains (%s).", blacklist[index]);
        if ((strstr(url, blacklist[index])) != NULL) {
            if (debug > 0)
                syslog(LOG_INFO, "blacklist: url blocked.");
            return 1;
        }
    }
    return 0;
}

