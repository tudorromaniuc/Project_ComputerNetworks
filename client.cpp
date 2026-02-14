#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <signal.h>  

using namespace std;

#define PORT 2023
#define IP "127.0.0.1"

void afiseaza_meniu() {
    printf("\n--- COMENZI DISPONIBILE ---\n");
    printf("1. REGISTER <user> <pass> <mail>  -> Inregistrare cont nou\n");
    printf("2. LOGIN <user> <pass>            -> Autentificare\n");
    printf("3. LISTA                          -> Vezi campionate disponibile\n");
    printf("4. JOIN <id_campionat>            -> Inscriere in campionat\n");
    printf("5. ADAUGA_CAMP <nume> <joc> <nr> <tip> -> (Admin) Creare campionat\n");
    printf("6. REPROGRAMEAZA <data_noua>      -> Schimba data meciului tau\n");
    printf("7. SET_SCOR <id_meci> <s1> <s2>   -> (Admin/User) Actualizeaza scor\n");
    printf("8. ISTORIC                        -> (Admin) Vezi toate meciurile\n");
    printf("9. LOGOUT / EXIT                  -> Iesire\n");
    printf("---------------------------\nComanda ta: ");
    fflush(stdout);
}

int main() {
    int sd;
    struct sockaddr_in server;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(IP);
    server.sin_port = htons(PORT);

    if (connect(sd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Eroare la conectare");
        return 1;
    }

    printf("Conectat la server!\n");
    afiseaza_meniu();

    int pid = fork();

    if (pid == 0) {
        char buffer[4096];
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int n = recv(sd, buffer, sizeof(buffer)-1, 0);
            if (n <= 0) {
                printf("\nServerul a inchis conexiunea.\n");
                kill(getppid(), SIGKILL);
                exit(0);
            }
            printf("\n[SERVER]:\n%s\n", buffer);
            printf("Comanda ta: ");
            fflush(stdout);
        }
    } else {
        char buffer[1024];
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);
            
            if (strncmp(buffer, "EXIT", 4) == 0) {
                kill(pid, SIGKILL);
                break;
            }

            if (strlen(buffer) > 1) {
                send(sd, buffer, strlen(buffer), 0);
            }
            usleep(100000); 
        }
    }

    close(sd);
    return 0;
}