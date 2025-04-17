#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>

// Costanti di configurazione
#define TABLE_SIZE 3 // Dimensione griglia tris (3x3)
#define GRID_SIZE 9  // Totale celle (TABLE_SIZE * TABLE_SIZE)

// Flag di comunicazione tra server e client
#define WAIT_FLAG 0
#define START_FLAG 1
#define OPPONENT_MOVE_FLAG 2
#define YOUR_MOVE_FLAG 3
#define WIN_FLAG 4
#define LOSE_FLAG 5
#define DRAW_FLAG 6

const int CREATE_GAME_FLAG = 7;
const int JOIN_GAME_FLAG = 8;
const int GAME_REQUEST_FLAG = 9;
const int GAME_ACCEPTED_FLAG = 10;
const int GAME_REJECTED_FLAG = 11;
const int LIST_GAMES_FLAG = 12;
//-------------
//Prototipi: 
void print_grid(char *grid);
/* Stampa la griglia di gioco */
void print_grid(char *grid)
{
    printf("\n");
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        printf(" ");
        for (int j = 0; j < TABLE_SIZE; j++)
        {
            printf(" %c ", grid[i * TABLE_SIZE + j]);
            if (j < TABLE_SIZE - 1)
                printf("|");
        }
        printf("\n");

        if (i < TABLE_SIZE - 1)
        {
            printf("-------------\n");
        }
    }
    printf("\n");
}
void clear_screen();
void show_menu();
int get_menu_choice();
int position_to_index(int pos);
int get_valid_move(char *grid);
void handle_game(SOCKET client_socket, char *player_name, char *opponent_name,
    char player_symbol, char opponent_symbol, char *grid);
void show_available_games(SOCKET client_socket);
void handle_join_private_game(SOCKET client_socket, char* player_name);

//-------------


/* Pulisce lo schermo */
void clear_screen()
{
    system("cls");
}

/* Mostra il menu principale */
void show_menu() {
    clear_screen();
    printf("=== TRIS ONLINE ===\n\n");
    printf("1. Partita casuale\n");
    printf("2. Crea stanza privata\n");
    printf("3. Unisciti a stanza privata\n");
    printf("4. Esci\n\n");
}
/* Ottiene la scelta del menu */
int get_menu_choice() {
    char input[10];
    int choice;

    while (1) {
        printf("Seleziona un'opzione (1-4): ");
        fgets(input, sizeof(input), stdin);

        if (sscanf(input, "%d", &choice) != 1 || choice < 1 || choice > 4) {
            printf("Scelta non valida. Inserisci un numero tra 1 e 4.\n");
            continue;
        }

        return choice;
    }
}


/* Converte coordinate 1-9 in indice della griglia (0-8) */
int position_to_index(int pos)
{
    return pos - 1;
}

/* Valida la mossa dell'utente */
int get_valid_move(char *grid)
{
    int position;
    char input[10];

    while (1)
    {
        printf("Inserisci la tua mossa (1-9): ");
        fgets(input, sizeof(input), stdin);

        if (sscanf(input, "%d", &position) != 1)
        {
            printf("Input non valido. Inserisci un numero tra 1 e 9.\n");
            continue;
        }

        if (position < 1 || position > 9)
        {
            printf("Posizione non valida. Inserisci un numero tra 1 e 9.\n");
            continue;
        }

        int index = position_to_index(position);
        if (grid[index] != ' ')
        {
            printf("Cella già occupata. Scegli un'altra posizione.\n");
            continue;
        }

        break;
    }

    return position_to_index(position);
}

