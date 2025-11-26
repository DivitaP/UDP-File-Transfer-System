/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define BUFSIZE 1024
#define CHUNKSIZE 16000

/*
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(0);
}
int initiate_operation_to_server(int sockfd, char *filename, char *op, struct sockaddr_in serveraddr, int serverlen);
void exit_operation_to_server(int sockfd, struct sockaddr_in serveraddr, int serverlen);
void ls_to_server(int sockfd, struct sockaddr_in serveraddr, int serverlen);
void delete_file_from_server(int sockfd, char *buf, struct sockaddr_in serveraddr, int serverlen);
void put_file_to_server(int sockfd, char *filename, struct sockaddr_in serveraddr, int serverlen);
void get_file_from_server(int sockfd, char *filename, struct sockaddr_in serveraddr, int serverlen);
void send_file_with_ack(char *filename, int sockfd, struct sockaddr_in *serveraddr);
void receive_file_with_ack(int sockfd, char *filename, struct sockaddr_in *serveraddr);

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

time_t start_time, end_time;

int main(int argc, char **argv)
{

    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    char input[BUFSIZE];
    char command[16]; // keeping this small since its going to be anything from get/put/delete/ls/exit
    char filename[256];

    /* check command line arguments */
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /*-----------------------------------------------------------------------------------------*/

    while (1)
    {
        printf("Please enter your choice from the following: \n");
        printf("get [filename]\n");
        printf("put [filename]\n");
        printf("delete [filename]\n");
        printf("ls \n");
        printf("exit \n");
        printf("Input: ");
        if (fgets(input, sizeof(input), stdin) != NULL)
        {
            // TODO: improve this if-else thingy
            input[strcspn(input, "\n")] = '\0';

            bzero(command, sizeof(command));
            bzero(filename, sizeof(filename));

            sscanf(input, "%s %s", command, filename);
            int status = initiate_operation_to_server(sockfd, filename, command, serveraddr, sizeof(serveraddr));

            if (status)
            {
                if (strcmp(command, "get") == 0)
                {
                    // printf("GET: %s %s\n", command, filename);
                    get_file_from_server(sockfd, filename, serveraddr, sizeof(serveraddr));
                }
                else if (strcmp(command, "put") == 0)
                {
                    // printf("PUT: %s %s\n", command, filename);
                    put_file_to_server(sockfd, filename, serveraddr, sizeof(serveraddr));
                }
                else if (strcmp(command, "delete") == 0)
                {
                    // printf("DELETE: %s %s\n", command, filename);
                    delete_file_from_server(sockfd, filename, serveraddr, sizeof(serveraddr));
                }
                else if (strcmp(command, "ls") == 0)
                {
                    // printf("LS: %s \n", command);
                    ls_to_server(sockfd, serveraddr, sizeof(serveraddr));
                }
                else if (strcmp(command, "exit") == 0)
                {
                    // printf("EXIT: %s\n", command);
                    exit_operation_to_server(sockfd, serveraddr, sizeof(serveraddr));
                    printf("Exiting from the connection with server \n");
                    break;
                }
                else
                {
                    printf("Invalid input requested. Please enter valid command\n");
                }
            }
        }
    }

    return 0;
}

/*
    Validation to check if file parameter is present for input commands get/put/delete
    This is to prevent segmentation fault / null pointers for file
*/
int checkFileReq(char *op)
{
    return (!strcmp(op, "get") ||
            !strcmp(op, "delete") ||
            !strcmp(op, "put"));
}

/* Validation to check if input command is valid and among (get/put/delete/ls/exit) */
int checkInput(char *op)
{
    return (!strcmp(op, "get") ||
            !strcmp(op, "delete") ||
            !strcmp(op, "put") ||
            !strcmp(op, "ls") ||
            !strcmp(op, "exit"));
}

/*
    This is the entry point for client.
    Client sends the intended input to server and server gets ready for the next steps.
*/
int initiate_operation_to_server(int sockfd, char *filename, char *op, struct sockaddr_in serveraddr, int serverlen)
{

    int is_valid_input = checkInput(op);
    if (!is_valid_input)
    {
        printf("Invalid input requested. Please enter valid command\n");
        return 0;
    }

    int is_file_req = checkFileReq(op);
    if (is_file_req)
    {
        if (filename == NULL)
        {
            printf("Filename is NULL. Please enter valid command.\n");
            printf("--------------------------------------------------------------------------------\n");
            return 0;
        }

        if (strlen(filename) == 0)
        {
            printf("Filename is empty. Please enter valid command.\n");
            printf("--------------------------------------------------------------------------------\n");
            return 0;
        }
    }
    if (!strcmp(op, "put"))
    {
        FILE *file = fopen(filename, "r");
        if (!(access(filename, F_OK) == 0))
        {
            printf("Error opening file. File does not exist. Please enter valid command.\n");
            printf("--------------------------------------------------------------------------------\n");
            return 0;
        }
    }

    printf("Initiating %s command to the server.\n", op);
    int n;

    /* result = op(command) + filename e.g. get abc.txt */

    char result[256];
    bzero(result, sizeof(result));
    sprintf(result, "%s %s", op, filename);

    n = sendto(sockfd, result, strlen(result), 0, (struct sockaddr *)&serveraddr, serverlen);
    if (n < 0)
        error("ERROR in sendto");

    printf("--------------------------------------------------------------------------------\n");
    return 1;
}

