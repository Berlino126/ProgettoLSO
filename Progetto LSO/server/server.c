#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
// Costanti di configurazione
#define CLIENTS_LIMIT 10 // Numero massimo di client in attesa
#define RUNNING 1        // Flag per il loop principale
#define NO_FLAG 0        // Flag generico
#define TABLE_SIZE 3     // Dimensione griglia tris (3x3)
#define GRID_SIZE 9      // Totale celle (TABLE_SIZE * TABLE_SIZE)

// Flag di comunicazione tra server e client
const int WAIT_FLAG = 0;
const int START_FLAG = 1;
const int OPPONENT_MOVE_FLAG = 2;
const int YOUR_MOVE_FLAG = 3;
const int WIN_FLAG = 4;
const int LOSE_FLAG = 5;
const int DRAW_FLAG = 6;
const int CREATE_GAME_FLAG = 7;
const int JOIN_GAME_FLAG = 8;
const int GAME_REQUEST_FLAG = 9;
const int GAME_ACCEPTED_FLAG = 10;
const int GAME_REJECTED_FLAG = 11;
const int LIST_GAMES_FLAG = 12;
// Stati del gioco
enum
{
    GAME_NOT_OVER, // Partita in corso
    PLAYER1_WIN,   // Vittoria giocatore 1 (X)
    PLAYER2_WIN,   // Vittoria giocatore 2 (O)
    GAME_DRAW      // Pareggio
};

// Struttura per rappresentare un giocatore
typedef struct player_t
{
    SOCKET socket; // Socket di connessione
    char *name;    // Nome giocatore
    int name_len;  // Lunghezza nome
} player_t;
// Struttura per rappresentare una partita
typedef struct game_t
{
    player_t *player1;
    player_t *player2;
    int game_id;
} game_t;
// Struttura per rappresentare una partita in attesa
typedef struct pending_game_t {
    player_t* creator;
    player_t* challenger;
    int game_id;
    int is_waiting; // 1 se in attesa di giocatori, 0 se completa
} pending_game_t;
// Lista globale di partite in attesa
pending_game_t** pending_games = NULL;
int pending_games_count = 0;
CRITICAL_SECTION games_cs; // Per la sincronizzazione tra thread
//----------------
//Prototipi: 
void handle_join_request(player_t* player, int game_id);
player_t *create_player(SOCKET socket, char *name, int name_len);
void delete_player(player_t *player);
game_t *create_game(player_t *player1, player_t *player2);
void delete_game(game_t *game);
player_t* receive_player(SOCKET socket);
uint8_t row_col(size_t i, size_t j);
uint8_t check_win(char *table);
DWORD WINAPI game_function(LPVOID arg);
void init_games_list();
void add_pending_game(pending_game_t* game);
void remove_pending_game(int game_id);
pending_game_t* find_pending_game(int game_id);
pending_game_t* create_pending_game(player_t* creator);
void handle_create_game(player_t* player);
void handle_list_games(SOCKET socket);
void handle_game_response(player_t* creator, int game_id, int response);

//-----------------
/* Crea un nuovo giocatore */
player_t *create_player(SOCKET socket, char *name, int name_len)
{
    printf("[SERVER] Creazione nuovo giocatore...\n");
    player_t *player = (player_t *)malloc(sizeof(player_t));
    player->socket = socket;
    player->name = name;
    player->name_len = name_len;
    printf("[SERVER] Giocatore creato: Socket %d, Nome: %s\n", socket, name);
    return player;
}

/* Elimina un giocatore liberando la memoria */
void delete_player(player_t *player)
{
    printf("[SERVER] Eliminazione giocatore %s...\n", player->name);
    free(player->name);
    free(player);
}


/* Crea una nuova partita */
game_t *create_game(player_t *player1, player_t *player2)
{
    printf("[SERVER] Creazione nuova partita: %s vs %s\n",
           player1->name, player2->name);
    game_t *game = (game_t *)malloc(sizeof(game_t));
    game->player1 = player1;
    game->player2 = player2;
    game->game_id = rand();
    printf("[GAME] ID Partita: %d\n", game->game_id);
    return game;
}

/* Elimina una partita liberando la memoria */
void delete_game(game_t *game)
{
    printf("[SERVER] Terminazione partita...\n");
    delete_player(game->player1);
    delete_player(game->player2);
    free(game);
}