/* Gestisce la partita */
void handle_game(SOCKET client_socket, char *player_name, char *opponent_name,
                 char player_symbol, char opponent_symbol, char *grid)
{
    int game_id = 0; // Variabile per memorizzare l'ID partita

    while (1)
    {
        int flag;

        if (recv(client_socket, (char *)&flag, sizeof(int), 0) <= 0)
        {
            fprintf(stderr, "Connessione con il server persa.\n");
            break;
        }

        switch (flag)
        {
        case WAIT_FLAG:
            clear_screen();
            printf("In attesa che un altro giocatore si connetta...\n");
            break;

        case START_FLAG:
        {
            clear_screen();

            // 1. Ricevi prima l'ID partita dal server
            int net_game_id;
            if (recv(client_socket, (char *)&net_game_id, sizeof(int), 0) <= 0)
            {
                fprintf(stderr, "Errore ricezione ID partita\n");
                return;
            }

            // Non lo so che è l'ho trovato su stackoverflow, non passava l'id
            int game_id = ntohl(net_game_id);

            printf("=== PARTITA INIZIATA ===\n");
            printf("Partita trovata! Inizio del gioco.\n");

            // 2. Poi ricevi i dati dell'avversario (come prima)
            int opponent_name_len;
            if (recv(client_socket, (char *)&opponent_name_len, sizeof(int), 0) <= 0)
            {
                fprintf(stderr, "Errore ricezione lunghezza nome avversario.\n");
                return;
            }
            opponent_name_len = ntohs(opponent_name_len);

            if (recv(client_socket, opponent_name, opponent_name_len, 0) <= 0)
            {
                fprintf(stderr, "Errore ricezione nome avversario.\n");
                return;
            }
            opponent_name[opponent_name_len] = '\0';

            if (recv(client_socket, &player_symbol, sizeof(char), 0) <= 0)
            {
                fprintf(stderr, "Errore ricezione simbolo giocatore.\n");
                return;
            }

            opponent_symbol = (player_symbol == 'X') ? 'O' : 'X';

            printf("Stai giocando contro: %s\n", opponent_name);
            printf("Il tuo simbolo: %c\n", player_symbol);

            memset(grid, ' ', GRID_SIZE);
            break;
        }

        case YOUR_MOVE_FLAG:
        {
            if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0)
            {
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

            if (send(client_socket, (const char *)&net_move, sizeof(int), 0) < 0)
            {
                fprintf(stderr, "Invio mossa fallito. Errore: %d\n", WSAGetLastError());
                return;
            }
            break;
        }

        case OPPONENT_MOVE_FLAG:
        {
            if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0)
            {
                fprintf(stderr, "Errore ricezione griglia.\n");
                return;
            }

            clear_screen();
            // 4. Aggiungi visualizzazione ID partita durante l'attesa
            printf("=== TURNO AVVERSARIO ===\n");
            printf("Tu: %c (%s) vs Avversario: %c (%s)\n",
                   player_symbol, player_name, opponent_symbol, opponent_name);

            print_grid(grid);
            printf("In attesa della mossa dell'avversario...\n");
            break;
        }

        case WIN_FLAG:
        {
            if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0)
            {
                fprintf(stderr, "Errore ricezione griglia.\n");
                return;
            }

            clear_screen();
            // 5. Aggiungi ID partita al risultato finale
            printf("=== VITTORIA! ===\n");
            printf("Hai sconfitto %s!\n", opponent_name);
            print_grid(grid);

            printf("Premi un tasto per uscire...\n");
            _getch();
            return;
        }

        case LOSE_FLAG:
        {
            if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0)
            {
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

        case DRAW_FLAG:
        {
            if (recv(client_socket, grid, GRID_SIZE * sizeof(char), 0) <= 0)
            {
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

/* Gestisce la creazione di una partita privata */
void handle_create_private_game(SOCKET client_socket, char* player_name) {
    clear_screen();
    printf("=== CREAZIONE STANZA PRIVATA ===\n\n");
    
    // Invia richiesta di creazione partita
    int flag = htons(CREATE_GAME_FLAG);
    int name_len = htons(strlen(player_name));
    
    if (send(client_socket, (const char*)&flag, sizeof(int), 0) < 0 ||
        send(client_socket, (const char*)&name_len, sizeof(int), 0) < 0 ||
        send(client_socket, player_name, strlen(player_name), 0) < 0) {
        fprintf(stderr, "Invio richiesta creazione fallito\n");
        return;
    }
    
    // Ricevi conferma con ID partita
    int game_id;
    if (recv(client_socket, (char*)&flag, sizeof(int), 0) <= 0 ||
        recv(client_socket, (char*)&game_id, sizeof(int), 0) <= 0) {
        fprintf(stderr, "Ricezione ID partita fallita\n");
        return;
    }
    
    game_id = ntohl(game_id);
    printf("Stanza privata creata con successo!\n");
    printf("ID Partita: %d\n", game_id);
    printf("In attesa che un giocatore si unisca...\n\n");
    
    // Aspetta una richiesta di partecipazione
    while (1) {
        if (recv(client_socket, (char*)&flag, sizeof(int), 0) <= 0) {
            fprintf(stderr, "Connessione con il server persa.\n");
            break;
        }
        
        flag = ntohs(flag);
        
        if (flag == GAME_REQUEST_FLAG) {
            // Ricevi nome del giocatore che vuole unirsi
            int req_name_len;
            char req_name[50];
            
            if (recv(client_socket, (char*)&req_name_len, sizeof(int), 0) <= 0 ||
                recv(client_socket, req_name, ntohs(req_name_len), 0) <= 0) {
                fprintf(stderr, "Ricezione richiesta fallita\n");
                break;
            }
            
            req_name[ntohs(req_name_len)] = '\0';
            
            // Chiedi all'utente se accettare
            printf("\nIl giocatore %s vuole unirsi alla tua partita.\n", req_name);
            printf("Accetti? (s/n): ");
            
            char response;
            scanf(" %c", &response);
            while(getchar() != '\n'); // Pulisci buffer
            
            // Invia risposta al server
            int response_flag = (response == 's' || response == 'S') ? 1 : 0;
            response_flag = htons(response_flag);
            
            if (send(client_socket, (const char*)&game_id, sizeof(int), 0) < 0 ||
                send(client_socket, (const char*)&response_flag, sizeof(int), 0) < 0) {
                fprintf(stderr, "Invio risposta fallito\n");
                break;
            }
            
            if (response_flag) {
                printf("Richiesta accettata. Inizio partita...\n");
                return; // La partita inizierà ora
            } else {
                printf("Richiesta rifiutata. In attesa di altri giocatori...\n");
            }
        }
    }
}
/* Gestisce la richiesta di unione a una partita privata */
void handle_join_private_game(SOCKET client_socket, char* player_name) {
    clear_screen();
    printf("=== UNISCITI A STANZA PRIVATA ===\n\n");
    
    // Dichiarazione delle variabili mancanti
    char opponent_name[50] = {0};
    char player_symbol = ' ', opponent_symbol = ' ';
    char grid[GRID_SIZE];
    memset(grid, ' ', GRID_SIZE);
    
    // Chiedi ID partita
    int game_id;
    char input[20];
    
    printf("Inserisci l'ID della partita a cui vuoi unirti: ");
    fgets(input, sizeof(input), stdin);
    
    if (sscanf(input, "%d", &game_id) != 1) {
        printf("ID non valido.\n");
        return;
    }
    
    // Invia richiesta di unione
    int flag = htons(JOIN_GAME_FLAG);
    int name_len = htons(strlen(player_name));
    int net_game_id = htonl(game_id);
    
    if (send(client_socket, (const char*)&flag, sizeof(int), 0) < 0 ||
        send(client_socket, (const char*)&name_len, sizeof(int), 0) < 0 ||
        send(client_socket, player_name, strlen(player_name), 0) < 0 ||
        send(client_socket, (const char*)&net_game_id, sizeof(int), 0) < 0) {
        fprintf(stderr, "Invio richiesta unione fallito\n");
        return;
    }
    
    // Ricevi la risposta dal server
    int response_flag;
    if (recv(client_socket, (char*)&response_flag, sizeof(int), 0) <= 0) {
        fprintf(stderr, "Connessione con il server persa.\n");
        return;
    }
    
    response_flag = ntohs(response_flag);
    
    if (response_flag == GAME_ACCEPTED_FLAG) {
        printf("Richiesta accettata! Partita in corso...\n");
        // Aspetta l'inizio effettivo della partita
        handle_game(client_socket, player_name, opponent_name, player_symbol, opponent_symbol, grid);
    } else if (response_flag == GAME_REJECTED_FLAG) {
        printf("Richiesta rifiutata. Il creatore ha declinato la tua richiesta.\n");
    } else {
        printf("Errore: partita non trovata o già iniziata.\n");
    }
    
    printf("\nPremi un tasto per continuare...");
    _getch();
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

    // Configurazione iniziale
    if (argc > 1) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
        if (argc > 2) {
            server_port = atoi(argv[2]);
        }
    }

    // Inizializzazione Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup fallito. Errore: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Input nome giocatore
    clear_screen();
    printf("=== TRIS ONLINE ===\n\n");
    printf("Inserisci il tuo nome : ");
    fgets(player_name, sizeof(player_name), stdin);
    player_name[strcspn(player_name, "\n")] = '\0';

    // Menu principale
    while (1) {
        show_menu();
        int choice = get_menu_choice();

        client_socket = INVALID_SOCKET;

        switch (choice) {
            case 1: { // Partita casuale
                if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
                    fprintf(stderr, "Creazione socket fallita. Errore: %d\n", WSAGetLastError());
                    break;
                }
            
                server.sin_addr.s_addr = inet_addr(server_ip);
                server.sin_family = AF_INET;
                server.sin_port = htons(server_port);
            
                if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
                    fprintf(stderr, "Connessione al server fallita. Errore: %d\n", WSAGetLastError());
                    closesocket(client_socket);
                    break;
                }
            
                // Invia prima il flag di partita casuale
                int casual_flag = htons(WAIT_FLAG); // Usiamo WAIT_FLAG per partita casuale
                if (send(client_socket, (const char*)&casual_flag, sizeof(int), 0) < 0) {
                    fprintf(stderr, "Invio flag partita casuale fallito\n");
                    closesocket(client_socket);
                    break;
                }
            
                // Poi invia i dati del giocatore
                int name_len = strlen(player_name);
                int net_name_len = htons(name_len);
            
                if (send(client_socket, (const char*)&net_name_len, sizeof(int), 0) < 0 ||
                    send(client_socket, player_name, name_len, 0) < 0) {
                    fprintf(stderr, "Invio dati giocatore fallito\n");
                    closesocket(client_socket);
                    break;
                }
            
                printf("\nConnessione al server stabilita. In attesa di un avversario...\n");
                handle_game(client_socket, player_name, opponent_name, player_symbol, opponent_symbol, grid);
                break;
            }
            case 2: { // Crea stanza privata
                if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
                    fprintf(stderr, "Creazione socket fallita. Errore: %d\n", WSAGetLastError());
                    break;
                }

                server.sin_addr.s_addr = inet_addr(server_ip);
                server.sin_family = AF_INET;
                server.sin_port = htons(server_port);

                if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
                    fprintf(stderr, "Connessione al server fallita. Errore: %d\n", WSAGetLastError());
                    closesocket(client_socket);
                    break;
                }

                handle_create_private_game(client_socket, player_name);
                handle_game(client_socket, player_name, opponent_name, player_symbol, opponent_symbol, grid);
                break;
            }

            case 3: { // Unisciti a stanza privata
                if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
                    fprintf(stderr, "Creazione socket fallita. Errore: %d\n", WSAGetLastError());
                    break;
                }

                server.sin_addr.s_addr = inet_addr(server_ip);
                server.sin_family = AF_INET;
                server.sin_port = htons(server_port);

                if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
                    fprintf(stderr, "Connessione al server fallita. Errore: %d\n", WSAGetLastError());
                    closesocket(client_socket);
                    break;
                }

                handle_join_private_game(client_socket, player_name);
                handle_game(client_socket, player_name, opponent_name, player_symbol, opponent_symbol, grid);
                break;
            }

            case 4: // Esci
                WSACleanup();
                printf("\nArrivederci!\n");
                return EXIT_SUCCESS;
        }

        if (client_socket != INVALID_SOCKET) {
            closesocket(client_socket);
        }
    }

    WSACleanup();
    return EXIT_SUCCESS;
}
