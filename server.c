
#include "protocol.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define MAX_BUFFER_LEN 1600
#define MAX_ID_LEN      10
#define INITIAL_CLIENTS_CAP 100

#define MSG_TYPE_SUBSCRIBE   1
#define MSG_TYPE_UNSUBSCRIBE 2
#define MSG_TYPE_MESSAGE     3

typedef struct {
    int fd;
    char id[MAX_ID_LEN + 1];
    struct sockaddr_in addr;
    int topics_count;
    char **topics;
} client_t;

// verificam daca un client e deja abonat la topic
bool is_subscribed(const client_t *client, char *topic) {
    if (!client->topics || client->topics_count <= 0) {
        return false;
    }
    
    // iteram prin topic urile clientului
    for (int i = 0; i < client->topics_count; i++) {
        if (strcmp(client->topics[i], topic) == 0) {
            return true; // clientul e abonat la topic
        }
    }
    // clientul nu e abonat la topic
    return false;
}

// adaugam un client la topic
 void subscribe_client(client_t *client,char *topic) {
   // daca clientul e deja abonat la topic dam return
    if (is_subscribed(client, topic))
        return;
    
    // daca clientul nu  e abonat la topic, adaugam un topic nou
    client->topics = realloc(client->topics, (client->topics_count + 1) * sizeof(char*));
    client->topics[client->topics_count] = strdup(topic);
    client->topics_count++;
}

// ne dezabonam de la un topic
 void unsubscribe_client(client_t *client, char *topic) {
    for (int i = 0; i < client->topics_count; i++) {
        if (strcmp(client->topics[i], topic) == 0) {
            free(client->topics[i]);
            if (i < client->topics_count - 1) {
                client->topics[i] = client->topics[client->topics_count - 1];
            }
            client->topics_count--;
            return;
        }
    }
}

