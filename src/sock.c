#include <sock.h>
#include <file.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef WIN
int init_win_sock(void) {
    WSADATA d;
    int iResult = WSAStartup(MAKEWORD(2, 2), &d);
    if (iResult != 0) {
        fprintf(stderr, "Failed to initialize. %d\n", WSAGetLastError());
        return 1;
    }
    return iResult;
}
#endif

//standard function for socket creation
void sock_close(SOCKET sock) {
    #ifdef WIN
        closesocket(sock);
        return;
    #else
        close(sock);
    #endif
}

//create an IPv4 sockaddr_in
void create_address(peer *address, char *ip) {
    char *tokip = strtok(ip, ":");
    char *tokport = strtok(NULL, ":");
    address->con.sin_addr.s_addr = inet_addr(tokip);
    address->con.sin_family = AF_INET;
    address->con.sin_port = htons((unsigned short)(atoi(tokport))); //make string into int, then the int into short, then the short into a network byte order short
    address->status = ONLINE;
}

//bind a server to the socket
bool create_server(SOCKET *server, sockaddr_in address, int opt) {
    *server = socket(AF_INET, SOCK_STREAM, 0);
    if (is_invalid_sock(*server)) {
        printf("Error upon socket creation.\n");
        return false;
    }
    if (setsockopt(*server, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) != 0) {
        printf("Error creating server");
        return false;
    }
    if (bind(*server, (const struct sockaddr *)&address, sizeof(address)) != 0) {
        printf("Error binding server");
        return false;
    }
    return true;
}

//create peers
peer * create_peers(char **peers_ip, size_t peers_size) {
    peer * peers = malloc(sizeof(peer)* peers_size);
    for (int i = 0; i < peers_size; i++) {
        create_address(&peers[i], peers_ip[i]);
        peers[i].status = OFFLINE;
        printf("Adding new peer %s status OFFLINE\n", peers_ip[i]);
    }
    for(int i = 0; i < peers_size; i++) {
        free(peers_ip[i]);
    }
    free(peers_ip);
    return peers;
}

//print the peers in list
void show_peers(peer server, SOCKET server_soc, int *clock, peer *peers, size_t peers_size) {
    printf("List peers:\n");
    printf("\t[0] Go back\n");
    for(int i = 0; i < peers_size; i++) {
        printf("\t[%d] %s:%d\n", i+1, inet_ntoa(peers[i].con.sin_addr), peers[i].con.sin_port);
    }
    int input;
    scanf("%d", &input);
    if(input == 0) return;
    else if(input > 0 && input <= peers_size) {
        char *msg = build_message(server.con, *clock, HELLO, NULL);
        send_message(msg, server_soc, &peers[input-1], HELLO);
        free(msg);
    }
}

//create a message with the sender ip, its clock and the message type
char * build_message(sockaddr_in sender_ip, int clock, MSG_TYPE msg_type, void *args) {
    char *ip = inet_ntoa(sender_ip.sin_addr);
    char *msg = malloc(sizeof(char) * MSG_SIZE);
    switch (msg_type) {
    /*
    MESSAGE FORMATING: <sender_ip>:<sender_port> <sender_clock> <MSG_TYPE> [<ARGS...>]\n <- important newline
    */
    case HELLO:
        sprintf(msg, "%s:%d %d HELLO\n", ip, (int)sender_ip.sin_port, clock);
        break;
    case BYE:
        sprintf(msg, "%s:%d %d BYE\n", ip, (int)sender_ip.sin_port, clock);
        break;
    default:    
        break;
    }
    return msg;
}

//send a built message to an ONLINE known peer socket
void send_message(char *msg, SOCKET server_soc, peer *neighbour, MSG_TYPE msg_type) {
    if(connect(server_soc, (const struct sockaddr*)&(neighbour->con), sizeof(*neighbour)) < 0) {
        printf("Error connecting to peer");
    }
    size_t len = strlen(msg);
    if(send(server_soc, msg, len + 1, 0) == len) {
        switch (msg_type)
        {
            case HELLO:
                neighbour->status = ONLINE;
                break;
            default:
                break;
        }
    }
}

//read message, mark its sender and return the message type
MSG_TYPE read_message(peer receiver, char *buf, int *clock, peer *sender) { //TODO: add args when needed
    char *cpy_msg1 = strdup(buf);
    char *cpy_msg2 = strdup(cpy_msg1);
    char *tok = strtok(cpy_msg1, " ");
    create_address(sender, tok);
    strtok(cpy_msg2, " ");
    tok = strtok(NULL, " ");
    //*clock = atoi(tok);
    tok = strtok(NULL, " ");
    if(strcmp(tok, "HELLO") == 0)
        return HELLO;
    if(strcmp(tok, "BYE") == 0)
        return BYE;
    return UNEXPECTED_MSG_TYPE; //TODO: TREAT ERRORS WHEN RECEIVING MSG
}

//check if peer is in list of known peers
bool peer_in_list(peer a, peer * neighbours, size_t peers_size) {
    for(int i = 0; i < peers_size; i++) {
        if(strcmp(inet_ntoa(a.con.sin_addr), inet_ntoa(neighbours[i].con.sin_addr)) == 0 && (a.con.sin_port == neighbours[i].con.sin_port)) return true;
    }
    return false;
}

//send a bye message to every peer in list
void bye_peers(peer server, SOCKET server_soc, int *clock, peer *peers, size_t peers_size) {
    for(int i = 0; i < peers_size; i++) {
        if(peers[i].status == ONLINE) {
            char *msg = build_message(server.con, *clock, BYE, NULL);
            send_message(msg, server_soc, &peers[i], BYE);
        }
    }
}
