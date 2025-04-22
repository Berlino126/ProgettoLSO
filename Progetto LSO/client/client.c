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
#define CREATE_PRIVATE 10
#define PRIVATE_CREATED 11
#define JOIN_PRIVATE 12
#define JOIN_REQUEST 13
#define JOIN_ACCEPTED 14
#define JOIN_REJECTED 15
#define NO_FLAG 0 
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

/* Pulisce lo schermo */
void clear_screen()
{
    system("cls");
}

/* Mostra il menu principale */
void show_menu()
{
    clear_screen();
    printf("=== TRIS ONLINE ===\n\n");
    printf("1. Partita casuale\n");
    printf("2. Crea stanza privata\n");
    printf("3. Unisciti a stanza privata\n");
    printf("4. Esci\n\n");
}
/* Ottiene la scelta del menu */
int get_menu_choice()
{
    char input[10];
    int choice;

    while (1)
    {
        printf("Seleziona un'opzione (1-4): ");
        fgets(input, sizeof(input), stdin);

        if (sscanf(input, "%d", &choice) != 1 || choice < 1 || choice > 4)
        {
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
            printf("ID Partita: %d\n", game_id);
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
            printf("ID Partita: %d\n", game_id);
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
            printf("ID Partita: %d\n", game_id);
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
            printf("ID Partita: %d\n", game_id);
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
int main(int argc, char *argv[])
{
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
    if (argc > 1)
    {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
        if (argc > 2)
        {
            server_port = atoi(argv[2]);
        }
    }

    // Inizializzazione Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
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
    while (1)
    {
        show_menu();
        int choice = get_menu_choice();

        client_socket = INVALID_SOCKET;

        switch (choice)
        {
            case 1: {
                client_socket = socket(AF_INET, SOCK_STREAM, 0);
                server.sin_addr.s_addr = inet_addr(server_ip);
                server.sin_family = AF_INET;
                server.sin_port = htons(server_port);
                connect(client_socket, (struct sockaddr*)&server, sizeof(server));
            
                int flag = NO_FLAG;
                send(client_socket, (const char*)&flag, sizeof(int), 0);
            
                int name_len = strlen(player_name);
                int net_name_len = htons(name_len);
                send(client_socket, (const char*)&net_name_len, sizeof(int), 0);
                send(client_socket, player_name, name_len, 0);
            
                printf("Connessione al server stabilita. In attesa di un avversario...");
                handle_game(client_socket, player_name, opponent_name, player_symbol, opponent_symbol, grid);
                break;
            }

        case 2: {
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
            server.sin_addr.s_addr = inet_addr(server_ip);
            server.sin_family = AF_INET;
            server.sin_port = htons(server_port);
            connect(client_socket, (struct sockaddr*)&server, sizeof(server));
        
            int flag = CREATE_PRIVATE;
            send(client_socket, (const char*)&flag, sizeof(int), 0);
        
            int name_len = strlen(player_name);
            int net_name_len = htons(name_len);
            send(client_socket, (const char*)&net_name_len, sizeof(int), 0);
            send(client_socket, player_name, name_len, 0);
        
            int resp_flag, net_room_id;
            recv(client_socket, (char*)&resp_flag, sizeof(int), 0);
            recv(client_socket, (char*)&net_room_id, sizeof(int), 0);
        
            int room_id = ntohl(net_room_id);
            printf("Stanza privata creata. ID: %d\nAspettando richieste...\n", room_id);
        
            while (1) {
                recv(client_socket, (char*)&resp_flag, sizeof(int), 0);
                if (resp_flag == JOIN_REQUEST) {
                    int len;
                    recv(client_socket, (char*)&len, sizeof(int), 0);
                    len = ntohs(len);
                    char joiner_name[50];
                    recv(client_socket, joiner_name, len, 0);
                    joiner_name[len] = '\0';
                    printf("%s vuole unirsi. Accetti? (y/n): ", joiner_name);
                    char ch = getchar();
                    getchar();
                    int reply = (ch == 'y' || ch == 'Y') ? JOIN_ACCEPTED : JOIN_REJECTED;
                    send(client_socket, (const char*)&reply, sizeof(int), 0);
                    if (reply == JOIN_ACCEPTED) {
                        handle_game(client_socket, player_name, joiner_name, player_symbol, opponent_symbol, grid);
                        break;
                    }
                }
            }
            break;
        }
        
        // CLIENT.C - case 3 (unisciti)
        case 3: {
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
            server.sin_addr.s_addr = inet_addr(server_ip);
            server.sin_family = AF_INET;
            server.sin_port = htons(server_port);
            connect(client_socket, (struct sockaddr*)&server, sizeof(server));
        
            int flag = JOIN_PRIVATE;
            send(client_socket, (const char*)&flag, sizeof(int), 0);
        
            printf("Inserisci ID stanza privata: ");
            int room_id;
            scanf("%d", &room_id);
            getchar();
            int net_room_id = htonl(room_id);
            send(client_socket, (const char*)&net_room_id, sizeof(int), 0);
        
            int name_len = strlen(player_name);
            int net_name_len = htons(name_len);
            send(client_socket, (const char*)&net_name_len, sizeof(int), 0);
            send(client_socket, player_name, name_len, 0);
        
            int response;
            recv(client_socket, (char*)&response, sizeof(int), 0);
        
            if (response == JOIN_ACCEPTED) {
                printf("Richiesta accettata! Avvio partita...\n");
                handle_game(client_socket, player_name, opponent_name, player_symbol, opponent_symbol, grid);
            } else {
                printf("Richiesta rifiutata o stanza non trovata.\n");
            }
            break;
        }
        
        
        case 4: // Esci
            WSACleanup();
            printf("\nArrivederci!\n");
            return EXIT_SUCCESS;
        }

        if (client_socket != INVALID_SOCKET)
        {
            closesocket(client_socket);
        }

        printf("\nPremi un tasto per continuare...");
        _getch();
    }

    WSACleanup();
    return EXIT_SUCCESS;
}
