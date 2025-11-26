/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
#define CHUNKSIZE 16000

/*
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

void exit_operation_to_server(int sockfd, struct sockaddr_in clientaddr, int clientlen);
void ls_to_server(int sockfd, struct sockaddr_in clientaddr, int clientlen);
void delete_file_from_server(int sockfd, char *filename, struct sockaddr_in clientaddr, int clientlen);
void put_file_to_server(int sockfd, char *filename, struct sockaddr_in *clientaddr, int clientlen);
void get_file_from_server(int sockfd, char *filename, struct sockaddr_in clientaddr, int clientlen);
void receive_file_with_ack(int sockfd, char *filename, struct sockaddr_in *clientaddr);
void send_file_with_ack(char *filename, int sockfd, struct sockaddr_in *clientaddr);

struct timeval set_timeout(int sockfd, int seconds)
{
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    return timeout;
}

void disable_timeout(struct timeval timeout, int sockfd)
{
    timeout.tv_sec = 0; // disable timeout
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

int main(int argc, char **argv)
{
    int sockfd;                    /* socket */
    int portno;                    /* port to listen on */
    int clientlen;                 /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp;         /* client host info */
    char buf[BUFSIZE];             /* message buf */
    char *hostaddrp;               /* dotted decimal host addr string */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */

    /*
     * check command line arguments
     */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval, sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *)&serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /*
     * main loop: wait for a datagram, then echo it
     */
    clientlen = sizeof(clientaddr);
    while (1)
    {
        char command[100];
        bzero(command, sizeof(command));
        /*
         *  recvfrom: receive a UDP datagram from a client
            received command from client
            e.g. get abc.txt
         */
        n = recvfrom(sockfd, command, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
        // if (n < 0)
        //     printf("ERROR in recvfrom");

        printf("server received %ld/%d bytes: %s\n", strlen(command), n, command);

        // send timeout
        struct timeval timeout1;
        timeout1.tv_sec = 2;
        timeout1.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout1, sizeof(timeout1));

        char op[16];
        char filename[256];

        bzero(op, sizeof(op));
        bzero(filename, sizeof(filename));

        command[strcspn(command, "\n")] = '\0';

        // segregating command: op -> get filename -> abc.txt
        sscanf(command, "%s %s", op, filename);

        if (strcmp(op, "get") == 0)
        {
            get_file_from_server(sockfd, filename, clientaddr, clientlen);
        }
        else if (strcmp(op, "put") == 0)
        {
            put_file_to_server(sockfd, filename, &clientaddr, clientlen);
        }
        else if (strcmp(op, "delete") == 0)
        {
            delete_file_from_server(sockfd, filename, clientaddr, clientlen);
        }
        else if (strcmp(op, "ls") == 0)
        {
            ls_to_server(sockfd, clientaddr, clientlen);
        }
        else if (strcmp(op, "exit") == 0)
        {
            exit_operation_to_server(sockfd, clientaddr, sizeof(clientaddr));
            printf("Exiting from the connection with server \n");
        }
        else
        {
            printf("Invalid input requested.");
        }
    }
}

void exit_operation_to_server(int sockfd, struct sockaddr_in clientaddr, int clientlen)
{
    int n;
    
    printf("Client is done with all operations.\n");
    
    char buffer1[100];
    bzero(buffer1, sizeof(buffer1));
    
    strcpy(buffer1, "Goodbye Client. Done with all operations!");

    // sending goodbye message to client
    n = sendto(sockfd, buffer1, strlen(buffer1), 0,
               (struct sockaddr *)&clientaddr, clientlen);

    // if (n < 0)
    //     printf("ERROR in sendto");

    printf("--------------------------------------------------------------------------------\n");
}