/* Riceve i dati del giocatore dal client */
player_t* receive_player(SOCKET socket) {
    printf("[SERVER] Ricezione dati giocatore...\n");
    
    // Ricevi prima il flag del comando
    int command;
    if (recv(socket, (char*)&command, sizeof(int), 0) <= 0) {
        printf("[ERRORE] Ricezione comando fallita\n");
        return NULL;
    }
    command = ntohs(command);
    
    // Ricevi i dati del giocatore (nome)
    int name_len;
    if (recv(socket, (char*)&name_len, sizeof(int), 0) <= 0) {
        printf("[ERRORE] Ricezione lunghezza nome fallita\n");
        return NULL;
    }
    name_len = ntohs(name_len);
    
    char* name = (char*)malloc((name_len + 1) * sizeof(char));
    if (!name) {
        printf("[ERRORE] Allocazione memoria nome fallita\n");
        return NULL;
    }
    
    if (recv(socket, name, name_len * sizeof(char), 0) <= 0) {
        printf("[ERRORE] Ricezione nome fallita\n");
        free(name);
        return NULL;
    }
    name[name_len] = '\0';
    
    player_t* player = create_player(socket, name, name_len);
    
    // Gestisci i diversi tipi di comando
    if (command == WAIT_FLAG) {
        // Partita casuale - aggiungi alla coda di attesa
        printf("[SERVER] Giocatore %s in attesa di partita casuale\n", name);
        return player;
    }
    else if (command == CREATE_GAME_FLAG) {
        // Creazione partita privata
        handle_create_game(player);
        return player;
    }
    else if (command == JOIN_GAME_FLAG) {
        // Richiesta di unione a partita privata
        int game_id;
        if (recv(socket, (char*)&game_id, sizeof(int), 0) <= 0) {
            printf("[ERRORE] Ricezione ID partita fallita\n");
            delete_player(player);
            return NULL;
        }
        game_id = ntohl(game_id);
        handle_join_request(player, game_id);
        return player;
    }
    
    // Comando non riconosciuto
    delete_player(player);
    return NULL;
}

/* Converte coordinate riga/colonna in indice lineare */
uint8_t row_col(size_t i, size_t j)
{
    return TABLE_SIZE * i + j;
}

