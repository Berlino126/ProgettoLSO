#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

// Costanti di configurazione
#define CLIENTS_LIMIT 10       // Limite massimo di client in attesa
#define RUNNING 1              // Flag per il loop di gioco
#define NO_FLAG 0              // Nessun flag speciale
#define TABLE_SIZE 3           // Dimensione della griglia di gioco (3x3)
#define GRID_SIZE 9            // Totale celle (TABLE_SIZE * TABLE_SIZE)
#define MAX_ROOMS 20           // Numero massimo di stanze private

// Flag di comunicazione tra server e client
const int WAIT_FLAG = 0;       // Attendi un avversario
const int START_FLAG = 1;       // Inizio partita
const int OPPONENT_MOVE_FLAG = 2; // Avversario ha mosso
const int YOUR_MOVE_FLAG = 3;   // È il tuo turno
const int WIN_FLAG = 4;         // Hai vinto
const int LOSE_FLAG = 5;        // Hai perso
const int DRAW_FLAG = 6;        // Pareggio
const int CREATE_PRIVATE = 10;  // Richiesta creazione stanza privata
const int PRIVATE_CREATED = 11; // Stanza privata creata
const int JOIN_PRIVATE = 12;    // Richiesta di unirsi a stanza privata
const int JOIN_REQUEST = 13;    // Richiesta di join ricevuta
const int JOIN_ACCEPTED = 14;   // Join accettato
const int JOIN_REJECTED = 15;   // Join rifiutato

// Stati del gioco
enum {
    GAME_NOT_OVER, // Partita in corso
    PLAYER1_WIN,   // Vittoria giocatore 1 (X)
    PLAYER2_WIN,   // Vittoria giocatore 2 (O)
    GAME_DRAW      // Pareggio
};

// Struttura per rappresentare un giocatore
typedef struct player_t {
    int socket;     // Socket del giocatore
    char *name;     // Nome del giocatore
    int name_len;   // Lunghezza del nome
} player_t;

// Struttura per una stanza privata
typedef struct private_room_t {
    int id;             // ID unico della stanza
    player_t *creator;  // Giocatore che ha creato la stanza
} private_room_t;

private_room_t *private_rooms[MAX_ROOMS]; // Array di stanze private

// Genera un ID unico per una stanza privata
int generate_unique_room_id() {
    printf("[ROOM] Generazione ID stanza unico\n");
    return 1000 + rand() % 9000; // ID tra 1000 e 9999
}

// Aggiunge una stanza privata all'array
void add_private_room(private_room_t *room) {
    printf("[ROOM] Aggiunta stanza privata ID: %d\n", room->id);
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (private_rooms[i] == NULL) {
            private_rooms[i] = room;
            return;
        }
    }
    printf("[ROOM] ERRORE: Numero massimo di stanze raggiunto\n");
}

// Trova una stanza per ID
private_room_t *find_room_by_id(int id) {
    printf("[ROOM] Ricerca stanza ID: %d\n", id);
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (private_rooms[i] && private_rooms[i]->id == id) {
            return private_rooms[i];
        }
    }
    printf("[ROOM] Stanza non trovata\n");
    return NULL;
}

// Rimuove una stanza per ID
void remove_room_by_id(int id) {
    printf("[ROOM] Rimozione stanza ID: %d\n", id);
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (private_rooms[i] && private_rooms[i]->id == id) {
            free(private_rooms[i]);
            private_rooms[i] = NULL;
        }
    }
}

// Crea un nuovo giocatore
player_t *create_player(int socket, char *name, int name_len) {
    printf("[PLAYER] Creazione giocatore: %s\n", name);
    player_t *player = malloc(sizeof(player_t));
    player->socket = socket;
    player->name = name;
    player->name_len = name_len;
    return player;
}

// Elimina un giocatore
void delete_player(player_t *player) {
    printf("[PLAYER] Eliminazione giocatore: %s\n", player->name);
    free(player->name);
    free(player);
}

// Struttura per rappresentare una partita
typedef struct game_t {
    player_t *player1; // Giocatore 1 (X)
    player_t *player2; // Giocatore 2 (O)
    int game_id;       // ID unico della partita
} game_t;