void ls_to_server(int sockfd, struct sockaddr_in clientaddr, int clientlen)
{
    int n;
    
    char buffer[BUFSIZE];
    bzero(buffer, sizeof(buffer));

    const char *current_working_dir_path = ".";
    char command[256];

    snprintf(command, sizeof(command), "ls %s > ../ls_output.txt", current_working_dir_path);
    printf("Server implementing ls command...\n");
    /* 
        Implementing ls . > ../ls_output.txt using system
        It saves the output of ls command in the specified file temporaryily
    */
    system(command);

    FILE *fp;

    fp = fopen("../ls_output.txt", "r");

    if (fp == NULL)
    {
        printf("Error while opening ls output file!\n");
        return;
    }

    // sending ls file output to client
    while (fread(buffer, 1, sizeof(buffer), fp) != NULL)
    {
        n = sendto(sockfd, buffer, BUFSIZE, 0,
                   (struct sockaddr *)&clientaddr, clientlen);
        if (n < 0)
            printf("ERROR in sendto");
    }

    fclose(fp);

    //removing temp file
    remove("../ls_output.txt");
    
    printf("Server implementation of ls command complete.\n");
    printf("--------------------------------------------------------------------------------\n");
}

/*
    Sends a message to the client, displaying success or failure
*/
void delete_file_from_server(int sockfd, char *filename, struct sockaddr_in clientaddr, int clientlen)
{
    int n;
    if (remove(filename) == 0)
    {
        printf("Removed file %s \n", filename);
        
        char buffer[100];
        bzero(buffer, sizeof(buffer));
        
        sprintf(buffer, "Delete %s successful!", filename);
        
        n = sendto(sockfd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&clientaddr, clientlen);
        // if (n < 0)
        //     printf("ERROR in sendto");
    }
    else
    {
        printf("Error while delete file. File does not exists.\n");
        
        char buffer[100];
        bzero(buffer, sizeof(buffer));
        
        sprintf(buffer, "%s does not exist on server!", filename);
        
        n = sendto(sockfd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&clientaddr, clientlen);
        // if (n < 0)
        //     printf("ERROR in sendto");
    }
    printf("--------------------------------------------------------------------------------\n");
}

void put_file_to_server(int sockfd, char *filename, struct sockaddr_in *clientaddr, int clientlen)
{
    char *name = filename;

    /*  this function is to have RECV TIMEOUT. It helps during packet loss. */
    struct timeval timeout = set_timeout(sockfd, 2); // 2 seconds timeout

    receive_file_with_ack(sockfd, filename, &clientaddr);

    int n;

    char buffer1[100];
    bzero(buffer1, sizeof(buffer1));

    sprintf(buffer1, "Put %s successful!", name);

    n = sendto(sockfd, buffer1, strlen(buffer1), 0,
               (struct sockaddr *)&clientaddr, clientlen);
    // if (n < 0)
    //     printf("ERROR in sendto");

    /*  this function is to disable RECV TIMEOUT. It helps during packet loss. */
    disable_timeout(timeout, sockfd);

    printf("--------------------------------------------------------------------------------\n");
}

void get_file_from_server(int sockfd, char *filename, struct sockaddr_in clientaddr, int clientlen)
{
    send_file_with_ack(filename, sockfd, &clientaddr);

    printf("--------------------------------------------------------------------------------\n");
}