void exit_operation_to_server(int sockfd, struct sockaddr_in serveraddr, int serverlen)
{
    int n;

    char buffer1[BUFSIZE];
    bzero(buffer1, sizeof(buffer1));

    n = recvfrom(sockfd, buffer1, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);

    // if (n < 0)
    //     printf("ERROR in recvfrom");

    printf("Reply from server:\n%s\n", buffer1);

    printf("--------------------------------------------------------------------------------\n");
}

void ls_to_server(int sockfd, struct sockaddr_in serveraddr, int serverlen)
{
    int n;

    char buf[BUFSIZE];
    bzero(buf, sizeof(buf));

    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);

    // if (n < 0)
    //     printf("ERROR in recvfrom");

    printf("Reply from server:\n%s\n", buf);

    printf("--------------------------------------------------------------------------------\n");
}

/*
    Receives a message from server, displaying success or failure
*/
void delete_file_from_server(int sockfd, char *buf, struct sockaddr_in serveraddr, int serverlen)
{

    int n;

    char buffer1[BUFSIZE];
    bzero(buffer1, sizeof(buffer1));

    n = recvfrom(sockfd, buffer1, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);

    // if (n < 0)
    //     printf("ERROR in recvfrom");

    printf("Reply from server:\n%s\n", buffer1);
    printf("--------------------------------------------------------------------------------\n");
}

void put_file_to_server(int sockfd, char *filename, struct sockaddr_in serveraddr, int serverlen)
{
    int n;
    time(&start_time);
    send_file_with_ack(filename, sockfd, &serveraddr);

    char buffer1[BUFSIZE];
    bzero(buffer1, sizeof(buffer1));

    n = recvfrom(sockfd, buffer1, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);

    // if (n < 0)
    //     printf("ERROR in recvfrom");
    time(&end_time);
    printf("Reply from server:\n%s\n", buffer1);
    printf("Put file from server took %.2f seconds.\n", difftime(end_time, start_time));

    printf("--------------------------------------------------------------------------------\n");
}

void get_file_from_server(int sockfd, char *filename, struct sockaddr_in serveraddr, int serverlen)
{

    /*  this function is to have RECV TIMEOUT. It helps during packet loss. */
    struct timeval timeout = set_timeout(sockfd, 2); // 2 seconds timeout
    time(&start_time);
    receive_file_with_ack(sockfd, filename, &serveraddr);

    /*  this function is to disable RECV TIMEOUT. It helps during packet loss. */
    disable_timeout(timeout, sockfd);
    time(&end_time);
    printf("Get file from server took %.2f seconds.\n", difftime(end_time, start_time));

    printf("--------------------------------------------------------------------------------\n");
}

void send_file_with_ack(char *filename, int sockfd, struct sockaddr_in *serveraddr)
{
    int MAX_RETRIES = 5; // setting maximum number of tries in case of packet loss

    FILE *fp = fopen(filename, "r");
    if (!fp)
        printf("Error opening file. File does not exist.\n");

    char buffer[CHUNKSIZE];
    bzero(buffer, sizeof(buffer));

    socklen_t serverlen = sizeof(*serveraddr);
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
                       (struct sockaddr *)serveraddr, serverlen) == -1)
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
                if (recvfrom(sockfd, ack, sizeof(ack), 0, (struct sockaddr *)serveraddr, &serverlen) > 0)
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
            printf("Maximum retries reached for sequence no. %d. Aborting...\n", seq_num);
            sendto(sockfd, "FAIL", 4, 0, (struct sockaddr *)serveraddr, serverlen);
            fclose(fp);
        }

        seq_num++;
    }

    disable_timeout(timeout, sockfd);

    // Sending EOF messgage to indicate end of file
    sendto(sockfd, "EOF", 3, 0, (struct sockaddr *)serveraddr, serverlen);
    fclose(fp);

    printf("File sent successfully.\n");
}

/*
    Function to carry out receive file contents with acknowledgement
    Handles packet loss
*/
void receive_file_with_ack(int sockfd, char *filename, struct sockaddr_in *serveraddr)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
        printf("Error creating file");

    /*
        This is logic for receiving packets in chunks. Extracting an integer value (sequence number)
        On server side it would be checked and ACK will be sent
    */
    char buffer[CHUNKSIZE + sizeof(int)];
    bzero(buffer, sizeof(buffer));

    socklen_t serverlen = sizeof(*serveraddr);
    int expected_seq = 0;

    while (1)
    {
        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                      (struct sockaddr *)serveraddr, &serverlen);

        if (bytes_received < 0)
            printf("recvfrom failed\n");

        // if message is DNE -> The file does not exist on server
        if (strncmp(buffer, "DNE", 3) == 0)
        {
            printf("File %s does not exists on server!\n", filename);
            break;
        }

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
            remove(filename);
            break;
        }

        int received_seq;
        memcpy(&received_seq, buffer, sizeof(int));

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
        sendto(sockfd, ack, strlen(ack), 0, (struct sockaddr *)serveraddr, serverlen);
    }
    fclose(fp);
}