// Crea una nuova partita
game_t *create_game(player_t *player1, player_t *player2) {
    printf("[GAME] Creazione partita tra %s e %s\n", player1->name, player2->name);
    game_t *game = malloc(sizeof(game_t));
    game->player1 = player1;
    game->player2 = player2;
    game->game_id = rand();
    return game;
}

// Elimina una partita
void delete_game(game_t *game) {
    printf("[GAME] Eliminazione partita ID: %d\n", game->game_id);
    delete_player(game->player1);
    delete_player(game->player2);
    free(game);
}

// Riceve i dati di un giocatore dal socket
player_t *receive_player(int socket) {
    printf("[PLAYER] Ricezione dati giocatore dal socket %d\n", socket);
    
    int name_len;
    if (recv(socket, &name_len, sizeof(int), 0) <= 0) {
        printf("[ERRORE] Ricezione lunghezza nome fallita\n");
        return NULL;
    }
    name_len = ntohs(name_len);
    printf("[PLAYER] Lunghezza nome ricevuta: %d\n", name_len);

    char *name = malloc((name_len + 1) * sizeof(char));
    if (recv(socket, name, name_len, 0) <= 0) {
        printf("[ERRORE] Ricezione nome fallita\n");
        free(name);
        return NULL;
    }
    name[name_len] = '\0';
    printf("[PLAYER] Nome ricevuto: %s\n", name);

    return create_player(socket, name, name_len);
}

// Converte coordinate riga/colonna in indice lineare
uint8_t row_col(size_t i, size_t j) {
    return TABLE_SIZE * i + j;
}

