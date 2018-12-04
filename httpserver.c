#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

#define LIBHTTP_REQUEST_MAX_SIZE 8192

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;


struct socket_fds {
    int client_fd;
    int proxy_fd;
};

/*check whether request a file or whatever*/
char * containsFile(char *path){
    char *res;
    char cwd[1024];
    bzero(cwd,1024);
    if(getcwd(cwd,sizeof(cwd))==NULL){
        res = "no cwd";
        return res;
    }
    strcat(cwd, "/");
    strcat(cwd,server_files_directory);
    strcat(cwd, "/files");
    strcat(cwd, path);
    struct stat sb;
    if(stat(cwd, &sb)<0){
        return NULL;
    } else {
        if(S_ISREG(sb.st_mode)){
            bzero(res,sizeof(res));
            res = malloc(sizeof(cwd));
            strcpy(res, cwd);
            return res;
        } else if (S_ISDIR(sb.st_mode)){
            res = malloc(sizeof(cwd));
            strcpy(res,cwd);
            
            strcat(cwd,"/index.html");
            bzero(&sb, sizeof(sb));
            if(stat(cwd, &sb)<0){
                return res;
            }else {
                res = malloc(sizeof(cwd));
                strcpy(res,cwd);
                return res;
            }
        }
        else return NULL;
    }
}

/*read the file content*/
char * readFile(char *path, char *path2){
    char *buffer;
    long length;
    
    struct stat sb;
    if(stat(path, &sb)<0){
        buffer = "read File Wrong!";
        return buffer;
    }
    //not found index.html in path
    //list the files it contains
    if(S_ISDIR(sb.st_mode)){
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir (path)) != NULL) {
            /* print all the files and directories within directory */
            buffer = malloc(2048 * sizeof(char));
            bzero(buffer,2048);
            strcat(buffer, "<h1><a href=\"http://192.168.162.162:8000/index.html\">Home</a></h1>");
            while ((ent = readdir (dir)) != NULL) {
                
                if((ent->d_name)[0]!='.'){
                    strcat(buffer, "<h1><a href=\"http://192.168.162.162:8000");
                    strcat(buffer, path2);
                    if(path2[strlen(path2)-1]!='/') strcat(buffer, "/");
                    strcat(buffer, ent->d_name);
                    strcat(buffer, "\">");
                    strcat(buffer, ent->d_name);
                    strcat(buffer, "</a></h1>");
                }
            }
            closedir (dir);
            return buffer;
            
            //send response
            http_start_response(fd, 200);
            http_send_header(fd, "Content-Type", type);
            http_send_header(fd, );
            
            
        } else {
            /* could not open directory */
            //perror ("");
            buffer = "wrong !!";
            return buffer;
        }
    }else {
        FILE *fd ;
        if( (fd=fopen(path, "r+")) == NULL){
            return NULL;
        } else {
            fseek(fd,0,SEEK_END);
            length = ftell(fd);
            fseek(fd, 0, SEEK_SET);
            buffer = malloc(length * (sizeof(char)));
            if(buffer){
                fread(buffer, sizeof(char), length, fd);
            }
            fclose(fd);
            return buffer;
        }
    }
    
}



/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {

  /*
   * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
   * any existing code.
   */

  struct http_request *request = http_request_parse(fd);
    //if error was found
    if(request == NULL ){
        http_start_response(fd,400); // 400 - bad request
        http_end_headers(fd);
        close(fd);
        return;
    }
    
    
    
    char *method = request->method;
    char *path = request->path;
    char *type;
    size_t length;
    
    char *filePath = containsFile(path);
    char *sendStr;
    if(filePath==NULL){
        sendStr = "no string";
        respond_with_NotFound(fd);
        http_start_response(fd, 404);
        http_send_header(fd, "Content-Type", type);
        http_end_headers(fd);
        http_send_string(fd,sendStr);
        
    } else {
        sendStr = readFile(filePath, path);
        if(sendStr == NULL)
            sendStr = "nofile\n";
        
        struct stat sb;
        if(stat(filePath, &sb)<0){
            perror("stat() wrong");
            exit(1);
        }
        if(S_ISDIR(sb.st_mode)){
            type = "text/html";
            length = strlen(sendStr);
            
            http_start_response(fd, 200);
            http_send_header(fd, "Content-Type", type);
            http_end_headers(fd);
            http_send_string(fd,sendStr);
            
        }else {
            type = http_get_mime_type(filePath);;
            length = (size_t)sb.st_size;
            
            http_start_response(fd, 200);
            http_send_header(fd, "Content-Type", type);
            http_end_headers(fd);
            http_send_data(fd,sendStr,length);
            
        }
    }
    

}

//int proxy_socket_number;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;


/*forward client request to proxy target through server*/

