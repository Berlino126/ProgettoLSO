#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

// Costanti di configurazione
#define CLIENTS_LIMIT 10      // Numero massimo di client in attesa
#define RUNNING 1             // Flag per il loop principale
#define NO_FLAG 0             // Flag generico
#define TABLE_SIZE 3          // Dimensione griglia tris (3x3)
#define GRID_SIZE 9           // Totale celle (TABLE_SIZE * TABLE_SIZE)

// Flag di comunicazione tra server e client
const int WAIT_FLAG = 0;
const int START_FLAG = 1;
const int OPPONENT_MOVE_FLAG = 2;
const int YOUR_MOVE_FLAG = 3;
const int WIN_FLAG = 4;
const int LOSE_FLAG = 5;
const int DRAW_FLAG = 6; 

// Stati del gioco
enum {
    GAME_NOT_OVER,   // Partita in corso
    PLAYER1_WIN,     // Vittoria giocatore 1 (X)
    PLAYER2_WIN,     // Vittoria giocatore 2 (O)
    GAME_DRAW        // Pareggio
};

// Struttura per rappresentare un giocatore
typedef struct player_t {
    SOCKET socket;    // Socket di connessione
    char* name;       // Nome giocatore
    int name_len; // Lunghezza nome
} player_t;

/* Crea un nuovo giocatore */
player_t* create_player(SOCKET socket, char* name, int name_len) {
    printf("[SERVER] Creazione nuovo giocatore...\n");
    player_t* player = (player_t*)malloc(sizeof(player_t));
    player->socket = socket;
    player->name = name;
    player->name_len = name_len;
    printf("[SERVER] Giocatore creato: Socket %d, Nome: %s\n", socket, name);
    return player;
}

/* Elimina un giocatore liberando la memoria */
void delete_player(player_t* player) {
    printf("[SERVER] Eliminazione giocatore %s...\n", player->name);
    free(player->name);
    free(player);
}

// Struttura per rappresentare una partita
typedef struct game_t {
    player_t* player1; // Primo giocatore
    player_t* player2; // Secondo giocatore
} game_t;

/* Crea una nuova partita */
game_t* create_game(player_t* player1, player_t* player2) {
    printf("[SERVER] Creazione nuova partita: %s vs %s\n", 
           player1->name, player2->name);
    game_t* game = (game_t*)malloc(sizeof(game_t));
    game->player1 = player1;
    game->player2 = player2;
    return game;
}

/* Elimina una partita liberando la memoria */
void delete_game(game_t* game) {
    printf("[SERVER] Terminazione partita...\n");
    delete_player(game->player1);
    delete_player(game->player2);
    free(game);
}

/* Riceve i dati del giocatore dal client */
player_t* receive_player(SOCKET socket) {
    printf("[SERVER] Ricezione dati giocatore...\n");
    int name_len;
    
    // Ricevi lunghezza nome
    if(recv(socket, (char*)&name_len, sizeof(int), 0) <= 0) {
        printf("[ERRORE] Ricezione lunghezza nome fallita\n");
        return NULL;
    }
    name_len = ntohs(name_len);
    
    // Alloca memoria per il nome
    char* name = (char*)malloc((name_len + 1) * sizeof(char));
    if(name == NULL) {
        printf("[ERRORE] Allocazione memoria nome fallita\n");
        return NULL;
    }
    
    // Ricevi il nome
    if(recv(socket, name, name_len * sizeof(char), 0) <= 0) {
        printf("[ERRORE] Ricezione nome fallita\n");
        free(name);
        return NULL;
    }
    name[name_len] = '\0';
    
    printf("[SERVER] Giocatore connesso: %s\n", name);
    return create_player(socket, name, name_len);
}

/* Converte coordinate riga/colonna in indice lineare */
uint8_t row_col(size_t i, size_t j) {
    return TABLE_SIZE * i + j;
}