// Controlla lo stato della partita (vittoria/pareggio)
uint8_t check_win(char *table) {
    printf("[GAME] Controllo stato partita\n");
    
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
        // Controlla colonne
        if (table[row_col(0, i)] == table[row_col(1, i)] &&
            table[row_col(1, i)] == table[row_col(2, i)] &&
            table[row_col(0, i)] != ' ') {
            printf("[GAME] Vittoria colonna %zu\n", i);
            return table[row_col(0, i)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
        }

        // Controlla righe
        if (table[row_col(i, 0)] == table[row_col(i, 1)] &&
            table[row_col(i, 1)] == table[row_col(i, 2)] &&
            table[row_col(i, 0)] != ' ') {
            printf("[GAME] Vittoria riga %zu\n", i);
            return table[row_col(i, 0)] == 'X' ? PLAYER1_WIN : PLAYER2_WIN;
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

// Funzione principale del gioco (eseguita in un thread separato)
void *game_function(void *arg) {
    game_t *game = (game_t *)arg;
    player_t *player1 = game->player1;
    player_t *player2 = game->player2;
    int game_id = game->game_id;

    printf("[GAME] Partita [%d] iniziata tra %s e %s\n",
           game_id, player1->name, player2->name);

    // Comunica l'inizio della partita ai giocatori
    int net_game_id = htonl(game_id);
    send(player1->socket, &START_FLAG, sizeof(int), NO_FLAG);
    send(player1->socket, &net_game_id, sizeof(int), NO_FLAG);
    send(player2->socket, &START_FLAG, sizeof(int), NO_FLAG);
    send(player2->socket, &net_game_id, sizeof(int), NO_FLAG);

    // Invia i nomi dei giocatori
    printf("[GAME] Invio nomi giocatori\n");
    int p1_len = htons(player1->name_len);
    int p2_len = htons(player2->name_len);
    send(player2->socket, &p1_len, sizeof(int), NO_FLAG);
    send(player1->socket, &p2_len, sizeof(int), NO_FLAG);
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
    memset(table, ' ', GRID_SIZE);
    printf("[GAME] Griglia inizializzata\n");

    // Loop principale del gioco
    printf("[GAME] Inizio loop di gioco\n");
    do {
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
        if (recv(player1->socket, (char *)&move, sizeof(int), 0) <= 0) {
            printf("[ERRORE] Ricezione mossa da %s fallita\n", player1->name);
            break;
        }
        move = ntohs(move);
        printf("[GAME] %s ha mosso in posizione %d\n", player1->name, move);
        table[move] = 'X';

        // Invia aggiornamento a entrambi i giocatori
        send(player1->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
        send(player2->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        // Controlla stato del gioco
        win_flag = check_win(table);
        printf("[GAME] Stato dopo mossa: %d\n", win_flag);

        // Gestisci fine partita
        if (win_flag == GAME_DRAW) {
            printf("[GAME] Pareggio!\n");
            send(player1->socket, (const char *)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        } else if (win_flag == PLAYER1_WIN) {
            printf("[GAME] %s ha vinto!\n", player1->name);
            send(player1->socket, (const char *)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        } else if (win_flag == PLAYER2_WIN) {
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
        if (recv(player2->socket, (char *)&move, sizeof(int), 0) <= 0) {
            printf("[ERRORE] Ricezione mossa da %s fallita\n", player2->name);
            break;
        }
        move = ntohs(move);
        printf("[GAME] %s ha mosso in posizione %d\n", player2->name, move);
        table[move] = 'O';

        // Invia aggiornamento a entrambi i giocatori
        send(player1->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
        send(player2->socket, (const char *)&OPPONENT_MOVE_FLAG, sizeof(int), NO_FLAG);
        send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);

        // Controlla stato del gioco
        win_flag = check_win(table);
        printf("[GAME] Stato dopo mossa: %d\n", win_flag);

        // Gestisci fine partita
        if (win_flag == GAME_DRAW) {
            printf("[GAME] Pareggio!\n");
            send(player1->socket, (const char *)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&DRAW_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        } else if (win_flag == PLAYER1_WIN) {
            printf("[GAME] %s ha vinto!\n", player1->name);
            send(player1->socket, (const char *)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        } else if (win_flag == PLAYER2_WIN) {
            printf("[GAME] %s ha vinto!\n", player2->name);
            send(player1->socket, (const char *)&LOSE_FLAG, sizeof(int), NO_FLAG);
            send(player1->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            send(player2->socket, (const char *)&WIN_FLAG, sizeof(int), NO_FLAG);
            send(player2->socket, table, GRID_SIZE * sizeof(char), NO_FLAG);
            break;
        }
    } while (RUNNING);

    printf("[GAME] Partita [%d] terminata\n", game_id);
    delete_game(game);
    return NULL;
}

int main() {
    setbuf(stdout, NULL);
    printf("[SERVER] Avvio server...\n");
    srand(time(NULL));
    
    // Creazione socket server
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] Socket creato\n");

    // Configurazione indirizzo server
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = INADDR_ANY;

    // Binding del socket
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] Bind effettuato sulla porta 8080\n");

    // Inizio ascolto connessioni
    if (listen(server_socket, CLIENTS_LIMIT) < 0) {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] In ascolto per connessioni...\n");

    struct sockaddr_in player1_struct, player2_struct;
    socklen_t len_struct = sizeof(struct sockaddr_in);

    // Loop principale del server
    while (1) {
        printf("[SERVER] In attesa di connessioni...\n");
        
        // Accetta connessione da un client
        int client_socket = accept(server_socket, (struct sockaddr *)&player1_struct, &len_struct);
        if (client_socket < 0) {
            printf("[SERVER] Errore nell'accettare la connessione\n");
            continue;
        }
        printf("[SERVER] Nuova connessione accettata\n");

        // Ricevi il flag iniziale dal client
        int initial_flag;
        if (recv(client_socket, &initial_flag, sizeof(int), 0) <= 0) {
            printf("[SERVER] Errore nella ricezione del flag iniziale\n");
            close(client_socket);
            continue;
        }
        printf("[SERVER] Flag iniziale ricevuto: %d\n", initial_flag);

        // Gestione stanze private
        if (initial_flag == CREATE_PRIVATE) {
            printf("[SERVER] Richiesta creazione stanza privata\n");
            player_t *creator = receive_player(client_socket);
            if (!creator) {
                printf("[SERVER] Errore nella ricezione del creatore\n");
                close(client_socket);
                continue;
            }

            // Crea una nuova stanza privata
            int room_id = generate_unique_room_id();
            private_room_t *room = malloc(sizeof(private_room_t));
            room->id = room_id;
            room->creator = creator;
            add_private_room(room);

            // Comunica l'ID della stanza al creatore
            int net_room_id = htonl(room_id);
            send(creator->socket, &PRIVATE_CREATED, sizeof(int), 0);
            send(creator->socket, &net_room_id, sizeof(int), 0);
            printf("[SERVER] Stanza privata %d creata da %s\n", room_id, creator->name);
            continue;
        } 
        else if (initial_flag == JOIN_PRIVATE) {
            printf("[SERVER] Richiesta di unione a stanza privata\n");
            int net_room_id;
            recv(client_socket, &net_room_id, sizeof(int), 0);
            int room_id = ntohl(net_room_id);
            printf("[SERVER] Tentativo di unione a stanza %d\n", room_id);

            player_t *joiner = receive_player(client_socket);
            if (!joiner) {
                printf("[SERVER] Errore nella ricezione del giocatore\n");
                close(client_socket);
                continue;
            }

            private_room_t *room = find_room_by_id(room_id);
            if (!room) {
                printf("[SERVER] Stanza %d non trovata\n", room_id);
                send(joiner->socket, &JOIN_REJECTED, sizeof(int), 0);
                delete_player(joiner);
                continue;
            }

            // Invia richiesta di join al creatore della stanza
            printf("[SERVER] Invio richiesta di join a %s\n", room->creator->name);
            int req = JOIN_REQUEST;
            int name_len = htons(joiner->name_len);
            send(room->creator->socket, &req, sizeof(int), 0);
            send(room->creator->socket, &name_len, sizeof(int), 0);
            send(room->creator->socket, joiner->name, joiner->name_len, 0);

            // Attendi risposta dal creatore
            int response;
            recv(room->creator->socket, &response, sizeof(int), 0);

            if (response == JOIN_ACCEPTED) {
                printf("[SERVER] Join accettato per %s\n", joiner->name);
                send(joiner->socket, &JOIN_ACCEPTED, sizeof(int), 0);
                
                // Crea un thread per la partita
                pthread_t thread_id;
                pthread_create(&thread_id, NULL, game_function, create_game(room->creator, joiner));
                pthread_detach(thread_id);
                
                // Rimuovi la stanza (ora la partita è iniziata)
                remove_room_by_id(room_id);
            } else {
                printf("[SERVER] Join rifiutato per %s\n", joiner->name);
                send(joiner->socket, &JOIN_REJECTED, sizeof(int), 0);
                delete_player(joiner);
            }
            continue;
        } 
        else {
            // Modalità gioco normale (non privata)
            printf("[SERVER] Modalità gioco normale\n");
            player_t *player1 = receive_player(client_socket);
            if (!player1) {
                printf("[SERVER] Errore nella ricezione del giocatore 1\n");
                close(client_socket);
                continue;
            }

            printf("[SERVER] Giocatore 1 connesso: %s\n", player1->name);
            send(player1->socket, &WAIT_FLAG, sizeof(int), 0);
            printf("[SERVER] In attesa del secondo giocatore...\n");

            // Accetta connessione dal secondo giocatore
            int player2_socket = accept(server_socket, (struct sockaddr *)&player2_struct, &len_struct);
            if (player2_socket < 0) {
                printf("[SERVER] Errore nell'accettare il secondo giocatore\n");
                delete_player(player1);
                continue;
            }

            // Verifica che il secondo client sia un giocatore (non una richiesta speciale)
            int second_flag;
            if (recv(player2_socket, &second_flag, sizeof(int), 0) <= 0 || second_flag != NO_FLAG) {
                printf("[SERVER] Secondo client non valido\n");
                delete_player(player1);
                close(player2_socket);
                continue;
            }

            player_t *player2 = receive_player(player2_socket);
            if (!player2) {
                printf("[SERVER] Errore nella ricezione del giocatore 2\n");
                delete_player(player1);
                close(player2_socket);
                continue;
            }

            printf("[SERVER] Giocatore 2 connesso: %s\n", player2->name);
            
            // Crea un thread per la partita
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, game_function, create_game(player1, player2));
            pthread_detach(thread_id);
            printf("[SERVER] Partita avviata tra %s e %s\n", player1->name, player2->name);
        }
    }

    // Chiusura server (mai raggiunto in questo loop infinito)
    close(server_socket);
    printf("[SERVER] Server terminato\n");
    return 0;
}
