#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <stdint.h>

// Socket libraries
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>

// Buffer for sending chunk data to server
#define BUFSIZE 1024

#define IPV6 1 // Indicates we are using ipv6 

// FTP defines 
#define FTP_PORT 2000 // Data port for ftp

// Command Messages
#define GO "go"
#define CLIENT_DONE "END_OF_TRANSMISSION"
#define SUCCESS "success"
#define ACK "ack"
#define RETRY "retry"

#define WIPE(x) (memset(x, '\0', sizeof(x)))


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

uint64_t get_filelen(FILE *fp) { 
    uint64_t flen;
    
    fseek(fp, 0L, SEEK_END);
    flen = ftell(fp);
    //fseek(fp, 0L, SEEK_SET);
    rewind(fp);

    return flen;
}

int main(int argc, char *argv[]) { 
    int sd; // Socket decriptor for connection to server
    size_t size_read;
    ssize_t mlen; // Message length
    uint64_t total_read = 0; // Total bytes read from file

#ifdef IPV6
    struct sockaddr_in6 server, client;
#else 
    struct sockaddr_in server, client;
#endif
    char *ip; // Server IP address
    char *fn; // Filename to send to server
    unsigned char buf[BUFSIZE]; // Read buffer for file to send to server
    unsigned char server_buf[BUFSIZE];

    // Parse arguments for server IP address and filename from user
    // Read positional arguments for ip and port from command line
    if (argc != NUM_ARGS) { usage(); }
    ip = argv[Ip];
    fn = argv[Fn];

    // Check to make sure file exists
    FILE *fp = fopen(fn, "rb");
    if (fp == NULL) { 
        printf("FIle %s does not exist!\n", fn);
        exit(0);
    }

    // Initiate tcp connection to server
    /// Init socket 
#ifdef IPV6 
    sd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
#else 
    sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    if (sd == -1) { 
        printf("Failed to init socket!\n");
        exit(0);
    }

    // IP port assignments for server
#ifdef IPV6 
    server.sin6_family = AF_INET6;
    server.sin6_port = htons(FTP_PORT);
    inet_pton(AF_INET6, ip, &server.sin6_addr);
    server.sin6_scope_id = if_nametoindex("enp7s0");
#else 
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(FTP_PORT);
#endif
    // Connect cient to server socket
    if (connect(sd, (struct sockaddr*)&server, sizeof(server)) != 0) { 
        printf("Failed to connect to server!\n");
        exit(0);
    }
    printf("Successfully connected to server! --> %d\n", sd);

    /// Main loop to send file to server 
    /// Get size of file and append to request to server
    sprintf(buf, "%s,%ld", fn, get_filelen(fp));
    printf("Message to server [%s]\n", buf);

    // Send request to server 
    if (send(sd, buf, strlen(buf), 0) < 0) { 
        printf("Unable to send request to server\n");
        exit(0);
    }
    printf("Successfully sent request to server %s:%d\n", ip, FTP_PORT);
    
    // wait for ack and send filename to server
    if ((mlen = recv(sd, buf, sizeof(buf), 0)) < 0) { 
        printf("Unable to receive message from server\n");
        exit(0);
    }
    printf("len %ld\n", mlen);
    buf[mlen] = '\0'; // Null terminate
    printf("Server message: [%ld]%s\n", mlen, buf);
    
    if (strcmp(buf, ACK) != 0) { 
        printf("Didn't receive filename ack!\n");
        close(sd);
        exit(0);
    }
    WIPE(buf); // Clean buffer
    
    size_t nrecv; // Number of bytes the server received
    char* end_ptr;
    // Begin sending file data
    while((size_read = fread(&buf, 1, BUFSIZE, fp)) > 0) {    
        while(1) {
            // Send data to server
            if (send(sd, buf, size_read, 0) != size_read) { 
                printf("Error sending data file!\n");
                break;
            }
            printf("Sent [%d]%s\n", (int)size_read, (char*)buf);

            // get ack from server
            mlen = read(sd, server_buf, BUFSIZE);

            // If we get a retry, try to resend buffer
            if (strcmp(server_buf, RETRY) != 0) { 
                printf("Got retry\n");
                break;
            }
            break;
        }

        //memset(buf, '\0', BUFSIZE); // Clean buffer
    }
    fclose(fp);
    printf("Done sending file: %ld bytes sent\n", total_read);
    //send(sd, CLIENT_DONE, strlen(CLIENT_DONE), 0); // Indicate to server we are done

    memset(buf, 0, BUFSIZE); // Clean buffer

    printf("Waiting on response from server...\n");
    // Get status from server
    mlen = recv(sd, buf, sizeof(buf), 0);
    if (mlen < 0) { 
        printf("Unable to receive message from server\n");
        exit(0);
    }
    buf[mlen] = '\0';
    printf("Server message: %s\n", buf);

    // Inform the user whether the file transfer was successful and close the connection
    if (strcmp(buf, SUCCESS) != 0) { 
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