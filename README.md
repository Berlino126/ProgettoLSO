# ProgettoLSO
# Tris Online – Progetto Client/Server

Questo progetto implementa una versione multiplayer del gioco Tris (Tic-Tac-Toe) in C, utilizzando socket TCP per la comunicazione client-server. Il sistema supporta partite casuali e stanze private, con possibilità di accettare o rifiutare avversari.

## 📦 Requisiti

- Docker
- Docker Compose

## 🚀 Avvio del progetto

1. Clona o scarica il progetto.
2. Posizionati nella cartella principale:

   ```bash
   cd Progetto_LSO
   ```

3. Avvia il sistema:

   ```bash
   docker-compose up --build
   ```

> Questo comando compilerà automaticamente `server.c` e `client.c`, e avvierà i container `tris_server`, `tris_client1` e `tris_client2`.

---

## 🎮 Come giocare

Apri due terminali separati:

### Primo giocatore

```bash
docker exec -it tris_client1 /bin/bash
./client
```

### Secondo giocatore

```bash
docker exec -it tris_client2 /bin/bash
./client 
```

> Il client presenta un menu per scegliere tra: partita casuale, creazione o accesso a stanza privata.

---

## 🛠 Struttura del progetto

```
Progetto_LSO/
├── server/
│   └── server.c
├── client/
│   └── client.c
├── Dockerfile
└── docker-compose.yml
```

- `server.c`: codice del server, gestisce più partite in parallelo tramite thread.
- `client.c`: client testuale, consente l’interazione da terminale.
- `Dockerfile`: compila sia server che client.
- `docker-compose.yml`: definisce i servizi e la rete condivisa.
