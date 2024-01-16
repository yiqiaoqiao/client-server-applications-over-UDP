#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>

#include "utils.h"

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    socklen_t addr_size_from = sizeof(server_addr_from);
    struct timeval tv;
    struct packet pkt;
    //struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    //char last = 0;
    //char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, addr_size);
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    //==============================================================
    // TODO: Read from file, and initiate reliable data transfer to the server
    int recv_len;
    tv.tv_sec = 0;
    tv.tv_usec = 1000;
    bool last_packet_sent = false;
    struct packet last_packet;

    while(!last_packet_sent) {
        if (!last_packet_sent && fgets(buffer, PAYLOAD_SIZE, fp) == NULL) {
            // Build the last packet
            build_packet(&last_packet, seq_num, ack_num, 1, 0, PAYLOAD_SIZE, buffer);
            last_packet_sent = true;
        } else{
            // Build a regular packet
            build_packet(&pkt, seq_num, ack_num, 0, 0, PAYLOAD_SIZE, buffer);
        }

        // Send the packet
        struct packet *packet_to_send = last_packet_sent ? &last_packet : &pkt;
        socklen_t size = last_packet_sent ? PAYLOAD_SIZE : sizeof(*packet_to_send);
        recv_len = sendto(send_sockfd, packet_to_send, size, 0, (struct sockaddr*)&server_addr_to, addr_size);
        if (recv_len < 0) {
            perror("Error sending data to the server");
            return 1;
        }

        // Wait for ACK
        while (1){
            int ack = -1;
            struct packet temp;
            setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
            recv_len = recvfrom(listen_sockfd, &temp, sizeof(temp), 0, (struct sockaddr*)&server_addr_from, &addr_size_from);
            if (recv_len < 0) {
                perror("Error receiving ACK from server\n");
                ack = -1;
            }
            else
                ack = temp.acknum;
            if (ack == -1 && last_packet_sent){
                printSend(&last_packet, 1);
                recv_len = sendto(send_sockfd, &last_packet, PAYLOAD_SIZE, 0, (struct sockaddr*)&server_addr_to, addr_size);
                if (recv_len < 0) {
                    perror("Error sending data to the server");
                    return 1;
                }
                continue;
            } else if(ack != -1 && ack == ack_num && !last_packet_sent){
                //ack received, move on to next packet
                break;
            }
            recv_len = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, addr_size);
        }
        bzero(buffer, PAYLOAD_SIZE);
        seq_num++;
        ack_num++;
    }


    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