void send_file_with_ack(char *filename, int sockfd, struct sockaddr_in *clientaddr)
{
    int MAX_RETRIES = 5; // setting maximum number of tries in case of packet loss

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        printf("Error opening file. File does not exists.\n");

        int n;
        // if message is DNE -> The file does not exist on server
        n = sendto(sockfd, "DNE", 3, 0, (struct sockaddr *)clientaddr, sizeof(*clientaddr));
        if (n < 0)
            printf("ERROR in sendto");

        return;
    }

    char buffer[CHUNKSIZE];
    bzero(buffer, sizeof(buffer));

    socklen_t clientlen = sizeof(*clientaddr);
    int bytes_read, seq_num = 0;

    struct timeval timeout = set_timeout(sockfd, 2); // 2 seconds timeout

    while ((bytes_read = fread(buffer, 1, CHUNKSIZE, fp)) > 0)
    {
        /*
            This is logic for sending packets in chunks. Appending an integer value (sequence number)
            On server side it would be checked and ACK will be sent
        */
        char packet[CHUNKSIZE + sizeof(int)];
        bzero(packet, sizeof(packet));

        memcpy(packet, &seq_num, sizeof(int));
        memcpy(packet + sizeof(int), buffer, bytes_read);

        int retries = 0;
        while (retries < MAX_RETRIES) // Try sending packets MAX_RETRIWS times
        {
            if (sendto(sockfd, packet, bytes_read + sizeof(int), 0,
                       (struct sockaddr *)clientaddr, clientlen) == -1)
            {
                // printf("sendto failed");
            }

            else
            {
                char ack[16];
                bzero(ack, sizeof(ack));

                /*  receive ACK from server.
                    if its the same as current sequence number,
                    then move to the next chunk
                */
                if (recvfrom(sockfd, ack, sizeof(ack), 0, (struct sockaddr *)clientaddr, &clientlen) > 0)
                {
                    if (atoi(ack) == seq_num)
                        break;
                }

                printf("Retrying sequence no. %d... Remaining tries (%d/%d)\n", seq_num, retries + 1, MAX_RETRIES);
                retries++;
            }
        }

        if (retries == MAX_RETRIES)
        {
            printf("Max retries reached for chunk %d. Aborting...\n", seq_num);
            sendto(sockfd, "FAIL", 4, 0, (struct sockaddr *)clientaddr, clientlen);
            fclose(fp);
        }

        seq_num++;
    }

    disable_timeout(timeout, sockfd);

    // Sending EOF messgage to indicate end of file
    sendto(sockfd, "EOF", 3, 0, (struct sockaddr *)clientaddr, clientlen);
    fclose(fp);

    printf("File sent successfully.\n");
}

/*
    Function to carry out receive file contents with acknowledgement
    Handles packet loss
*/
void receive_file_with_ack(int sockfd, char *filename, struct sockaddr_in *clientaddr)
{
    FILE *fp = fopen(filename, "w");

    char buffer[CHUNKSIZE + sizeof(int)];
    socklen_t clientlen = sizeof(*clientaddr);
    int expected_seq = 0;

    while (1)
    {
        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                      (struct sockaddr *)clientaddr, &clientlen);
        if (bytes_received < 0)
            printf("recvfrom failed\n");

        // if message is EOF -> indicates end of file that is being sent
        if (strncmp(buffer, "EOF", 3) == 0)
        {
            printf("File received successfully.\n");
            break;
        }

        // if message is FAIL -> indicates file was not saved on server successfully
        if (strncmp(buffer, "FAIL", 4) == 0)
        {
            printf("File not received successfully. Please try again.\n");

            char buffer1[100];
            bzero(buffer1, sizeof(buffer1));

            sprintf(buffer1, "Put %s not successful!", filename);

            int n;
            n = sendto(sockfd, buffer1, strlen(buffer1), 0,
                       (struct sockaddr *)&clientaddr, clientlen);
            // if (n < 0)
            //     printf("ERROR in sendto");

            remove(filename); // remove file created since content is wrong or empty
            break;
        }

        int received_seq;
        memcpy(&received_seq, buffer, sizeof(int));
        // printf("Received seq %d/%d\n", received_seq, expected_seq);

        // checking if sequence number is correct or packet loss occured
        if (received_seq == expected_seq)
        {
            // saving content into file, if there is no packet loss
            fwrite(buffer + sizeof(int), 1, bytes_received - sizeof(int), fp);
            expected_seq++;
        }

        char ack[16];
        bzero(ack, sizeof(ack));

        // sending back ACK = received sequence number
        sprintf(ack, "%d", received_seq);
        sendto(sockfd, ack, strlen(ack), 0, (struct sockaddr *)clientaddr, clientlen);
    }

    fclose(fp);
}
