/* openufp server
 *
 * author: Jeroen Nijhof
 * forked by Craig Armstrong to adapt to docker
 * license: GPL v3.0
 *
 * This server translates n2h2 or websense requests to different backends.
 * Frontends supported: n2h2, websense
 * Backends supported: proxy
 * 
 * Proxy: this backend will recieve url get requests from this server and
 *        when the proxy server response contains the PROXY_DENY_PATTERN
 *        a n2h2 or websense deny response will be sent and if not an allow response.
 *
 */

#include "openufp.h"

// Helper functions
void usage() {
    printf("\nUsage: openufp [OPTIONS] <-n|-w> <BACKEND>\n");
    printf("Example: openufp -n -p '192.168.1.10:3128:Access Denied.'\n");
    printf("Example: openufp -n -f blacklist -p '192.168.1.10:3128:Access Denied.'\n");
    printf("Example: openufp -C http://www.test.com\n\n");
    printf("OPTIONS:\n");
    printf("   -l PORT   on which port openufp will listen for incoming requests\n");
    printf("   -r URL    when url is denied the client will be redirected to this url; n2h2 only\n");
    printf("   -c SECS   cache expire time in seconds; default 3600; 0 disables caching\n");
    printf("   -C URL    remove specified URL from cache\n");
    printf("   -d LEVEL  debug level 1-3\n");
    printf("   -F        run in foreground, don't fork main process\n\n");
    printf("FRONTEND:\n");
    printf("   -n        act as n2h2 server\n");
    printf("   -w        act as websense server\n");
    printf("BACKEND:\n");
    printf("   -p IP:PORT:DENY_PATTERN   use the proxy backend\n");
    printf("             IP is the ipnumber of the proxy server\n");
    printf("             PORT is the portnumber where the proxy server is listening on\n");
    printf("             DENY_PATTERN is a piece of text that should match the deny page\n");
    printf("   -f FILE   use the blacklist file backend\n");
    printf("             FILE is a file which contains blacklisted urls\n");
    printf("   -g        use the squidGuard backend\n\n");
    printf("NOTE:\n");
    printf("   The default location of the cache db is /var/cache/openufp/cache.db.\n");
    printf("   When squidguard backend is used be sure that this program has rw permissions\n");
    printf("   to the squidguard db files.\n\n");
    printf("Version: %s\n", VERSION);
    printf("Report bugs to: jeroen@jeroennijhof.nl\n\n");
    printf("Look at the differences with the original version on github.com/craigarms/openufp\n\n");
}

