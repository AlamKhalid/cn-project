#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define BUFFER_SIZE 500
#define PORT 2500
#define CWND_SIZE 5
#define fileName "output.mp4"

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
    int sock, length, n, size, receivedBytes, remain, totalAcks = 0, sent, i, nAcks;
    // sock for socket, lenght to store socket length, n for size of sent data
    char clientAddress[20];
    struct sockaddr_in server;
    FILE *file;
    struct packet packets[CWND_SIZE], pktAck, pktAcks[CWND_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        error("Failed to create socket");
        exit(-1);
    }

    length = sizeof(struct sockaddr_in);
    bzero((char *)&server, length);

    server.sin_family = AF_INET;
    //INADDR_ANY is to get the ip address of PC automatically
    server.sin_addr.s_addr = INADDR_ANY;
    //htons converts into network understandable format
    server.sin_port = htons(PORT);
    //bind
    if ((bind(sock, (struct sockaddr *)&server, length)) < 0)
    {
        error("bind failed");
        exit(-1);
    }

    file = fopen(fileName, "w");
    recvfrom(sock, &size, sizeof(off_t), 0, (struct sockaddr *)&server, &length);
    remain = size;
    printf("Receiving File Size: %d\n", size);
    n = 1;

    while (n != 0 && remain > 0)
    {
        do
        {

            // Loop for receiving packets
            for (i = 0; i < CWND_SIZE; i++)
            {
                n = recvfrom(sock, &pktAck, sizeof(struct packet),
                             0, (struct sockaddr *)&server, &length);
                if (n > 0)
                {
                    // Check if it is end flag or not
                    if (strcmp(pktAck.data, TERMINATION_FLAG) == 0)
                    {
                        n = 0;
                        break;
                    }
                    // reordering of packets using seq number
                    packets[pktAck.seq - 1] = pktAck;
                }
            }

            // Loop for sending Acks
            nAcks = 0;
            for (i = 0; i < CWND_SIZE; i++)
            {

                // Check if corresponding packet was received or not
                strcpy(pktAcks[i].data, "ACK");
                pktAcks[i].seq = i + 1;
                sent = sendto(sock, &pktAcks[i], sizeof(pktAcks[i]), 0, (struct sockaddr *)&server, length);
                if (sent > 0)
                {
                    nAcks++;
                    totalAcks++;
                    printf("ACK %d sent\n", pktAcks[i].seq);
                }
            }
        } while (nAcks < CWND_SIZE);

        // Loop for writing data into file
        for (int i = 0; i < CWND_SIZE; i++)
        {
            // Check if there is data in packet
            if (packets[i].size != 0)
            {
                printf("Writing packet: %d\n", packets[i].seq);
                fwrite(packets[i].data, 1, packets[i].size, file);
                receivedBytes += packets[i].size;
                remain -= packets[i].size;
            }
        }

        // Zeroing array of packets
        memset(packets, 0, sizeof(packets));

        // Print received and remaining bytes
        printf("Total Acks: %d\n", totalAcks);
        printf("Total received bytes: %d\nRemaining data: %d bytes\n", receivedBytes, remain);
    }
    printf("File received fully\n");
    printf("Data remaining is %d bytes\n", remain);

    // Close socket and file
    close(sock);
    fclose(file);

    return 0;
}