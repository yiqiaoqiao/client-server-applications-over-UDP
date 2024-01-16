#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    //struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    //==============================================================
    // TODO: Receive file from the client and save it as output.txt

    // char buffer[PAYLOAD_SIZE];
    //socklen_t addr_size;

    bool is_last = false;

    while(1){
        addr_size = sizeof(client_addr_from);
        //Get packet
        recv_len = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr_from, &addr_size);
        //Check for errors
        if (recv_len < 0) {
            perror("Error sending data to the server");
            return 1;
        }
        //Check for last packet
        is_last = (ack_pkt.last==1);
        
        //Write the payload if packet is in sequence
        if (ack_pkt.seqnum == expected_seq_num) {
            fprintf(fp, "%s", ack_pkt.payload);
        }
        //Handle duplicate packets
        else if (ack_pkt.seqnum < expected_seq_num) {
            int q;
            char buffer[PAYLOAD_SIZE];
            memcpy(buffer, (char*)&ack_pkt.acknum, sizeof(unsigned int));
            struct packet p;
            build_packet(&p, ack_pkt.seqnum, ack_pkt.acknum, 0, 1, PAYLOAD_SIZE, buffer);
            //Send packet
            q = sendto(send_sockfd, &p, sizeof(p), 0, (struct sockaddr*)&client_addr_to, sizeof(client_addr_to));
            if (q == -1) 
                perror("Error sending ACK to the client");
            else
                bzero(buffer, PAYLOAD_SIZE);
            continue;
        }
        //Send ACK for received packet
        int q;
        char buffer[PAYLOAD_SIZE];
        memcpy(buffer, (char*)&ack_pkt.acknum, sizeof(unsigned int));
        struct packet p;
        build_packet(&p, ack_pkt.seqnum, ack_pkt.acknum, 0, 1, PAYLOAD_SIZE, buffer);
        //Send packet
        q = sendto(send_sockfd, &p, sizeof(p), 0, (struct sockaddr*)&client_addr_to, sizeof(client_addr_to));
        if (q == -1) 
            perror("Error sending ACK to the client");
        else
            bzero(buffer, PAYLOAD_SIZE);
        
        //Break the loop if the last packet has been received.
        if(is_last)
            break;

        //update expected sequence number
        //expected_ack_num = ack_pkt.acknum+1;
        expected_seq_num = ack_pkt.seqnum+1;

    }


    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;

}