// Main function
int main(int argc, char**argv) {
    int openufp_fd;
    pid_t pid, child_pid;
    struct sockaddr_in openufp_addr;
    int local_port = 0;
    char *redirect_url = NULL;
    char sg_redirect[URL_SIZE];
    int cache_exp_secs = 3600;
    int debug = 0;
    int frontend = 0;
    char *proxy_ip = NULL;
    int proxy_port = 0;
    char *proxy_deny_pattern = NULL;
    char *blacklist = NULL;
    int squidguard = 0;
    int foreground = 0;
    int c;
    char *https = "https://";

    while ((c = getopt(argc, argv, "l:r:c:C:d:nwp:f:guF")) != -1) {
        char *p;
        char hash[10];
        DB *cachedb;
        int ret = 0;
        switch(c) {
            case 'l':
                local_port = atoi(optarg);
                break;
            case 'r':
                redirect_url = optarg;
                break;
            case 'c':
                cache_exp_secs = atoi(optarg);
                break;
            case 'C':
                cachedb = open_cache();
                get_hash(optarg, hash);
                if (rm_cache(cachedb, hash, 255) == -1)
                    ret = 1;
                close_cache(cachedb, 0);
                exit(ret);
                break;
            case 'd':
                debug = atoi(optarg);
                break;
            case 'n':
                frontend = N2H2;
                break;
            case 'w':
                frontend = WEBSNS;
                break;
            case 'p':
                p = strtok(optarg, ":");
                if (p == NULL)
                    break;
                proxy_ip = p;
                p = strtok (NULL, ":");
                if (p == NULL)
                    break;
                proxy_port = atoi(p);
                p = strtok (NULL, ":");
                if (p == NULL)
                    break;
                proxy_deny_pattern = p;
                break;
            case 'f':
                blacklist = optarg;
                break;
            case 'g':
                squidguard = 1;
                break;
            case 'F':
                foreground = 1;
                break;
            default:
                usage();
                exit(1);
        }
    }
    if (frontend == 0 || ((proxy_ip == NULL || proxy_port == 0 || proxy_deny_pattern == NULL)
                    && blacklist == NULL && squidguard == 0)) {
        usage();
        exit(1);
    }

    // SIGCHLD handler for stupid childs
    signal(SIGCHLD, SIG_IGN);

    openufp_fd = socket(AF_INET,SOCK_STREAM,0);
    if (openufp_fd < 0) {
        close(openufp_fd);
        printf("openufp v%s: socket failed.\n", VERSION);
        exit(1);
    }

    int optval = 1;
    if (setsockopt(openufp_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        close(openufp_fd);
        printf("openufp v%s: setsockopt failed.\n", VERSION);
        exit(1);
    }

    bzero(&openufp_addr, sizeof(openufp_addr));
    openufp_addr.sin_family = AF_INET;
    openufp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (local_port == 0) {
        if (frontend == N2H2) {
            local_port = 4005;
        } else {
            local_port = 15868;
        }
    }
    openufp_addr.sin_port = htons(local_port);

    if (bind(openufp_fd, (struct sockaddr *)&openufp_addr, sizeof(openufp_addr)) < 0) {
        close(openufp_fd);
        printf("openufp v%s: bind failed.\n", VERSION);
        exit(1);
    }

    if (listen(openufp_fd, 1024) < 0) {
        close(openufp_fd);
        printf("openufp v%s: listen failed.\n", VERSION);
        exit(1);
    }

    printf("openufp v%s: started.\n", VERSION);
    openlog("openufp", LOG_PID|LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "v%s: Forked from Jeroen Nijhof <jeroen@jeroennijhof.nl> v1.09 by Craig Armstrong", VERSION);
    syslog(LOG_INFO, "started listening on %d, waiting for requests...", local_port); 

    if(foreground == 0){
        pid = fork();
    }

    if (pid == 0 || foreground == 1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_size;
        int cli_fd;

        for(;;) {
            cli_size = sizeof(cli_addr);
            cli_fd = accept(openufp_fd, (struct sockaddr *)&cli_addr, &cli_size);
            syslog(LOG_INFO, "client connection accepted.");

            if ((child_pid = fork()) == 0) {
                close(openufp_fd);
                int msgsize = 0;
                int denied = 0;
                char msg[REQ_SIZE];
                struct uf_request request;

                DB *cachedb = NULL;
                if (cache_exp_secs > 0)
                    cachedb = open_cache();
                else
                    syslog(LOG_INFO, "caching disabled.");

                int cached = 0;
                char hash[10];
                struct n2h2_req *n2h2_request = NULL;
                struct websns_req *websns_request = NULL;
                for(;;) {
                    bzero(&msg, sizeof(msg));
                    msgsize = recvfrom(cli_fd, msg, REQ_SIZE, 0, (struct sockaddr *)&cli_addr, &cli_size);
                    if (msgsize < 1) {
                        syslog(LOG_WARNING, "connection closed by client.");
                        close_cache(cachedb, debug);
                        close(cli_fd);
                        exit(1);
                    }

                    // Validate request
                    if (frontend == N2H2) {
                        n2h2_request = (struct n2h2_req *)msg;
                        request = n2h2_validate(n2h2_request, msgsize);
                    } else {
                        websns_request = (struct websns_req *)msg;

			//secret debug
                        if(debug > 3)
			{
		        	syslog(LOG_INFO, "Websense debug request output: size %d, vers_maj %d, vers_min %d, vers_pat %d, serial %d, code %d, desc %d, srcip %d, dstip %d, urlsize %d, url %s",
                                                 websns_request->size, websns_request->vers_maj, websns_request->vers_min, websns_request->vers_pat, websns_request->serial, websns_request->code, websns_request->desc, websns_request->srcip, websns_request->dstip, websns_request->urlsize, websns_request->url);
			}
                        websns_convert(websns_request, msg, msgsize, debug);
                        request = websns_validate(websns_request, msgsize);

                    }
                    if (request.type == UNKNOWN) {
                        syslog(LOG_WARNING, "request type not known, closing connecion.");
                        close_cache(cachedb, debug);
                        close(cli_fd);
                        exit(1);
                    }

                    // Alive request
                    if (request.type == N2H2_ALIVE) {
                        if (debug > 2)
                            syslog(LOG_INFO, "n2h2: received alive request, sending alive response.");
                        n2h2_alive(cli_fd, n2h2_request);
                    }
                    if (request.type == WEBSNS_ALIVE) {
                        if (debug > 2)
                            syslog(LOG_INFO, "websns: received alive request, sending alive response.");
                        websns_alive(cli_fd, websns_request);
                    }

                    // URL request
                    if (request.type == N2H2_REQ || request.type == WEBSNS_REQ) {
                        if (debug > 0) {
                            syslog(LOG_INFO, "received url request - Original URL: %s", request.url);
			}

			// Handle HTTPS for N2H2 only since IP is provided in URI:
			if (strstr(https, request.url) != NULL && request.type == N2H2_REQ) {
			    if (debug > 0) {
			    	syslog(LOG_INFO, "received HTTPS url request");
			    }
			}

                        // check if cached
                        get_hash(request.url, hash);
                        cached = in_cache(cachedb, hash, cache_exp_secs, debug);
                        if (cached == -1) // Happens when there is a cache problem
                            cached = 0;

                        // parse url to blacklist
                        if (!cached && !denied && blacklist != NULL) {
                            denied = blacklist_backend(blacklist, request.url, debug);
                        }

                        // parse url to proxy
                        if (!cached && !denied && proxy_ip != NULL) {
                            denied = proxy_backend(proxy_ip, proxy_port, proxy_deny_pattern, request.url, debug);
                        }

                        // parse url to squidguard
                        if (!cached && !denied && squidguard) {
                            denied = squidguard_backend(request.srcip, request.usr, request.url, sg_redirect, debug);
                        }

                        if (denied) {
                            if (frontend == N2H2 && squidguard) {
                                n2h2_deny(cli_fd, n2h2_request, sg_redirect);
                            } else if (frontend == WEBSNS && squidguard) {
                                websns_deny(cli_fd, websns_request, sg_redirect);
                            } else if (frontend == N2H2) {
                                n2h2_deny(cli_fd, n2h2_request, redirect_url);
                            } else {
                                websns_deny(cli_fd, websns_request, redirect_url);
                            }

                            if (debug > 0) {
                                syslog(LOG_INFO, "url denied: srcip %s, srcusr %s, dstip %s, url %s",
                                    request.srcip, request.usr, request.dstip, request.url);
			    }
                        } else {
                            if (frontend == N2H2) {
                                n2h2_accept(cli_fd, n2h2_request);
                            } else {
                                websns_accept(cli_fd, websns_request);
                            }
                            if (!cached)
                                add_cache(cachedb, hash, debug);
                            if (debug > 0)
                                syslog(LOG_INFO, "url accepted: srcip %s, dstip %s, url %s",
                                                 request.srcip, request.dstip, request.url);
                        }
                        // reset denied
                        denied = 0;
                    }
                }
                close_cache(cachedb, debug);
            }
            close(cli_fd);
        }
    }
    close(openufp_fd);
    closelog();
    return 0;
}
