Ciurea Ana-Sorina 321 CC 
 # Tema 2 Aplicatie client-server TCP si UDP pentru gesionarea mesajelor

## Proiectul contine urmatoarele fisiere :
- protocol.h 
- server.c 
- subscriber.c 
- Makefile (pentru compilarea proiectului: exista si regula de make si de clean)

## protocol.h
    – struct udp_message_t:
        · topic[50]      – nume al topic-ului
        · data_type      – cod de interpretare (0=INT, 1=SHORT_REAL, 2=FLOAT, 3=STRING)
        · payload[1500]  – datele brute

    – struct tcp_message_t:
        · len_net        – lungimea totala 
        · type           – tip mesaj TCP
        · payload[]      – topic + continut

## server.c
- Parseaza portul din linia de comanda si verifica validitatea acestuia
- Creeaza un socket UDP si bind() pe portul specificat
- Creeaza un socket TCP, bind() pe acelasi port, apoi listen()
- Initializeaza fd_sets pentru select(): include UDP socket, TCP listen si STDIN optional
- Initializeaza vectorul de clienti cu o capacitate initiala (INITIAL_CLIENTS_CAP = 100)
- Intra intr-un while si apeleaza select()
- Daca UDP socket e gata, primeste udp_message_t cu recvfrom()
- Extrage topic, data_type si payload din udp_message_t
- Converteste payload-ul in text ("TOPIC") in functie de data_type
- Construieste un tcp_message_t
- Pentru fiecare client abonat la topic, trimite tcp_message_t cu send()
- Daca TCP listen socket e gata, accepta() conexiuni noi si verifica dacă clientul există deja (după ID)
- Daca nu exista, verifica dacă este depasita capacitatea curenta (nr_cl == clients_cap)
- Dacă da, dubleaza capacitatea (clients_cap *= 2) și realocă vectorul de clienți
- Adauga noul client in vector si socket-ul in fd_set 

- In plus avem urmatoarele functii pentru claritatea codului:
    bool is_subscribed(const client_t *client, char *topic) :
      Verifica daca un client este deja abonat la un anumit topic.
      -Daca lista de topicuri (client->topics) e nula sau topics_count <= 0, returneaza false.
      -Altfel itereaza prin sirul de topicuri si compara fiecare element cu topic.
      -Returneaza true daca gaseste un match, false in caz contrar.

    void subscribe_client(client_t *client,char *topic) :
      Adauga un topic nou la lista de abonamente a clientului, daca nu este deja abonat.
      -Apeleaza is_subscribed si iese imediat daca deja exista abonamentul.
      -Foloseste realloc pentru a extinde vectorul de char* cu un element in plus.
      -Copiaza sirul topic folosind strdup si actualizeaza topics_count.

    void unsubscribe_client(client_t *client, char *topic) :
    Sterge un topic din lista de abonamente a clientului.
    -Cauta in vectorul de topicuri elementul egal cu topic.
    -Daca nu este ultimul element, suprascrie pozitia curenta cu ultimul sir din vector.

    int find_client_by_id(client_t *clients, int count,  char *id) :
    Gaseste indexul unui client intr-un tablou de clienti, dupa id.
    -Parcurge tabloul de clienti cu lungime count.
    -Compară clients[i].id cu id folosind strcmp.
    -Returneaza indexul daca gaseste potrivirea, altfel -1.  

    void send_msg_to_client(client_t *client, udp_message_t *udp_message, struct sockaddr_in *src_addr) :
    Construieşte si trimite un mesaj UDP catre un client.

 ##  subscriber.c
- Parseaza argumentele din linia de comanda: CLIENT_ID, SERVER_IP, SERVER_PORT  
- Creeaza un socket TCP (socket())  
- Se conecteaza la server folosind connect()  
- Dezactiveaza Nagle cu setsockopt(TCP_NODELAY) pentru transmitere imediata  
- Trimite lungimea ID-ului si ID-ul clientului catre server  
- Initializeaza fd_set pentru select() pe STDIN si socket-ul TCP  
- Intra intr-un loop principal si apeleaza select()  
- Daca STDIN e gata, citeste o linie cu fgets()  
- Daca linia incepe cu “subscribe ”, construieste tcp_message_t cu type=0 si topic, apoi send()  
- Daca linia incepe cu “unsubscribe ”, construieste tcp_message_t cu type=1 si topic, apoi send()  
- Daca linia este “exit”, inchide socket-ul si iese din loop  
- Daca socket-ul TCP are date, citeste intai header-ul len_net si type cu recv()  
- Calculeaza payload_len = ntohs(len_net) - sizeof(type) - sizeof(len_net) si recv() payload-ul   
- La recv()==0 sau eroare, afiseaza mesaj de deconectare si inchide socket-ul

Dificultati intampinate:
- A fost complicat de testat ce nu merge.
- Conversia intre host si network byte order pentru campul len_net.
- Lipsa implementarii wildcard-urilor in subscrieri: Nu am implementat partajarea topic-urilor cu “*” sau “?” pentru abonamente generice, desi ar fi util pentru flexibilitate.

Pentru o intelegere mai buna a conceptelor am folosit urmatoarele surse:
- socket https://www.geeksforgeeks.org/socket-programming-cc/ si https://pubs.opengroup.org/onlinepubs/9699919799/functions/socket.html
- functia bind https://pubs.opengroup.org/onlinepubs/9699919799/functions/bind.html
- https://www.geeksforgeeks.org/udp-server-client-implementation-c/

- laboratoarele 6, 7, 8 https://pcom.pages.upb.ro/labs/lab6/lecture.html
