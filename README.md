## Openufp
Open URL Filtering Proxy is an URL Filtering Server for N2H2 or Websense compatible devices.
It supports multiple back ends like url blacklist file, squidguard and proxy servers like dansguardian.
This project has been known as n2h2p.


## Install
Just type "make" followed by "sudo make install".
Make sure you have libdb and libdb-dev installed, preferable version 5.1 or higher.


## Usage
```
Usage: openufp [OPTIONS] <-n|-w> <BACKEND>

OPTIONS:
   -l PORT   on which port openufp will listen for incoming requests
   -r URL    when url is denied the client will be redirected to this url
   -c SECS   cache expire time in seconds; default 3600; 0 disables caching
   -C URL    remove specified URL from cache
   -d LEVEL  debug level 1-3

FRONTEND:
   -n        act as n2h2 server
   -w        act as websense server
BACKEND:
   -p IP:PORT:DENY_PATTERN   use the proxy backend
             IP is the ipnumber of the proxy server
             PORT is the portnumber where the proxy server is listening on
             DENY_PATTERN is a piece of text that should match the deny page
   -f FILE   use the blacklist file backend
             FILE is a file which contains blacklisted urls
   -g        use the squidGuard backend
```

## Note
The default location of the cache db is **/var/cache/openufp/cache.db**<br>
When squidguard backend is used be sure that this program has **rw** permissions
to the squidguard db files.
