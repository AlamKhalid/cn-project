#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define BUFFER_SIZE 500
#define PORT 2500
#define CWND_SIZE 5

char *TERMINATION_FLAG = ".EOF.";

void error(char *ex)
{
    perror(ex);
    exit(0);
}

struct packet
{
    int seq, size;
    char data[BUFFER_SIZE];
};

int main(int argc, char *argv[])
{
    int sock, length, n, size, ack, acks[CWND_SIZE], packetCount = 0, i, partialFileSize;
    // sock for socket, lenght to store socket length, n for size of sent data
    char fileName[20], serverAddress[] = "127.0.0.1";
    struct sockaddr_in server;
    FILE *file;
    struct packet packets[CWND_SIZE], pktAck;

    /* Get file name from user */
    printf("Enter filename: ");
    scanf("%s", fileName);

    /* Get server address from user */
    // printf("Enter IP address of server(receiver): ");
    // scanf("%s", serverAddress);

    /*Create socket*/
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    /*If value returned is -1, display error*/
    if (sock < 0)
    {
        error("socket failed");
    }

    bzero((char *)&server, sizeof(server));
    server.sin_family = AF_INET;
    inet_pton(AF_INET, serverAddress, &server.sin_addr);
    server.sin_port = htons(PORT);
    length = sizeof(struct sockaddr_in);

    // Opening file with fileName as read-only
    file = fopen(fileName, "r");
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    //send size of file
    printf("\nSending file size which is %d bytes\n", size);
    n = sendto(sock, &size, sizeof(int), 0, (struct sockaddr *)&server, length);
    partialFileSize = 1;
    memset(acks, 0, sizeof(acks));
    printf("Now sending file in packets\n\n");
    do
    {
        // Loop for reading next packets
        n = 1;
        for (i = 0; i < CWND_SIZE; i++)
        {
            partialFileSize = fread(packets[i].data, 1, BUFFER_SIZE, file);
            packets[i].seq = n;
            packets[i].size = partialFileSize;
            n++;
            if (partialFileSize > 0)
                packetCount++;
        }
        // Resend tag
        while (1)
        {
            // Loop for sending packets in the buffer array
            for (i = 0; i < CWND_SIZE; i++)
            {
                if (acks[i] == 0)
                {
                    printf("Sending packet %d\n", packets[i].seq);
                    sendto(sock, &packets[i], sizeof(struct packet), 0, (struct sockaddr *)&server, length);
                }
            }

            // Loop for receiving Acks
            for (i = 0; i < CWND_SIZE; i++)
            {
                ack = recvfrom(sock, &pktAck, sizeof(struct packet), 0, (struct sockaddr *)&server, &length);
                if (strcmp(pktAck.data, "ACK") == 0 && ack > 0)
                {
                    printf("ACK %d received\n", pktAck.seq);
                    acks[pktAck.seq - 1] = 1;
                }
                else
                {
                    break;
                }
            }
            if (i == CWND_SIZE)
                break;
        }
        memset(acks, 0, sizeof(acks));
        memset(packets, 0, sizeof(packets));

    } while (partialFileSize > 0);

    printf("Total packets sent were: %d\n", packetCount);
    // Sending final end packet
    strcpy(pktAck.data, TERMINATION_FLAG);
    printf("Sending termination flag\n");
    sendto(sock, &pktAck, sizeof(struct packet), 0, (struct sockaddr *)&server, length);
    // Closing socket and file
    fclose(file);
    close(sock);
    return 0;
}
