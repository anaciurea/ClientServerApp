#include "protocol.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_BUFFER_LEN 1600
#define MAX_CMD_LEN    256

#define MSG_TYPE_SUBSCRIBE   1
#define MSG_TYPE_UNSUBSCRIBE 2
#define MSG_TYPE_MESSAGE     3

int main(int argc, char *argv[]) {
    char *client_id = argv[1];
    char *server_ip = argv[2];
    int server_port = atoi(argv[3]);

    //dezactivam buffering ul pentru stdout
    setvbuf(stdout, NULL, _IONBF, 0);

    // creeam un socket TCP
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    
    // dezactivăm algoritmul Nagle
    int flag = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        perror("setsockopt");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // configurama adresa
    struct sockaddr_in serv_addr;
    // initializez structura cu 0
    memset(&serv_addr, 0, sizeof(serv_addr));
    // setez familia de adr la ipv4
    serv_addr.sin_family = AF_INET;
    // setez nr portului
    serv_addr.sin_port = htons(server_port);
    
    // convertesc adresa ip din sirul de caract in forma binara
    if (inet_aton(server_ip, &serv_addr.sin_addr) == 0) {
        fprintf(stderr, "Invalid IP address\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // ne conectam la server
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // trimitem id ul clientului la server
    uint16_t id_len = strlen(client_id);
    // convertim la ordinea de biti a retelei
    uint16_t id_len_net = htons(id_len);
    
    // trimite id ul si lungimea
    if (send(sockfd, &id_len_net, sizeof(id_len_net), 0) < 0 || send(sockfd, client_id, id_len, 0) < 0) {
        perror("send client ID");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // multiplexare intre intearea de la tastatura si mesajele de la server
    fd_set read_fds, tmp_fds;
    // initializam setul de descriptori de la fisiere
    FD_ZERO(&read_fds);

    // adaugam stdin ul si socket ul in setul de descriptori de fisiere
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(sockfd, &read_fds);

    int max_fd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

    char buffer[MAX_BUFFER_LEN], command[MAX_CMD_LEN];
    
    while (1) {
        tmp_fds = read_fds;
        
        // vedem cine a scris sau de unde am citit in aceasta parcurgere
        if (select(max_fd + 1, &tmp_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // verificăm intrarea de la utilizator
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            if (!fgets(command, sizeof(command), stdin))
                break; // EOF
            
            command[strcspn(command, "\r\n")] = '\0';
            
            if (strcmp(command, "exit") == 0)
                break;

            // verificam daca vrem sa ne abonam la un anumit topic
            if (strncmp(command, "subscribe ", 10) == 0) {
                // extragem topicul din comanda, calculam lungimea topicului si al mesajului, 
                // convertim la ordinea de biti a reteli
                char *topic = command + 10;
                size_t topic_len = strlen(topic) + 1;
                uint16_t msg_len = 1 + topic_len;
                uint16_t msg_len_net = htons(msg_len); // Convert to network byte order
                
                char *msg = malloc(sizeof(uint16_t) + 1 + topic_len);
                if (msg) {
                    memcpy(msg, &msg_len_net, sizeof(uint16_t)); // Use msg_len_net
                    msg[sizeof(uint16_t)] = MSG_TYPE_SUBSCRIBE;
                    memcpy(msg + sizeof(uint16_t) + 1, topic, topic_len);
                    if(send(sockfd, msg, sizeof(uint16_t) + 1 + topic_len, 0)<0){
                        perror("send");
                        free(msg);
                        break;
                    }
                    printf("Subscribed to topic %s\n", topic);
                    free(msg);
                }
            } 

             // verificam daca vrem sa ne dezabonam la un anumit topic
            else if (strncmp(command, "unsubscribe ", 12) == 0) {
                char *topic = command + 12;
                size_t topic_len = strlen(topic) + 1;
                uint16_t msg_len = 1 + topic_len;
                uint16_t msg_len_net = htons(msg_len); // Convert to network byte order
                
                char *msg = malloc(sizeof(uint16_t) + 1 + topic_len);
                if (msg) {
                    memcpy(msg, &msg_len_net, sizeof(uint16_t)); // Use msg_len_net
                    msg[sizeof(uint16_t)] = MSG_TYPE_UNSUBSCRIBE;
                    memcpy(msg + sizeof(uint16_t) + 1, topic, topic_len);
                     if(send(sockfd, msg, sizeof(uint16_t) + 1 + topic_len, 0)<0){
                        perror("send");
                        free(msg);
                        break;
                    }
                    printf("Unsubscribed from topic %s\n", topic);
                    free(msg);
                }
            }
        }

        // verificăm mesajele de la server
        if (FD_ISSET(sockfd, &tmp_fds)) {
            // citim lungimea mesajului
            uint16_t msg_len_net;
            int recv_result = recv(sockfd, &msg_len_net, sizeof(msg_len_net), 0);
            
            if (recv_result <= 0) {
                // serverul s-a deconectat sau eroare
                if (recv_result == 0) {
                    printf("Server closed connection.\n");
                } else {
                    perror("recv");
                }
                break;
            }
            
            // convertim lungimea la ordinea octetilor din host
            uint16_t msg_len = ntohs(msg_len_net);
            if (msg_len <= 0 || msg_len > MAX_BUFFER_LEN - sizeof(msg_len_net)) {
                fprintf(stderr, "Invalid message length: %u\n", msg_len);
                continue;
            }
            
            // citim continutul mesajului
            if (recv(sockfd, buffer, msg_len, 0) != msg_len) {
                perror("recv message");
                continue;
            }
            
            // procesam mesajul : extragem tipul mesajului si payload ul
            uint8_t msg_type = buffer[0];
            char *payload = buffer + 1;

            if (msg_type == MSG_TYPE_MESSAGE) {
                struct sockaddr_in *sender_addr = (struct sockaddr_in*)payload;
                payload += sizeof(*sender_addr);
                
                // buffer pentru topic
                char topic[MAX_TOPIC_LEN + 1];
                memcpy(topic, payload, MAX_TOPIC_LEN);
                topic[MAX_TOPIC_LEN] = '\0';
                payload += MAX_TOPIC_LEN;
                
                // extragem tipul datelor
                uint8_t data_type = *payload++;
                
                // buffer pentru ip ul expeditorului
                char sender_ip[INET_ADDRSTRLEN];
                // convertim adr in string
                inet_ntop(AF_INET, &(sender_addr->sin_addr), sender_ip, INET_ADDRSTRLEN);
                // extragem portul
                uint16_t sender_port = ntohs(sender_addr->sin_port);
                
                // in functie de tipul de date primit, facem afisarea conform cerintei
                if (data_type == 0) {
                    // extragem semnul, valoarea si afisam output ul
                    uint8_t sign = *payload++;
                    uint32_t value = ntohl(*(uint32_t*)payload);
                    printf("%s:%d - %s - INT - %s%u\n",
                           sender_ip, sender_port, topic,
                           sign ? "-" : "", value);
                } else if (data_type == 1) {
                    uint16_t value = ntohs(*(uint16_t*)payload);
                    printf("%s:%d - %s - SHORT_REAL - %.2f\n",
                           sender_ip, sender_port, topic,
                           value / 100.0f);
                } else if (data_type == 2) {
                    uint8_t sign = *payload++;
                    uint32_t value = ntohl(*(uint32_t*)payload);
                    payload += sizeof(uint32_t);
                    uint8_t power = *payload;
                    
                    double real_value = value;
                    for (int i = 0; i < power; i++) {
                        real_value /= 10.0;
                    }
                    
                    printf("%s:%d - %s - FLOAT - %s%.*f\n",
                           sender_ip, sender_port, topic,
                           sign ? "-" : "", power, real_value);
                } else if (data_type == 3) {
                    printf("%s:%d - %s - STRING - %s\n",
                           sender_ip, sender_port, topic, payload);
                }
            }
        }
    }
    close(sockfd);
    return 0;
}