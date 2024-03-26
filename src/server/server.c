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

// Command Messages
#define GO "go"
#define REJECT "reject"
#define CLIENT_DONE "END_OF_TRANSMISSION"

// FTP defines 
#define FTP_PORT 2003
#define IP "127.0.0.1"

// Function for serving client that is connected
void serve_client(int client_sd) { 
    int mlen;
    char outfile[BUFSIZE]; // Filename to write out to
    unsigned char client_buf[BUFSIZE+1]; // +1 for null terminator
    unsigned char server_buf[BUFSIZE+1]; 
    memset(server_buf, '\0', BUFSIZE + 1);
    memset(client_buf, '\0', BUFSIZE + 1);

    printf("Serving %d\n", client_sd);

    // Receive request (filename) from client 
    if (recv(client_sd, client_buf, sizeof(client_buf), 0) < 0) { 
        printf("Unable to receive filename from client\n");
        return;
    }
    printf("Outfile %s\n", client_buf);
    strcpy(outfile, client_buf);

    // Initiate transfer from client
    /// Ask for accept or reject and send go or die 
    char query;
    while(1) { 
        printf("Accept or reject %s? (y/n): ", outfile);
        query = getchar();
        if (query == 'n') {
            // Do not accept
            strcpy(server_buf, REJECT);
            if (send(client_sd, server_buf, strlen(REJECT), 0) < 0) { 
                printf("Unable to send reject to client\n");
                return;
            }
            printf("Rejected client file!\n");
            return;
        } else if (query == 'y') { 
            break; // Go do stuff
        }
    }

    // Init out file to write data to 
    FILE *outfp = fopen(outfile, "w");

    // Otherwise accept, and listen for message
    strcpy(server_buf, GO);
    if (send(client_sd, server_buf, strlen(GO), 0) < 0) { 
        printf("Unable to send init to client\n");
        return;
    }

    char *trm_pos; // Will hold terminating string
    // Begin listening for chunks
    printf("Begin listening for data\n");
    memset(client_buf, '\0', BUFSIZE); // Clean buffer
    while ((mlen = read(client_sd, client_buf, BUFSIZE)) > 0) {
        client_buf[mlen] = '\0';

        trm_pos = strstr(client_buf, CLIENT_DONE);
        if (trm_pos != NULL) { 
            *trm_pos = '\0';
            printf("Received EOT\n"); // We are done
            printf("[%d]%s\n", strlen(client_buf), client_buf);
            fwrite(client_buf, strlen(client_buf), 1, outfp);
            memset(client_buf, '\0', BUFSIZE); // Clean buffer
            break;
        }
        fwrite(client_buf, strlen(client_buf), 1, outfp);
        memset(client_buf, '\0', BUFSIZE); // Clean buffer
    }
    fclose(outfp);

    // Confirm receiving the file from the client
    printf("Successfully received filename and message from client\n");

    // Send success to client 
    printf("Sending success to client\n");
    memset(server_buf, '\0', BUFSIZE);
    strcpy(server_buf, "success");
    if (send(client_sd, server_buf, strlen(server_buf), 0) < 0) { 
        printf("Unable to send successs to client\n");
        return;
    }
}

int main(int argc, char *argv[]) { 
    int sd, client_sd; // server and client fd 
    struct sockaddr_in server, client; 

    // Initialize and establish sd for server
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) { 
        printf("Socket creation failed!\n");
        exit(0);
    }
    printf("socket %d\n", sd);

    // Set socket options
    int enable = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));

    // Ip and port assignments for server
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(IP);
    server.sin_port = htons(FTP_PORT);

    // Bind port 
    if ((bind(sd, (struct sockaddr*)&server, sizeof(server)) != 0)) { 
        printf("Socket bind failed!\n");
        exit(0);
    }
    printf("Binded to %s:%d\n", IP, FTP_PORT);

    // Listen for connections from clients
    if (listen(sd, 5) != 0) { 
        printf("Unable to listen\n");
        exit(0);
    }
    int client_len = sizeof(client);

    while(1) { // Main listen and server loop
        printf("Listening for clients...\n");

        // Receive client request for file transfer
        client_sd = accept(sd, (struct sockaddr*)&client, (socklen_t *)&client_len);

        // Accept file from the client
        if (client_sd < 0) { 
            printf("Failed client request:: %d\n", client_sd);
            continue;
        }
        printf("Serving %s:%i\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        // Server client 
        serve_client(client_sd);
        close(client_sd); // close connection with served client 
        // repeat
    }

    close(sd); // Close server sd
    return 0;
}