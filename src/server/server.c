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

// Command Messages
#define GO "go"
#define REJECT "reject"
#define CLIENT_DONE "END_OF_TRANSMISSION"
#define SUCCESS "success"
#define ACK "ack"
#define RETRY "retry"

// Net defines
#define IPV6 1 
#define QUEUELEN 5

// FTP defines 
#define FTP_PORT 2000
#ifdef IPV6 
#define IP "::1"
#else
#define IP "127.0.0.1"
#endif

#define WIPE(x) (memset(x, '\0', sizeof(x)))

// Function for serving client that is connected
void serve_client(int client_sd) { 
    size_t mlen;
    uint64_t total_read = 0; // bytes read
    char* temp; // Used to get tokens from client request
    char* end_ptr;
    char outfile[BUFSIZE]; // Filename to write out to
    uint64_t flen; // File length
    unsigned char client_buf[BUFSIZE] = {0}; // +1 for null terminator
    unsigned char server_buf[BUFSIZE] = {0}; 

    printf("Serving %d\n", client_sd);

    // Receive filename from client
    mlen = recv(client_sd, client_buf, sizeof(client_buf), 0);
    if (mlen < 0) { 
        printf("Unable to receive filename from client\n");
        return;
    }
    // Retrieve filename, filelength
    printf("Message --> [%ld]%s\n", mlen, client_buf);    
    temp = strtok(client_buf, ","); // Get filename
    strcpy(outfile, temp);

    temp = strtok(NULL, ",");          // Get file length

    if (temp != NULL) { 
        flen = strtol(temp, &end_ptr, 10);
    } else { 
        printf("Failed to parse client request!\n");
        return;
    }
    printf("Receiving %ld bytes from %s\n", flen, outfile);
    WIPE(client_buf);

    // Initiate transfer from client
    /// Ask for accept or reject and send go or die 
    char query;
    printf("Accept or reject %s? (y/n): ", outfile);
    while(1) { 
        scanf("%c", &query);
        if (query == 'n') {
            // Do not accept
            if (send(client_sd, REJECT, strlen(REJECT), 0) < 0) { 
                printf("Unable to send reject to client\n");
                return;
            }
            printf("Rejected client file!\n");
            return;
        } else if (query == 'y') { 
            break; // Go do stuff
        }
    }

    // Otherwise accept, and listen for message
    printf("strlen %ld\n", strlen(ACK));
    if (send(client_sd, ACK, strlen(ACK), 0) < 0) { 
        printf("Unable to send init to client\n");
        return;
    }

    printf("file %s\n", outfile);
    // Init out file to write data to 
    FILE *outfp = fopen(outfile, "wb");

    if (outfp == NULL) { 
        printf("Failed to open %s\n", outfile);
        return;
    }

    size_t nsent = 0; // bytes client intended to send

    printf("Begin listening for data\n");
    while ((mlen = read(client_sd, client_buf, BUFSIZE)) > 0) {
        
        // Check if we received the right length
        if (flen > BUFSIZE && mlen < BUFSIZE) { 
            // ask to retransmit
            if (send(client_sd, RETRY, strlen(RETRY), 0) < 0) { 
                printf("Unable to send retry to client\n");
                break;
            }
            continue;
        } else { // Otherwise send we're good and send the next
            if (send(client_sd, ACK, strlen(ACK), 0) < 0) { 
                printf("Unable to send ack to client\n");
                break;
            }
        }
        
        // Write out to file on valid receive
        if (fwrite(client_buf, 1, mlen, outfp) != mlen) { 
            printf("Error writing file!\n");
            break;
        }
        total_read += mlen;
        flen -= mlen;

        printf("[%ld] Left %ld\n", mlen, flen);
        if (flen <= 0) { break; }
    }
    fclose(outfp);
    printf("didnt receive %ld\n", flen);

    // Confirm receiving the file from the client
    printf("Successfully received %ld bytes from %s\n", total_read, outfile);

    // Send success to client 
    printf("Sending success to client\n");
    if (send(client_sd, SUCCESS, strlen(SUCCESS), 0) < 0) { 
        printf("Unable to send successs to client\n");
        return;
    }
}

int main(int argc, char *argv[]) { 
    int sd, client_sd; // server and client fd 
#ifdef IPV6 
    char addr6_str[INET6_ADDRSTRLEN];
    struct sockaddr_in6 server, client;
#else
    struct sockaddr_in server, client; 
#endif

    // Initialize and establish sd for server
#ifdef IPV6 
    sd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
#else
    sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    if (sd == -1) { 
        printf("Socket creation failed!\n");
        exit(0);
    }
    printf("Connected to socket %d\n", sd);

    // Set socket options
    int enable = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));

    // Ip and port assignments for server
#ifdef IPV6 
    server.sin6_family = AF_INET6;
    server.sin6_addr = in6addr_any;
    server.sin6_port = htons(FTP_PORT);
    server.sin6_scope_id = if_nametoindex("enp7s0");
#else
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(IP);
    server.sin_port = htons(FTP_PORT);
#endif

    // Bind port 
    if ((bind(sd, (struct sockaddr*)&server, sizeof(server)) != 0)) { 
        printf("Socket bind failed!\n");
        close(sd);
        exit(0);
    }
    printf("Binded to %s:%d\n", IP, FTP_PORT);

    // Listen for connections from clients
    if (listen(sd, QUEUELEN) != 0) { 
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
    #ifdef IPV6 
        inet_ntop(AF_INET6, &(client.sin6_addr), addr6_str, sizeof(addr6_str));
        printf("Serving %s:%i\n", addr6_str, ntohs(client.sin6_port));
    #else 
        printf("Serving %s:%i\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    #endif
        // Server client 
        serve_client(client_sd);
        close(client_sd); // close connection with served client 
        // repeat
    }

    close(sd); // Close server sd
    return 0;
}