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

// Flag di comunicazione tra server e client
#define WAIT_FLAG 0
#define START_FLAG 1
#define OPPONENT_MOVE_FLAG 2
#define YOUR_MOVE_FLAG 3
#define WIN_FLAG 4
#define LOSE_FLAG 5
#define DRAW_FLAG 6

/* Stampa la griglia di gioco */
void print_grid(char* grid) {
    printf("\n");
    for (int i = 0; i < TABLE_SIZE; i++) {
        printf(" ");
        for (int j = 0; j < TABLE_SIZE; j++) {
            printf(" %c ", grid[i * TABLE_SIZE + j]);
            if (j < TABLE_SIZE - 1) printf("|");
        }
        printf("\n");
        
        if (i < TABLE_SIZE - 1) {
            printf("---+---+---\n");
        }
    }
    printf("\n");
}

/* Pulisce lo schermo */
void clear_screen() {
    system("cls");
}

/* Mostra il menu principale */
void show_menu() {
    clear_screen();
    printf("=== TRIS ONLINE ===\n\n");
}

/* Converte coordinate 1-9 in indice della griglia (0-8) */
int position_to_index(int pos) {
    return pos - 1;
}

/* Valida la mossa dell'utente */
int get_valid_move(char* grid) {
    int position;
    char input[10];
    
    while (1) {
        printf("Inserisci la tua mossa (1-9): ");
        fgets(input, sizeof(input), stdin);
        
        if (sscanf(input, "%d", &position) != 1) {
            printf("Input non valido. Inserisci un numero tra 1 e 9.\n");
            continue;
        }
        
        if (position < 1 || position > 9) {
            printf("Posizione non valida. Inserisci un numero tra 1 e 9.\n");
            continue;
        }
        
        int index = position_to_index(position);
        if (grid[index] != ' ') {
            printf("Cella già occupata. Scegli un'altra posizione.\n");
            continue;
        }
        
        break;
    }
    
    return position_to_index(position);
}

