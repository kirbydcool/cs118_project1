#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <string.h>	


void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void sendFile(int sock, FILE * fd);
void contentType(char* type, char* fileName);
void parseRequest(char * buffer,char * httpRequest); 
void sendHeader(int sock, int status, char* contentType, int contentLength);
void dostuff(int); 

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/

void dostuff (int sock)
{
    #define BUFSIZE  512*512    //arbitrary size
    int n;
    char buffer[BUFSIZE];
     
    // read in HTTP request 
    bzero(buffer,BUFSIZE);
    n = read(sock,buffer,BUFSIZE-1);
    if (n < 0) error("ERROR reading from socket");
    
    // dump request to console
    printf("Here is the message: %s\n",buffer);
    char file[BUFSIZE];
    bzero(file,BUFSIZE);

    // get name of requested file
    parseRequest(file,buffer);

    char testMessage[BUFSIZE];
    bzero(testMessage,BUFSIZE);
    int dataLength; 
    
    FILE * fd = fopen(file,"r");

    if (fd == NULL) // open failed 
    {
        sendHeader(sock, 404, "text/html", 0);
        printf("failed open file '%s'", file); 
    }
    else // send header and file
    {
        //find file length
        fseek(fd, 0L, SEEK_END);
        dataLength = ftell(fd);
        fseek(fd, 0L, SEEK_SET); //set back to beginning
        
        char content[BUFSIZE];
        contentType(content,file);

        // send all info
        sendHeader(sock, 200, content, dataLength);
        sendFile(sock, fd);
        
        fclose(fd);
    }

    if (n < 0) error("ERROR writing to socket");
}

void contentType(char* type, char* fileName)
{
    // only implemented 3 recognized file types
    // other types can be requested,
    // but won't be dealt with in HTTP
    if (strstr(fileName,".html") != NULL)
    {
        strcpy(type,"text/html; charset=UTF-8\n"); 
    }
    else if (strstr(fileName,".gif") != NULL)
    {
        strcpy(type,"image/gif\n"); 
    }
    else if (strstr(fileName,".jpeg") != NULL)
    {
        strcpy(type,"image/jpeg\n"); 
    }
    return;
}

void sendHeader(int sock, int status, char* contentType, int contentLength) {
    write(sock, "HTTP/1.1 ", 9);

    //status code
    char statusString[32];
    switch (status) {
        case 200:
            strcpy(statusString, "200 OK");
            break;
        case 404:
            strcpy(statusString, "404 Not Found");
            break;
        case 500:
        default:
            strcpy(statusString, "500 Internal Error");
    }
    write(sock, statusString, (int) strlen(statusString));
    
    //content language
    write(sock, "\nContent-Language: en-US\n", 1);
    
    //content type
    write(sock, "Content-Type: ", 14);
    write(sock, contentType, strlen(contentType));

    //charset
    //write(sock, "; charset=UTF-8\n", 16);
    
    //contentLength
    char length[BUFSIZE];
    sprintf(length,"Content-Length:%d", contentLength);
    write(sock, length, strlen(length));

    //connection
    write(sock,"\nConnection: keep-alive\n\n",25);
}

void parseRequest(char * buffer,char * httpRequest)
{
    // filename immediately follows GET
    char * substring = strstr(httpRequest,"GET /");
    if (substring != NULL)
    {
        //filename precedes HTTP
        char * substringEnd = strstr(substring,"HTTP");
        substring = &substring[5];
        int querySize = strlen(substring) - strlen(substringEnd) - 1;
        strncpy(buffer,substring,querySize); 
        buffer[querySize] = '\0';
    }      
    return; 
}

void sendFile(int sock, FILE * fd) {
    char buffer[BUFSIZE];
    long int dataLength;
    // iterate over data and send in chunks
    while( dataLength = fread(buffer, 1, BUFSIZE, fd) ) {
        write(sock, buffer, dataLength);
    }
    if(ferror(fd)) { //read error
        fprintf(stderr, "read error\n");
    }
} 