// gasim un client dupa id
 int find_client_by_id(client_t *clients, int count,  char *id) {
    for (int i = 0; i < count; i++) {
        if (strcmp(clients[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

// rtimite un mesaj catre un client
 void send_msg_to_client(client_t *client, udp_message_t *udp_message, struct sockaddr_in *src_addr) {
    // obtinem lungimea payload ului
    size_t payload_len = udp_message->available_content_len;
    // calculam lungimea mesajului
    size_t msg_len = sizeof(uint8_t) + sizeof(struct sockaddr_in) + MAX_TOPIC_LEN + 1 + payload_len;
    size_t total_len = sizeof(uint16_t) + msg_len;

    char *buffer = malloc(total_len);
    // convertim lungimea mesajului in formatul retelei si copiem lungimea mesajului in buffer
    uint16_t net_len = htons((uint16_t)msg_len);
    memcpy(buffer, &net_len, sizeof(net_len));
    buffer[sizeof(uint16_t)] = MSG_TYPE_MESSAGE;
    memcpy(buffer + sizeof(uint16_t) + 1, src_addr, sizeof(*src_addr));// copiam adr in buffer
    
    // verificam ca lungimea topicului sa nu depaseasca lungimea maxima permisa
    if (strlen(udp_message->topic) > MAX_TOPIC_LEN) {
        fprintf(stderr, "Topic too long: %s\n", udp_message->topic);
        free(buffer);
        return;
    }
    // copiam topicul in buffer
    memcpy(buffer + sizeof(uint16_t) + 1 + sizeof(*src_addr), udp_message->topic, MAX_TOPIC_LEN);
    buffer[sizeof(uint16_t) + 1 + sizeof(*src_addr) + MAX_TOPIC_LEN] = udp_message->data_type;
    memcpy(buffer + sizeof(uint16_t) + 1 + sizeof(*src_addr) + MAX_TOPIC_LEN + 1, 
           udp_message->content, payload_len);
    
    if (send(client->fd, buffer, total_len, 0) < 0) // trimitem mesajul catre client
        perror("send");
    
    free(buffer);
}

int main(int argc, char *argv[]) {
    int port = atoi(argv[1]);
    
    // dezactivam buffering ul
    setvbuf(stdout, NULL, _IONBF, 0);
    
    // cream socket udp
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in serv_udp = {0};
        serv_udp.sin_family = AF_INET;
        serv_udp.sin_port = htons(port);
        serv_udp.sin_addr.s_addr = INADDR_ANY;
        // facem bind socket ului de UDP
        if (bind(udp_socket, (struct sockaddr*)&serv_udp, sizeof(serv_udp)) < 0) {
            perror("bind udp"); 
            close(udp_socket);
            udp_socket = -1;
        }

    // cream socket tcp
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        // dezactivam algortimul lui Nagle pentru socket ul tcp
        int flag = 1;
        setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

        struct sockaddr_in serv_tcp = {0};
        serv_tcp.sin_family = AF_INET;
        serv_tcp.sin_port  = htons(port);
        serv_tcp.sin_addr.s_addr = INADDR_ANY;
        // facem bind socket ului de TCP
        if (bind(tcp_socket, (struct sockaddr*)&serv_tcp, sizeof(serv_tcp)) < 0) {
            perror("bind tcp"); 
            close(tcp_socket);
            tcp_socket = -1;   
        }

        // ascultam conexiuni pentru tcp
        if (listen(tcp_socket, SOMAXCONN) < 0) {
            perror("listen"); 
            close(tcp_socket);
            tcp_socket = -1;
        }
    
    // vector pentru a stoca informatii despre client
    client_t *clients = malloc(INITIAL_CLIENTS_CAP * sizeof(client_t));
    int nr_cl = 0;
    int clients_cap = INITIAL_CLIENTS_CAP;

    fd_set read_fds, tmp_fds;
    FD_ZERO(&read_fds);
    
    // adaugam socket ul udp, tcp si stdin in setul de descriptori pentru fisiere
    FD_SET(udp_socket, &read_fds);
    FD_SET(tcp_socket, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    
    int fdmax = -1;
    if (udp_socket > fdmax) 
        fdmax = udp_socket;
    if (tcp_socket > fdmax) 
        fdmax = tcp_socket;
    
    // buffer pentru a stoca datele primite
    char buffer[MAX_BUFFER_LEN];
    while (1) {
        tmp_fds = read_fds;

        //vedem pe ce socket uri s a scris in aceasta parcurgere
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            continue;
        }

        // verificam daca avem input de la stdin
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            if (fgets(buffer, sizeof(buffer), stdin)) {
                if (strncmp(buffer, "exit", 4) == 0) {
                    break;
                }
            }
        }

        // verificam daca avem o noua conexiune tcp
        if (tcp_socket >= 0 && FD_ISSET(tcp_socket, &tmp_fds)) {
            struct sockaddr_in cli_addr;
            socklen_t len = sizeof(cli_addr);
            int newfd = accept(tcp_socket, (struct sockaddr*)&cli_addr, &len);
            if (newfd >= 0) {
                int flag = 1;
                setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                
                // citim id ul clientului
                uint16_t net_idlen;
                if (recv(newfd, &net_idlen, sizeof(net_idlen), 0) != sizeof(net_idlen)) {
                    close(newfd);
                } else {
                    int idlen = ntohs(net_idlen);
                    if (idlen > 0 && idlen <= MAX_ID_LEN && recv(newfd, buffer, idlen, 0) == idlen) {
                        buffer[idlen] = '\0';
                        int client_idx = find_client_by_id(clients, nr_cl, buffer);
                        if (client_idx == -1) {
                            if (nr_cl == clients_cap) {
                                clients_cap *= 2;
                                client_t *temp = realloc(clients, clients_cap * sizeof(client_t));
                                if (temp == NULL) {
                                    perror("realloc clients");
                                    exit(EXIT_FAILURE);
                                }
                                clients = temp;
                            }
                            client_t client = {0};
                            client.fd = newfd;
                            client.addr = cli_addr;
                            client.topics_count = 0;
                            client.topics = NULL;
                            strncpy(client.id, buffer, MAX_ID_LEN);
                            client.id[MAX_ID_LEN] = '\0';
                            
                            clients[nr_cl++] = client;
                            FD_SET(newfd, &read_fds);
                            if (newfd > fdmax) fdmax = newfd;
                            
                            char client_addr[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, &(cli_addr.sin_addr), client_addr, INET_ADDRSTRLEN);
                            printf("New client %s connected from %s:%d.\n", 
                                  buffer, client_addr, ntohs(cli_addr.sin_port));
                        } else {
                            printf("Client %s already connected.\n", buffer);
                            close(newfd);
                        }
                    } else {
                        close(newfd);
                    }
                }
            }
        }

        // verificam daca avem un mesaj udp
        if (udp_socket >= 0 && FD_ISSET(udp_socket, &tmp_fds)) {
            struct sockaddr_in sender;
            socklen_t slen = sizeof(sender);
            int n = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &slen);
            if (n > 0) {
                // cream mesaj udp, extragem topic ul, tipul datelor si continutul
                udp_message_t udp_message = {0};
                memcpy(udp_message.topic, buffer, MAX_TOPIC_LEN);
                udp_message.topic[MAX_TOPIC_LEN] = '\0';
                udp_message.data_type = buffer[MAX_TOPIC_LEN];
                udp_message.available_content_len = n - MAX_TOPIC_LEN - 1;
                memcpy(udp_message.content, buffer + MAX_TOPIC_LEN + 1, udp_message.available_content_len);
                
                // trimitem msg clientilor abonati
                for (int i = 0; i < nr_cl; i++)
                    if (is_subscribed(&clients[i], udp_message.topic))
                        send_msg_to_client(&clients[i], &udp_message, &sender);
            }
        }

        // verificam activitatea clientilor tcp
        for (int i = 0; i < nr_cl; i++) {
            int fd = clients[i].fd;
            if (!FD_ISSET(fd, &tmp_fds)) 
                continue;

            uint16_t net_len;
            int r = recv(fd, &net_len, sizeof(net_len), 0);
            if (r <= 0) {
                printf("Client %s disconnected.\n", clients[i].id);
                close(fd);
                FD_CLR(fd, &read_fds);
                for (int j = 0; j < clients[i].topics_count; j++)
                    free(clients[i].topics[j]);
                free(clients[i].topics);
                clients[i] = clients[--nr_cl];
                i--;
                continue;
            }

            int len = ntohs(net_len);
            if (len <= 0 || len > MAX_BUFFER_LEN - sizeof(net_len))
                continue;
            
            memset(buffer, 0, sizeof(buffer));
            if (recv(fd, buffer, len, 0) != len) 
                continue;

            uint8_t type = buffer[0];
            char *payload = buffer + 1;

            // procesam mesajele subscribe/ unsubscribe
            if (type == MSG_TYPE_SUBSCRIBE) {
                char topic[MAX_TOPIC_LEN+1];
                int plen = len - 1;
                if (plen > MAX_TOPIC_LEN) 
                    plen = MAX_TOPIC_LEN;
                memcpy(topic, payload, plen);
                topic[plen] = '\0';
                subscribe_client(&clients[i], topic);

                // Trimitem confirmarea
                size_t pl = strlen(topic) + 1;
                size_t msg_sz = sizeof(uint16_t) + sizeof(uint8_t) + pl;
                char *confirm = malloc(msg_sz);
            
                uint16_t conf_len = htons((uint16_t)(1 + pl));
                memcpy(confirm, &conf_len, sizeof(conf_len));
                confirm[sizeof(uint16_t)] = MSG_TYPE_SUBSCRIBE;
                memcpy(confirm + sizeof(uint16_t) + 1, topic, pl);
                send(fd, confirm, msg_sz, 0);
                free(confirm);
            }
            else if (type == MSG_TYPE_UNSUBSCRIBE) {
                char topic[MAX_TOPIC_LEN+1];
                int plen = len - 1;
                if (plen > MAX_TOPIC_LEN) plen = MAX_TOPIC_LEN;
                memcpy(topic, payload, plen);
                topic[plen] = '\0';
                unsubscribe_client(&clients[i], topic);

                // Trimitem confirmarea
                size_t pl = strlen(topic) + 1;
                size_t msg_sz = sizeof(uint16_t) + sizeof(uint8_t) + pl;
                char *confirm = malloc(msg_sz);
            
                uint16_t conf_len = htons((uint16_t)(1 + pl));
                memcpy(confirm, &conf_len, sizeof(conf_len));
                confirm[sizeof(uint16_t)] = MSG_TYPE_UNSUBSCRIBE;
                memcpy(confirm + sizeof(uint16_t) + 1, topic, pl);
                send(fd, confirm, msg_sz, 0);
                free(confirm);
            }
        }
    }

    for (int i = 0; i < nr_cl; i++) {
        close(clients[i].fd);
        for (int j = 0; j < clients[i].topics_count; j++) {
            free(clients[i].topics[j]);
        }
        free(clients[i].topics);
    }

    close(udp_socket);
    close(tcp_socket);
    return 0;
}