/* Gestisce la partita */
void handle_game(SOCKET client_socket, char* player_name, char* opponent_name, 
                char player_symbol, char opponent_symbol, char* grid) {
    while (1) {
        int flag;
        
        if (recv(client_socket, (char*)&flag, sizeof(int), 0) <= 0) {
            fprintf(stderr, "Connessione con il server persa.\n");
            break;
        }
        
        switch (flag) {
            case WAIT_FLAG:
                clear_screen();
                printf("In attesa che un altro giocatore si connetta...\n");
                break;
                
            case START_FLAG: {
                clear_screen();
                printf("Partita trovata! Inizio del gioco.\n");
                
                int opponent_name_len;
                if (recv(client_socket, (char*)&opponent_name_len, sizeof(int), 0) <= 0) {
                    fprintf(stderr, "Errore ricezione lunghezza nome avversario.\n");
                    return;
                }
                opponent_name_len = ntohs(opponent_name_len);
                
                if (recv(client_socket, opponent_name, opponent_name_len, 0) <= 0) {
                    fprintf(stderr, "Errore ricezione nome avversario.\n");
                    return;
                }
                opponent_name[opponent_name_len] = '\0';
                
                if (recv(client_socket, &player_symbol, sizeof(char), 0) <= 0) {
                    fprintf(stderr, "Errore ricezione simbolo giocatore.\n");
                    return;
                }
                
                opponent_symbol = (player_symbol == 'X') ? 'O' : 'X';
                
                printf("Stai giocando contro: %s\n", opponent_name);
                printf("Il tuo simbolo: %c\n", player_symbol);
                
                memset(grid, ' ', GRID_SIZE);
                break;
            }
                
            case YOUR_MOVE_FLAG: {
                if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0) {
                    fprintf(stderr, "Errore ricezione griglia.\n");
                    return;
                }
                
                clear_screen();
                printf("=== TUO TURNO ===\n");
                printf("Tu: %c (%s) vs Avversario: %c (%s)\n", 
                      player_symbol, player_name, opponent_symbol, opponent_name);
                
                print_grid(grid);
                
                int move = get_valid_move(grid);
                int net_move = htons(move);
                
                if (send(client_socket, (const char*)&net_move, sizeof(int), 0) < 0) {
                    fprintf(stderr, "Invio mossa fallito. Errore: %d\n", WSAGetLastError());
                    return;
                }
                break;
            }
                
            case OPPONENT_MOVE_FLAG: {
                if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0) {
                    fprintf(stderr, "Errore ricezione griglia.\n");
                    return;
                }
                
                clear_screen();
                printf("=== TURNO AVVERSARIO ===\n");
                printf("Tu: %c (%s) vs Avversario: %c (%s)\n", 
                      player_symbol, player_name, opponent_symbol, opponent_name);
                
                print_grid(grid);
                printf("In attesa della mossa dell'avversario...\n");
                break;
            }
                
            case WIN_FLAG: {
                if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0) {
                    fprintf(stderr, "Errore ricezione griglia.\n");
                    return;
                }
                
                clear_screen();
                printf("=== VITTORIA! ===\n");
                printf("Hai sconfitto %s!\n", opponent_name);
                print_grid(grid);
                
                printf("Premi un tasto per uscire...\n");
                _getch();
                return;
            }
                
            case LOSE_FLAG: {
                if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0) {
                    fprintf(stderr, "Errore ricezione griglia.\n");
                    return;
                }
                
                clear_screen();
                printf("=== SCONFITTA ===\n");
                printf("%s ti ha sconfitto!\n", opponent_name);
                print_grid(grid);
                
                printf("Premi un tasto per uscire...\n");
                _getch();
                return;
            }
                
            case DRAW_FLAG: {
                if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0) {
                    fprintf(stderr, "Errore ricezione griglia.\n");
                    return;
                }
                
                clear_screen();
                printf("=== PAREGGIO ===\n");
                printf("La partita contro %s è terminata in pareggio!\n", opponent_name);
                print_grid(grid);
                
                printf("Premi un tasto per uscire...\n");
                _getch();
                return;
            }
                
            default:
                fprintf(stderr, "Flag sconosciuto ricevuto dal server: %d\n", flag);
                return;
        }
    }
}

/* Funzione principale del client */
int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET client_socket;
    struct sockaddr_in server;
    char server_ip[16] = "127.0.0.1";
    int server_port = 8080;
    char player_name[50];
    char opponent_name[50];
    char player_symbol, opponent_symbol;
    char grid[GRID_SIZE];
    
    show_menu();
    
    if (argc > 1) {
        strncpy(server_ip, argv[1], sizeof(server_ip)-1);
        if (argc > 2) {
            server_port = atoi(argv[2]);
        }
    }
    
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup fallito. Errore: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }
    
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        fprintf(stderr, "Creazione socket fallita. Errore: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }
    
    server.sin_addr.s_addr = inet_addr(server_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    
    printf("Inserisci il tuo nome (max 49 caratteri): ");
    fgets(player_name, sizeof(player_name), stdin);
    player_name[strcspn(player_name, "\n")] = '\0';
    
    if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
        fprintf(stderr, "Connessione al server fallita. Errore: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    
    int name_len = strlen(player_name);
    int net_name_len = htons(name_len);
    
    if (send(client_socket, (const char*)&net_name_len, sizeof(int), 0) < 0) {
        fprintf(stderr, "Invio lunghezza nome fallito. Errore: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    
    if (send(client_socket, player_name, name_len, 0) < 0) {
        fprintf(stderr, "Invio nome fallito. Errore: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    
    printf("Connessione al server stabilita. In attesa di un avversario...\n");
    
    handle_game(client_socket, player_name, opponent_name, player_symbol, opponent_symbol, grid);
    
    closesocket(client_socket);
    WSACleanup();
    return EXIT_SUCCESS;
}
