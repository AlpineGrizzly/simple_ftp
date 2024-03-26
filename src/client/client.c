#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>

// Socket libraries
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>

// Buffer for sending chunk data to server
#define BUFSIZE 1024

// FTP defines 
#define FTP_PORT 2003 // Data port for ftp
#define CLIENT_DONE "END_OF_TRANSMISSION"

// Arg defines
#define NUM_ARGS 3
enum Args{None, Ip, Fn};

/**
 * usage
 * 
 * Prints the usage of the program
*/
void usage() { 
	char* usage_string = "Usage: client [server_ip] [filename]\n" 
						 "FTP client \n\n" 
                         "-h      Show this information\n";
    
	printf("%s", usage_string);
    exit(0);
}

int main(int argc, char *argv[]) { 
    int sd; // Socket decriptor for connection to server
    int mlen; // Message length
    struct sockaddr_in server, client;
    char *ip; // Server IP address
    char *fn; // Filename to send to server
    unsigned char buf[BUFSIZE]; // Read buffer for file to send to server

    // Parse arguments for server IP address and filename from user
    // Read positional arguments for ip and port from command line
    if (argc != NUM_ARGS) { usage(); }
    ip = argv[Ip];
    fn = argv[Fn];

    // Check to make sure file exists
    FILE *fp = fopen(fn, "r");
    if (fp == NULL) { 
        printf("FIle %s does not exist!\n", fn);
        exit(0);
    }

    // Initiate tcp connection to server
    /// Init socket 
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) { 
        printf("Failed to init socket!\n");
        exit(0);
    }

    // IP port assignments for server
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(FTP_PORT);

    // Connect cient to server socket
    if (connect(sd, (struct sockaddr*)&server, sizeof(server)) != 0) { 
        printf("Failed to connect to server!\n");
        exit(0);
    }
    printf("Successfully connected to server! --> %d\n", sd);

    /// Main loop to send file to server 
    int success = 0; // Indicates whether file transfer was successful
    size_t size_read;

    // Send request to server 
    if (send(sd, fn, strlen(fn), 0) < 0) { 
        printf("Unable to send request to server\n");
        exit(0);
    }
    printf("Successfully sent request to server %s:%d\n", ip, FTP_PORT);
    
    // wait for confirmation and send filename to server
    if (mlen = recv(sd, buf, sizeof(buf), 0) < 0) { 
        printf("Unable to receive message from server\n");
        exit(0);
    }
    printf("Server message [%d == %d]: %s\n", strlen(buf), mlen, buf);
    
    if (strcmp(buf, "go") != 0) { 
        printf("Rejected by server!\n");
        close(sd);
        exit(0);
    }
    memset(buf, 0, BUFSIZE); // Clean buffer

    int total_read = 0;
    // Begin sending file data
    while(size_read = fread(&buf, 1, BUFSIZE, fp) > 0) {    
        // Read X bytes of data from file 
        // Send X bytes of data read to server
        printf("Sent [%d]%s\n", strlen(buf), (char*)buf);
        send(sd, buf, strlen(buf), 0);
        // repeat until EOF
        total_read += size_read;
        memset(buf, '\0', BUFSIZE); // Clean buffer
    }
    fclose(fp);
    printf("Done sending file: %d bytes sent\n", total_read);
    send(sd, CLIENT_DONE, strlen(CLIENT_DONE), 0); // Indicate to server we are done

    memset(buf, 0, BUFSIZE); // Clean buffer

    printf("Waiting on response from server...\n");
    // Get status from server
    if (recv(sd, buf, sizeof(buf), 0) < 0) { 
        printf("Unable to receive message from server\n");
        exit(0);
    }
    printf("Server message: %s\n", buf);
    // Inform the user whether the file transfer was successful and close the connection
    if (strcmp(buf, "success") != 0) { 
        // failed!
        printf("File transfer failed!\n");
    } else { 
        // success!
        printf("File transfer successful!\n");
    }

    // Close connection to server 
    close(sd);

    return 0;
}