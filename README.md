# httpserver

This project is an httpserver, writen in C. The HTTP server can handles HTTP GET request. The HTTP server is also implemented with proxy mode.

Usage:
help: (show the usage of the httpserver)

./httpserver --help  

first mode: (selects a directory from which to serve files, --num-threads is optional, it can sepecify the max number of clientsthe server can serve simultaneously, "directory/" indicates serving file from directory : "directory/" (you can change it to "files/" as I provided some simple documents in "files/") )

./httpserver --files directory/ --port 8000 [--num-threads 5]

seconde mode: (selects an upstream HTTP server to proxy, the argument can have a port number after a colon, e.g. inst.eecs.berkeley.edu:80, if not specified, the default prot is 80, --num-threads is optional, it can sepecify the max number of clientsthe server can serve simultaneously)

./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]


when httpserver works in the first mode, if the HTTP request's path corresponds to a file, it will search for the specified file. If the file exits,  respond with a 200 OK and the full content of the file, ortherwise, respond a 404 NOT FOUND.
If the HTTP request's path corresponds to a directory, and the directory does not contain an index.html file, respond with a HTML page contains the list of links containing all immediate children of the directory, as well as the parent directory.


when http server works in the second mode, the httpserver will do a DNS look up of server_proxy_hostname and get the IP address. Then create an socket and connect it to the IP address. After accept request from clients, it will wait for data at both sockets and when the data arrives, immediatedly read it and write to the other socket. This 2-way communication between the HTTP client and the upstream server is maintained through two threads.

--numb-threads will specify the max number of clients it can handle at the same time.
The httpserver is implemented with a thread_pool with exact --numb-threads of thread.
The server server will keep accept connections client_socket_file_descriptors and push it to a work_queue, the thread from the thread_pool will keep pop client_socket_file_descriptors from the work_queue and call appropriate request_handler to handle client request.