/* Controlla lo stato della partita (vittoria/pareggio/continua) */
uint8_t check_win(char *table)
{
    printf("[GAME] Controllo stato partita...\n");

    // Controlla diagonali
    if (table[row_col(1, 1)] != ' ')
    {
        if (table[row_col(0, 0)] == table[row_col(1, 1)] &&
            table[row_col(1, 1)] == table[row_col(2, 2)])
        {
            printf("[GAME] Vittoria diagonale 1\n");
            return table[row_col(1, 1)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }
        if (table[row_col(2, 0)] == table[row_col(1, 1)] &&
            table[row_col(1, 1)] == table[row_col(0, 2)])
        {
            printf("[GAME] Vittoria diagonale 2\n");
            return table[row_col(1, 1)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }
    }

    // Controlla righe e colonne
    for (size_t i = 0; i < TABLE_SIZE; ++i)
    {
        if (table[row_col(i, i)] == ' ')
            continue;

        // Controlla colonna
        if (table[row_col(0, i)] == table[row_col(1, i)] &&
            table[row_col(1, i)] == table[row_col(2, i)])
        {
            printf("[GAME] Vittoria colonna %d\n", i);
            return table[row_col(i, i)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }

        // Controlla riga
        if (table[row_col(i, 0)] == table[row_col(i, 1)] &&
            table[row_col(i, 1)] == table[row_col(i, 2)])
        {
            printf("[GAME] Vittoria riga %d\n", i);
            return table[row_col(i, i)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }
    }

    // Controlla se ci sono ancora mosse disponibili
    for (size_t i = 0; i < TABLE_SIZE * TABLE_SIZE; ++i)
    {
        if (table[i] == ' ')
        {
            printf("[GAME] Partita ancora in corso\n");
            return GAME_NOT_OVER;
        }
    }

    printf("[GAME] Pareggio\n");
    return GAME_DRAW;
}
/* Funzione per gestire la richiesta di partecipazione */
void handle_join_request(player_t* player, int game_id) {
    pending_game_t* game = find_pending_game(game_id);
    
    if (!game || !game->is_waiting) {
        // Partita non trovata o già completa
        send(player->socket, (const char*)&GAME_REJECTED_FLAG, sizeof(int), NO_FLAG);
        return;
    }
    
    // Invia la richiesta al creatore della partita
    game->challenger = player;
    
    int name_len = htons(player->name_len);
    int net_flag = htons(GAME_REQUEST_FLAG); // <--- AGGIUNGI QUESTO
    send(game->creator->socket, (const char*)&net_flag, sizeof(int), NO_FLAG);
    
    send(game->creator->socket, (const char*)&name_len, sizeof(int), NO_FLAG);
    send(game->creator->socket, player->name, player->name_len, NO_FLAG);
    
    printf("[SERVER] Richiesta di partecipazione a partita %d da %s\n", game_id, player->name);
}
/* Funzione principale del thread che gestisce una partita */
DWORD WINAPI game_function(LPVOID arg)
{
    game_t *game = (game_t *)arg;
    player_t *player1 = game->player1;
    player_t *player2 = game->player2;
    int game_id = game->game_id;

    printf("[GAME] Partita [%d] iniziata tra %s e %s\n",
           game_id, player1->name, player2->name);

    int net_game_id = htonl(game_id);

    send(player1->socket, (const char *)&START_FLAG, sizeof(int), NO_FLAG);
    send(player1->socket, (const char *)&net_game_id, sizeof(int), NO_FLAG);

    send(player2->socket, (const char *)&START_FLAG, sizeof(int), NO_FLAG);
    send(player2->socket, (const char *)&net_game_id, sizeof(int), NO_FLAG);
    // Invia i nomi dei giocatori
    printf("[GAME] Invio nomi giocatori\n");
    int player1_name_len = htons(player1->name_len);
    int player2_name_len = htons(player2->name_len);
    send(player2->socket, (const char *)&player1_name_len, sizeof(int), NO_FLAG);
    send(player1->socket, (const char *)&player2_name_len, sizeof(int), NO_FLAG);
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
    for (size_t i = 0; i < GRID_SIZE; ++i)
    {
        table[i] = ' ';
    }

    // Loop principale del gioco
    printf("[GAME] Inizio loop di gioco\n");
    do
    {
        uint8_t win_flag;
        int move;

        // Turno del giocatore 1 (X)
        printf("[GAME] Turno di %s (X)\n", player1->name);

        // Comunica al player1 che è il suo turno
        send(player1->socket, (const char *)&YOUR_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        // Comunica al player2 che deve attendere
        send(player2->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        // Ricevi mossa dal giocatore 1
        if (recv(player1->socket, (char *)&move, sizeof(int), 0) <= 0)
        {
            printf("[ERRORE] Ricezione mossa fallita\n");
            break;
        }
        move = ntohs(move);
        printf("[GAME] %s ha mosso in posizione %d\n", player1->name, move);
        table[move] = 'X';
        send(player1->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        send(player2->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
        // Controlla stato del gioco
        win_flag = check_win(table);
        printf("[GAME] Stato dopo mossa: %d\n", win_flag);

        // Gestisci fine partita
        if (win_flag == GAME_DRAW)
        {
            printf("[GAME] Pareggio!\n");
            send(player1->socket, (const char *)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
        else if (win_flag == PLAYER1_WIN)
        {
            printf("[GAME] %s ha vinto!\n", player1->name);
            send(player1->socket, (const char *)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
        else if (win_flag == PLAYER2_WIN)
        {
            printf("[GAME] %s ha vinto!\n", player2->name);
            send(player1->socket, (const char *)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }

        // Turno del giocatore 2 (O)
        printf("[GAME] Turno di %s (O)\n", player2->name);

        // Comunica al player2 che è il suo turno
        send(player2->socket, (const char *)&YOUR_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        // Comunica al player1 che deve attendere
        send(player1->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        // Ricevi mossa dal giocatore 2
        if (recv(player2->socket, (char *)&move, sizeof(int), 0) <= 0)
        {
            printf("[ERRORE] Ricezione mossa fallita\n");
            break;
        }
        move = ntohs(move);
        printf("[GAME] %s ha mosso in posizione %d\n", player2->name, move);
        table[move] = 'O';
        // Invia aggiornamento a ENTRAMBI i giocatori
        send(player1->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        send(player2->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
        // Controlla stato del gioco
        win_flag = check_win(table);
        printf("[GAME] Stato dopo mossa: %d\n", win_flag);

        // Gestisci fine partita
        if (win_flag == GAME_DRAW)
        {
            printf("[GAME] Pareggio!\n");
            send(player1->socket, (const char *)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
        else if (win_flag == PLAYER1_WIN)
        {
            printf("[GAME] %s ha vinto!\n", player1->name);
            send(player1->socket, (const char *)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
        else if (win_flag == PLAYER2_WIN)
        {
            printf("[GAME] %s ha vinto!\n", player2->name);
            send(player1->socket, (const char *)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
    } while (RUNNING);

    printf("[GAME] Partita terminata\n");
    delete_game(game);
    return 0;
}

/* Inizializza la sezione critica per le partite in attesa */
void init_games_list() {
    InitializeCriticalSection(&games_cs);
}
/* Aggiungi una partita in attesa alla lista */
void add_pending_game(pending_game_t* game) {
    EnterCriticalSection(&games_cs);
    
    pending_games_count++;
    pending_games = (pending_game_t**)realloc(pending_games, pending_games_count * sizeof(pending_game_t*));
    pending_games[pending_games_count - 1] = game;
    
    LeaveCriticalSection(&games_cs);
}
/* Rimuovi una partita in attesa dalla lista */
void remove_pending_game(int game_id) {
    EnterCriticalSection(&games_cs);
    
    for (int i = 0; i < pending_games_count; i++) {
        if (pending_games[i]->game_id == game_id) {
            free(pending_games[i]);
            for (int j = i; j < pending_games_count - 1; j++) {
                pending_games[j] = pending_games[j + 1];
            }
            pending_games_count--;
            pending_games = (pending_game_t**)realloc(pending_games, pending_games_count * sizeof(pending_game_t*));
            break;
        }
    }
    
    LeaveCriticalSection(&games_cs);
}
/* Trova una partita in attesa per ID */
pending_game_t* find_pending_game(int game_id) {
    EnterCriticalSection(&games_cs);
    
    pending_game_t* result = NULL;
    for (int i = 0; i < pending_games_count; i++) {
        if (pending_games[i]->game_id == game_id) {
            result = pending_games[i];
            break;
        }
    }
    
    LeaveCriticalSection(&games_cs);
    return result;
}
/* Crea una nuova partita in attesa */
pending_game_t* create_pending_game(player_t* creator) {
    pending_game_t* game = (pending_game_t*)malloc(sizeof(pending_game_t));
    game->creator = creator;
    game->challenger = NULL;
    game->game_id = rand();
    game->is_waiting = 1;
    return game;
}
/* Funzione per gestire la creazione di una partita */
void handle_create_game(player_t* player) {
    pending_game_t* game = create_pending_game(player);
    add_pending_game(game);
    
    // Comunica al creatore l'ID della partita
    int net_game_id = htonl(game->game_id);
    send(player->socket, (const char*)&CREATE_GAME_FLAG, sizeof(int), NO_FLAG);
    send(player->socket, (const char*)&net_game_id, sizeof(int), NO_FLAG);
    
    printf("[SERVER] Partita %d creata da %s\n", game->game_id, player->name);
}
/* Funzione per elencare le partite disponibili */
void handle_list_games(SOCKET socket) {
    EnterCriticalSection(&games_cs);
    
    // Invia il numero di partite disponibili
    int net_count = htonl(pending_games_count);
    send(socket, (const char*)&LIST_GAMES_FLAG, sizeof(int), NO_FLAG);
    send(socket, (const char*)&net_count, sizeof(int), NO_FLAG);
    
    // Invia i dettagli di ogni partita
    for (int i = 0; i < pending_games_count; i++) {
        if (pending_games[i]->is_waiting) {
            int net_id = htonl(pending_games[i]->game_id);
            int name_len = htons(pending_games[i]->creator->name_len);
            
            send(socket, (const char*)&net_id, sizeof(int), NO_FLAG);
            send(socket, (const char*)&name_len, sizeof(int), NO_FLAG);
            send(socket, pending_games[i]->creator->name, pending_games[i]->creator->name_len, NO_FLAG);
        }
    }
    
    LeaveCriticalSection(&games_cs);
}


/* Funzione per gestire la risposta del creatore */
void handle_game_response(player_t* creator, int game_id, int response) {
    pending_game_t* game = find_pending_game(game_id);
    
    if (!game || game->creator != creator || !game->challenger) {
        printf("[ERRORE] Partita %d non trovata o dati non validi\n", game_id);
        return;
    }
    
    if (response == 1) { // Accettato
        game->is_waiting = 0;
        
        // Comunica l'accettazione a entrambi i giocatori
        send(creator->socket, (const char*)&GAME_ACCEPTED_FLAG, sizeof(int), NO_FLAG);
        send(game->challenger->socket, (const char*)&GAME_ACCEPTED_FLAG, sizeof(int), NO_FLAG);
        
        // Crea il thread della partita con lo STESSO ID della partita in attesa
        HANDLE game_thread = CreateThread(NULL, 0, game_function, 
            create_game(creator, game->challenger), 0, NULL);
        if (game_thread) {
            CloseHandle(game_thread);
        } else {
            fprintf(stderr, "[ERRORE] Creazione thread fallita: %d\n", GetLastError());
        }
        
        // Rimuovi la partita dalla lista delle in attesa DOPO aver creato il thread
        remove_pending_game(game_id);
        
        printf("[SERVER] Partita %d iniziata tra %s e %s\n", 
            game_id, creator->name, game->challenger->name);
    } else { // Rifiutato
        send(game->challenger->socket, (const char*)&GAME_REJECTED_FLAG, sizeof(int), NO_FLAG);
        game->challenger = NULL; // Resetta il challenger ma mantieni la partita
        
        printf("[SERVER] Partita %d rifiutata da %s\n", game_id, creator->name);
    }
}

/* Funzione principale del server */
int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));
    printf("[SERVER] Avvio server...\n");
    init_games_list();
    // Inizializzazione Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "[ERRORE] WSAStartup fallito\n");
        return EXIT_FAILURE;
    }
    printf("[SERVER] Winsock inizializzato\n");

    // Creazione socket server
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET)
    {
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
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
    {
        fprintf(stderr, "[ERRORE] Bind fallito: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    printf("[SERVER] Bind effettuato sulla porta 8080\n");

    // Inizio ascolto connessioni
    if (listen(server_socket, CLIENTS_LIMIT) == SOCKET_ERROR)
    {
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
        printf("[SERVER] In attesa di connessioni...\n");
        SOCKET client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET) {
            fprintf(stderr, "[ERRORE] Accept fallito: %d\n", WSAGetLastError());
            continue;
        }
    
        // Peek per capire il tipo di richiesta
        int command_net;
        if (recv(client_socket, (char*)&command_net, sizeof(int), MSG_PEEK) <= 0) {
            closesocket(client_socket);
            continue;
        }
    
        int command = ntohs(command_net);
    
        if (command == WAIT_FLAG || command == CREATE_GAME_FLAG || command == JOIN_GAME_FLAG) {
            // Ricevi normalmente il giocatore
            player_t* player = receive_player(client_socket);
            if (!player) {
                closesocket(client_socket);
                continue;
            }
    
            if (command == WAIT_FLAG) {
                // LOGICA PARTITA CASUALE — come prima
                printf("[SERVER] In attesa di secondo giocatore...\n");
    
                SOCKET player2_socket = accept(server_socket, NULL, NULL);
                if (player2_socket == INVALID_SOCKET) {
                    fprintf(stderr, "[ERRORE] Accept secondo giocatore fallito\n");
                    delete_player(player);
                    continue;
                }
    
                player_t* player2 = receive_player(player2_socket);
                if (!player2) {
                    closesocket(player2_socket);
                    delete_player(player);
                    continue;
                }
    
                HANDLE game_thread = CreateThread(NULL, 0, game_function, create_game(player, player2), 0, NULL);
                if (game_thread) CloseHandle(game_thread);
            }
    
            // Per CREATE_GAME_FLAG e JOIN_GAME_FLAG non devi fare altro qui:
            // il resto è gestito dentro receive_player e handle_join_request
        }
        else {
            // Caso risposta a una richiesta di unione (dal creatore)
            int game_id_net, response_flag_net;
            if (recv(client_socket, (char*)&game_id_net, sizeof(int), 0) <= 0 ||
                recv(client_socket, (char*)&response_flag_net, sizeof(int), 0) <= 0) {
                fprintf(stderr, "[ERRORE] Ricezione risposta a richiesta fallita\n");
                closesocket(client_socket);
                continue;
            }
    
            int game_id = ntohl(game_id_net);
            int response_flag = ntohs(response_flag_net);
    
            pending_game_t* pending = find_pending_game(game_id);
            if (pending && pending->creator && pending->creator->socket == client_socket) {
                handle_game_response(pending->creator, game_id, response_flag);
            } else {
                fprintf(stderr, "[ERRORE] Creatore non valido per partita %d\n", game_id);
            }
        }
    
    } while (RUNNING);

    printf("[SERVER] Spegnimento server...\n");
    closesocket(server_socket);
    WSACleanup();
    return EXIT_SUCCESS;
}
