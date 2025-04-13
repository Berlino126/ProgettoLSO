#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>

// Costanti di configurazione
#define TABLE_SIZE 3          // Dimensione griglia tris (3x3)
#define GRID_SIZE 9           // Totale celle (TABLE_SIZE * TABLE_SIZE)
#define BUFFER_SIZE 1024      // Dimensione buffer per i messaggi

// Flag di comunicazione tra server e client
const int WAIT_FLAG = 0;
const int START_FLAG = 1;
const int OPPONENT_MOVE_FLAG = 2;
const int YOUR_MOVE_FLAG = 3;
const int WIN_FLAG = 4;
const int LOSE_FLAG = 5;
const int DRAW_FLAG = 6;

// Variabili globali
SOCKET client_socket;
char player_name[50];
char opponent_name[50];
char my_symbol = ' ';
char opponent_symbol = ' ';

/* Funzione per pulire lo schermo */
void clear_screen() {
    system("cls");
}

/* Funzione per visualizzare la griglia di gioco */
void display_grid(char* grid) {
    printf("\n");
    printf(" %c | %c | %c \n", grid[0], grid[1], grid[2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", grid[3], grid[4], grid[5]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", grid[6], grid[7], grid[8]);
    printf("\n");
}

/* Funzione per ottenere la mossa dal giocatore */
int get_player_move(char* grid) {
    int move;
    char input[10];
    
    while(1) {
        printf("Inserisci la tua mossa (1-9): ");
        fgets(input, sizeof(input), stdin);
        
        // Converti input in numero
        move = atoi(input) - 1; // Converti in indice 0-based
        
        // Controlla se la mossa è valida
        if(move >= 0 && move < 9 && grid[move] == ' ') {
            return move;
        }
        
        printf("Mossa non valida. Riprova.\n");
    }
}

/* Funzione per gestire la connessione al server */
int connect_to_server(const char* server_ip, int port) {
    // Inizializzazione Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup fallito\n");
        return -1;
    }

    // Creazione socket client
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        fprintf(stderr, "Creazione socket fallita: %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    // Configurazione indirizzo server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Connessione al server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        fprintf(stderr, "Connessione al server fallita: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return -1;
    }

    return 0;
}

/* Funzione per inviare il nome del giocatore al server */
void send_player_name() {
    // Ottieni il nome del giocatore
    printf("Inserisci il tuo nome: ");
    fgets(player_name, sizeof(player_name), stdin);
    player_name[strcspn(player_name, "\n")] = '\0'; // Rimuovi newline
    
    // Invia la lunghezza del nome
    int name_len = strlen(player_name);
    int net_name_len = htons(name_len);
    send(client_socket, (const char*)&net_name_len, sizeof(int), 0);
    
    // Invia il nome
    send(client_socket, player_name, name_len, 0);
}

/* Funzione per ricevere i dati iniziali della partita */
void receive_game_start() {
    int flag;
    int name_len;
    
    // Ricevi il flag di inizio
    recv(client_socket, (char*)&flag, sizeof(int), 0);
    
    if(flag == START_FLAG) {
        // Ricevi la lunghezza del nome dell'avversario
        recv(client_socket, (char*)&name_len, sizeof(int), 0);
        name_len = ntohs(name_len);
        
        // Ricevi il nome dell'avversario
        recv(client_socket, opponent_name, name_len, 0);
        opponent_name[name_len] = '\0';
        
        // Ricevi il simbolo assegnato
        recv(client_socket, &my_symbol, sizeof(char), 0);
        opponent_symbol = (my_symbol == 'X') ? 'O' : 'X';
        
        clear_screen();
        printf("Partita iniziata!\n");
        printf("Tu sei: %c\n", my_symbol);
        printf("Avversario: %s (%c)\n", opponent_name, opponent_symbol);
    }
}

/* Funzione principale del gioco */
void play_game() {
    char grid[GRID_SIZE];
    int flag;
    int move;
    
    // Inizializza la griglia
    memset(grid, ' ', GRID_SIZE);
    
    while(1) {
        // Ricevi il flag dal server
        recv(client_socket, (char*)&flag, sizeof(int), 0);
        
        if(flag == YOUR_MOVE_FLAG) {
            // È il tuo turno
            clear_screen();
            printf("Tocca a te muovere (%c)\n", my_symbol);
            printf("Avversario: %s (%c)\n", opponent_name, opponent_symbol);
            display_grid(grid);
            
            // Ottieni la mossa dal giocatore
            move = get_player_move(grid);
            
            // Invia la mossa al server
            move = htons(move);
            send(client_socket, (const char*)&move, sizeof(int), 0);
            
            // Aggiorna la griglia locale
            grid[ntohs(move)] = my_symbol;
        }
        else if(flag == OPPONENT_MOVE_FLAG) {
            // L'avversario ha mosso
            clear_screen();
            printf("Attendi la mossa dell'avversario...\n");
            printf("Avversario: %s (%c)\n", opponent_name, opponent_symbol);
            display_grid(grid);
            
            // Ricevi la griglia aggiornata
            recv(client_socket, grid, GRID_SIZE * sizeof(char), 0);
            
            display_grid(grid);
        }
        else if(flag == WIN_FLAG) {
            // Hai vinto!
            recv(client_socket, grid, GRID_SIZE * sizeof(char), 0);
            clear_screen();
            printf("Hai vinto la partita contro %s!\n", opponent_name);
            display_grid(grid);
            break;
        }
        else if(flag == LOSE_FLAG) {
            // Hai perso
            recv(client_socket, grid, GRID_SIZE * sizeof(char), 0);
            clear_screen();
            printf("Hai perso contro %s!\n", opponent_name);
            display_grid(grid);
            break;
        }
        else if(flag == DRAW_FLAG) {
            // Pareggio
            recv(client_socket, grid, GRID_SIZE * sizeof(char), 0);
            clear_screen();
            printf("Partita terminata in pareggio contro %s!\n", opponent_name);
            display_grid(grid);
            break;
        }
    }
}

/* Funzione principale */
int main(int argc, char* argv[]) {
    char server_ip[16] = "127.0.0.1"; // Default: localhost
    int port = 8080;
    
    // Leggi l'IP del server da riga di comando se fornito
    if(argc > 1) {
        strncpy(server_ip, argv[1], 15);
        server_ip[15] = '\0';
    }
    
    // Leggi la porta da riga di comando se fornita
    if(argc > 2) {
        port = atoi(argv[2]);
    }
    
    printf("Client Tris Multiplayer\n");
    printf("Connessione al server %s:%d...\n", server_ip, port);
    
    // Connetti al server
    if(connect_to_server(server_ip, port) != 0) {
        printf("Impossibile connettersi al server.\n");
        return 1;
    }
    
    printf("Connessione stabilita.\n");
    
    // Invia il nome del giocatore
    send_player_name();
    
    // Attendi l'inizio della partita
    printf("In attesa di un avversario...\n");
    
    // Ricevi il flag iniziale (WAIT o START)
    int initial_flag;
    recv(client_socket, (char*)&initial_flag, sizeof(int), 0);
    
    if(initial_flag == WAIT_FLAG) {
        printf("In attesa che il server trovi un avversario...\n");
        // Ora riceveremo START_FLAG quando la partita inizia
        receive_game_start();
    }
    else if(initial_flag == START_FLAG) {
        // Partita inizia immediatamente
        receive_game_start();
    }
    
    // Gioca la partita
    play_game();
    
    // Chiusura
    printf("Premi un tasto per uscire...\n");
    getch();
    
    closesocket(client_socket);
    WSACleanup();
    return 0;
}