void *server_to_proxy(void *fds){
    
    pthread_mutex_lock(&lock);
    struct socket_fds *socket_file_des = fds;
    
    int proxy_fd = socket_file_des->proxy_fd;
    int server_fd = socket_file_des->client_fd;
    
    char *buffer = malloc(LIBHTTP_REQUEST_MAX_SIZE + 1);
    if (!buffer) http_fatal_error("Malloc failed");
    while(1){
        bzero(buffer, LIBHTTP_REQUEST_MAX_SIZE + 1);
        int bytes_read = read(server_fd, buffer, LIBHTTP_REQUEST_MAX_SIZE);
        if(bytes_read<0){
            perror("error read ");
            close(proxy_fd);
            close(server_fd);
            break;
            return;
        }
        if(bytes_read>0){
            http_send_data(proxy_fd,buffer,(size_t)bytes_read);
        }
    }
    int bytes_read = read(fd, buffer, LIBHTTP_REQUEST_MAX_SIZE);
    read_buffer[bytes_read] = '\0'; /* Always null-terminate. */
    shutdown(proxy_fd, SHUT_RDWR);
    close(proxy_fd);
    shutdown(server_fd, SHUT_RDWR);
    close(server_fd);
    
    pthread_mutex_unlock(&lock);
    
}

/*forward proxy target response to client through server*/

void *proxy_to_server(void *fds){
    pthread_mutex_lock(&lock);
    
    struct socket_fds *socket_file_des = fds;
    
    int proxy_fd = socket_file_des->proxy_fd;
    int server_fd = socket_file_des->client_fd;
    
    char *buffer = malloc(LIBHTTP_REQUEST_MAX_SIZE + 1);
    if (!buffer) perror("Malloc failed");
    
    
    
    while(1){
        bzero(buffer, LIBHTTP_REQUEST_MAX_SIZE + 1);
        int bytes_read = read(proxy_fd, buffer, LIBHTTP_REQUEST_MAX_SIZE);
        if(bytes_read<0){
            perror("error read ");
            close(proxy_fd);
            close(server_fd);
            break;
            return;
        }
        else if(bytes_read>0){
            http_send_data(server_fd,buffer,(size_t)bytes_read);
        }
    pthread_mutex_unlock(&lock);
    }
    int bytes_read = read(fd, buffer, LIBHTTP_REQUEST_MAX_SIZE);
    read_buffer[bytes_read] = '\0'; /* Always null-terminate. */
    shutdown(proxy_fd, SHUT_RDWR);
    close(proxy_fd);
    shutdown(server_fd, SHUT_RDWR);
    close(server_fd);
}




/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
   * TODO: Your solution for Task 3 goes here! Feel free to delete/modify *
   * any existing code.
   */
    
    
    //create proxy socket
    int *proxy_socket_number;
    struct sockaddr_in server_address;
    //int client_socket_number;
    //size_t client_address_length = sizeof(client_address);
    
    struct hostent *server;
    
    pthread_t client_thread, proxy_thread;
    
    
    *proxy_socket_number = socket(PF_INET, SOCK_STREAM, 0);
    if (*proxy_socket_number == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }
    
    int socket_option = 1;
    if (setsockopt(*proxy_socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                   sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        exit(errno);
    }
    
    server = gethostbyname(server_proxy_hostname);
    if(server==NULL){
        perror("get host by name: ");
        exit(errno);
    }
    
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_proxy_port);
    
    if(connect(*proxy_socket_number, (struct sockaddr*)&server_address, sizeof(server_address))<0){
        perror("fail coonnect to server proxy: ");
        exit(errno);
    }
    
    struct socket_fds *socket_file_des;
    
    socket_file_des = malloc(sizeof(*socket_file_des));
    socket_file_des->client_fd = fd;
    socket_file_des->proxy_fd = (*proxy_socket_number);
    
    pthread_create(&client_thread, NULL, server_to_proxy, (void *)socket_file_des);
    pthread_create(&proxy_thread, NULL, proxy_to_server, (void *)socket_file_des);
    
    pthread_join(client_thread, NULL);
    pthread_join(proxy_thread, NULL);
    
    //shutdown(*proxy_socket_number, SHUT_RDWR);
    close(*proxy_socket_number);
    
    

}











/* thread get a task and server it */
void *thread_do_task(void *dummyPtr){
    
    void (*request_handler)(int) = dummyPtr;
    int client_socket_number = wq_pop(&work_queue);
    request_handler(client_socket_number);
    close(client_socket_number);
}


/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
//void serve_forever(int *socket_number, void (*request_handler)(int)) {

void *serve_forever(void *socket_number1) {
    
    int *socket_number = (int *)socket_number1;

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
      
      wq_push(&work_queue,client_socket_number);

    request_handler(client_socket_number);
    close(client_socket_number);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}



/* server fd , but initiallized in serve_forever*/
int server_fd;


void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}





int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }
    
  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

    wq_init(&work_queue);
    
    pthread_t thread_id[num_threads];
    pthread_t main_thread;
    pthread_create( &main_thread, NULL, serve_forever, (void *)&server_fd );
    while(1){
        int i;
        for(i=0;i<num_threads; i++){
            pthread_create( &thread_id[i], NULL, thread_do_task, request_handler );
        }
        for(i=0;i<num_threads; i++){
            pthread_join( thread_id[i], NULL);
        }
    }
    //serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