/* Controlla lo stato della partita (vittoria/pareggio/continua) */
uint8_t check_win(char* table) {
    printf("[GAME] Controllo stato partita...\n");
    
    // Controlla diagonali
    if (table[row_col(1, 1)] != ' ') {
        if (table[row_col(0, 0)] == table[row_col(1, 1)] && 
            table[row_col(1, 1)] == table[row_col(2, 2)]) {
            printf("[GAME] Vittoria diagonale 1\n");
            return table[row_col(1, 1)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }
        if (table[row_col(2, 0)] == table[row_col(1, 1)] && 
            table[row_col(1, 1)] == table[row_col(0, 2)]) {
            printf("[GAME] Vittoria diagonale 2\n");
            return table[row_col(1, 1)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }
    }
    
    // Controlla righe e colonne
    for (size_t i = 0; i < TABLE_SIZE; ++i) {
        if (table[row_col(i, i)] == ' ') continue;
        
        // Controlla colonna
        if (table[row_col(0, i)] == table[row_col(1, i)] && 
            table[row_col(1, i)] == table[row_col(2, i)]) {
            printf("[GAME] Vittoria colonna %d\n", i);
            return table[row_col(i, i)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }
        
        // Controlla riga
        if (table[row_col(i, 0)] == table[row_col(i, 1)] && 
            table[row_col(i, 1)] == table[row_col(i, 2)]) {
            printf("[GAME] Vittoria riga %d\n", i);
            return table[row_col(i, i)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }
    }
    
    // Controlla se ci sono ancora mosse disponibili
    for (size_t i = 0; i < TABLE_SIZE * TABLE_SIZE; ++i) {
        if (table[i] == ' ') {
            printf("[GAME] Partita ancora in corso\n");
            return GAME_NOT_OVER;
        }
    }
    
    printf("[GAME] Pareggio\n");
    return GAME_DRAW;
}

/* Funzione principale del thread che gestisce una partita */
DWORD WINAPI game_function(LPVOID arg) {
    game_t* game = (game_t*)arg;
    player_t* player1 = game->player1;
    player_t* player2 = game->player2;

    printf("[GAME] Partita iniziata tra %s e %s\n", 
           player1->name, player2->name);

    // Comunica l'inizio della partita
    printf("[GAME] Invio flag di inizio\n");
    send(player1->socket, (const char*)&START_FLAG, sizeof(int), NO_FLAG);
    send(player2->socket, (const char*)&START_FLAG, sizeof(int), NO_FLAG);

    // Invia i nomi dei giocatori
    printf("[GAME] Invio nomi giocatori\n");
    int player1_name_len = htons(player1->name_len);
    int player2_name_len = htons(player2->name_len);
    send(player2->socket, (const char*)&player1_name_len, sizeof(int), NO_FLAG);
    send(player1->socket, (const char*)&player2_name_len, sizeof(int), NO_FLAG);
    send(player2->socket, player1->name, player1->name_len, NO_FLAG);
    send(player1->socket, player2->name, player2->name_len, NO_FLAG);

    // Assegna i simboli (X inizia sempre per primo)
    printf("[GAME] Assegnazione simboli: %s=X, %s=O\n", 
           player1->name, player2->name);
           char X = 'X', O = 'O'; 
    send(player1->socket, &X, sizeof(char), NO_FLAG);
    send(player2->socket, &O, sizeof(char), NO_FLAG);

    // Inizializza la griglia di gioco
    char table[GRID_SIZE];
    for (size_t i = 0; i < GRID_SIZE; ++i) {
        table[i] = ' ';
    }

    // Loop principale del gioco
    printf("[GAME] Inizio loop di gioco\n");
    do {
        uint8_t win_flag;
        int move;

        // Turno del giocatore 1 (X)
        printf("[GAME] Turno di %s (X)\n", player1->name);
        send(player1->socket, (const char*)&YOUR_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
        send(player2->socket, (const char*)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);

        // Ricevi mossa dal giocatore 1
        if(recv(player1->socket, (char*)&move, sizeof(int), 0) <= 0) {
            printf("[ERRORE] Ricezione mossa fallita\n");
            break;
        }
        move = ntohs(move);
        printf("[GAME] %s ha mosso in posizione %d\n", player1->name, move);
        table[move] = 'X';
        send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
        // Controlla stato del gioco
        win_flag = check_win(table);
        printf("[GAME] Stato dopo mossa: %d\n", win_flag);

        // Gestisci fine partita
        if (win_flag == GAME_DRAW) {
            printf("[GAME] Pareggio!\n");
            send(player1->socket, (const char*)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char*)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
        else if (win_flag == PLAYER1_WIN) {
            printf("[GAME] %s ha vinto!\n", player1->name);
            send(player1->socket, (const char*)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char*)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
        else if (win_flag == PLAYER2_WIN) {
            printf("[GAME] %s ha vinto!\n", player2->name);
            send(player1->socket, (const char*)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char*)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }

        // Turno del giocatore 2 (O)
        printf("[GAME] Turno di %s (O)\n", player2->name);
        send(player1->socket, (const char*)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player2->socket, (const char*)&YOUR_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        // Ricevi mossa dal giocatore 2
        if(recv(player2->socket, (char*)&move, sizeof(int), 0) <= 0) {
            printf("[ERRORE] Ricezione mossa fallita\n");
            break;
        }
        move = ntohs(move);
        printf("[GAME] %s ha mosso in posizione %d\n", player2->name, move);
        table[move] = 'O';
        send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
        // Controlla stato del gioco
        win_flag = check_win(table);
        printf("[GAME] Stato dopo mossa: %d\n", win_flag);

        // Gestisci fine partita
        if (win_flag == GAME_DRAW) {
            printf("[GAME] Pareggio!\n");
            send(player1->socket, (const char*)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char*)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
        else if (win_flag == PLAYER1_WIN) {
            printf("[GAME] %s ha vinto!\n", player1->name);
            send(player1->socket, (const char*)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char*)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
        else if (win_flag == PLAYER2_WIN) {
            printf("[GAME] %s ha vinto!\n", player2->name);
            send(player1->socket, (const char*)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char*)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
    } while (RUNNING);

    printf("[GAME] Partita terminata\n");
    delete_game(game);
    return 0;
}

/* Funzione principale del server */
int main(int argc, char* argv[]) {
    printf("[SERVER] Avvio server...\n");
    
    // Inizializzazione Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "[ERRORE] WSAStartup fallito\n");
        return EXIT_FAILURE;
    }
    printf("[SERVER] Winsock inizializzato\n");

    // Creazione socket server
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        fprintf(stderr, "[ERRORE] Creazione socket fallita: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }
    printf("[SERVER] Socket creato\n");

    // Configurazione indirizzo server
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_port = htons(8080);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    // Binding del socket
    if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        fprintf(stderr, "[ERRORE] Bind fallito: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    printf("[SERVER] Bind effettuato sulla porta 8080\n");

    // Inizio ascolto connessioni
    if (listen(server_socket, CLIENTS_LIMIT) == SOCKET_ERROR) {
        fprintf(stderr, "[ERRORE] Listen fallito: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    printf("[SERVER] In ascolto per connessioni...\n");

    struct sockaddr_in player1_struct, player2_struct;
    int len_struct = sizeof(struct sockaddr_in);
    memset(&player1_struct, 0, len_struct);
    memset(&player2_struct, 0, len_struct);

    // Loop principale di accettazione connessioni
    do {
        printf("[SERVER] In attesa del primo giocatore...\n");
        SOCKET player1_socket = accept(server_socket, (struct sockaddr*)&player1_struct, &len_struct);
        if (player1_socket == INVALID_SOCKET) {
            fprintf(stderr, "[ERRORE] Accept primo giocatore fallito: %d\n", WSAGetLastError());
            continue;
        }
        printf("[SERVER] Primo giocatore connesso\n");

        player_t* player1 = receive_player(player1_socket);
        if(player1 == NULL) {
            closesocket(player1_socket);
            continue;
        }
        
        // Comunica al primo giocatore di attendere
        send(player1->socket, (const char*)&WAIT_FLAG, sizeof(int), 0);

        printf("[SERVER] In attesa del secondo giocatore...\n");
        SOCKET player2_socket = accept(server_socket, (struct sockaddr*)&player2_struct, &len_struct);
        if (player2_socket == INVALID_SOCKET) {
            fprintf(stderr, "[ERRORE] Accept secondo giocatore fallito: %d\n", WSAGetLastError());
            delete_player(player1);
            continue;
        }
        printf("[SERVER] Secondo giocatore connesso\n");

        player_t* player2 = receive_player(player2_socket);
        if(player2 == NULL) {
            closesocket(player2_socket);
            delete_player(player1);
            continue;
        }

        // Crea un thread per gestire la partita
        printf("[SERVER] Creazione thread partita...\n");
        HANDLE game_thread = CreateThread(NULL, 0, game_function, create_game(player1, player2), 0, NULL);
        if (game_thread == NULL) {
            fprintf(stderr, "[ERRORE] Creazione thread fallita: %d\n", GetLastError());
            delete_game(create_game(player1, player2));
        }
        else {
            CloseHandle(game_thread);
        }
    } while (RUNNING);

    printf("[SERVER] Spegnimento server...\n");
    closesocket(server_socket);
    WSACleanup();
    return EXIT_SUCCESS;
}