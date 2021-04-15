#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// alcune variabili globali di utilitá
#define BUFFER_SIZE 1024
#define LUNGHEZZA_SESSION_ID 11

char comando[BUFFER_SIZE];
char parola1[BUFFER_SIZE];
char parola2[BUFFER_SIZE];

char sessionId[LUNGHEZZA_SESSION_ID]; // contiene il session id generato ad ogni login

int logOk = -1;

const char* comandi = " \n*************** GIOCO DEL LOTTO *************** \n"
                      " Sono disponibili i seguenti comandi: \n \n "
                      " 1) !help <comando> --> mostra i dettagli di un comando\n"
                      " 2) !signup <username> <password> --> crea un nuovo utente\n"
                      " 3) !login <username> <password> --> autentica un utente\n"
                      " 4) !invia_giocata g --> invia una giocata g al server\n"
                      " 5) !vedi_giocate tipo --> visualizza le giocate precedenti dove tipo = (0,1) e \n"
                      " \t \t \t permette di visualizzare le giocate passate '0'\n"
                      " \t \t \t oppure le giocate attive '1' (ancora non estratte)\n"
                      " 6) !vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime\n"
                      " \t \t \t n estrazioni sulla ruota specificata \n"
                      " 7) !vedi_vincite -> visualizza le vincite di un utente\n"
                      " 8) !esci --> termina il client\n ";

// funzione chiamata all'avvio del sistema
static inline void prompt(){
    printf("%s",comandi);
}

int main(int argc, char** argv){

    int ret, sd, len;
    uint16_t lmsg;
    struct sockaddr_in srv_addr;
    char buffer[BUFFER_SIZE];
    char stato[BUFFER_SIZE];
    char controlloIP[BUFFER_SIZE];
    int portaServer;
    char* ipServer;

    if(argc != 3){
        perror("[ERR] Non ci sono i parametri\n");
        exit(-1);
    } else {
        ipServer = argv[1];
        printf("[LOG] IpServer: %s\n", ipServer);
        portaServer = atoi(argv[2]);
        printf("[LOG] PortaServer: %d\n", portaServer);    
    }

    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);

    /* Creazione indirizzo del server */
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(portaServer);
    inet_pton(AF_INET, ipServer, &srv_addr.sin_addr);
    
    ret = connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    if(ret < 0){
        perror("[ERR] Errore in fase di connessione: \n");
        exit(-1);
    }

    // CONTROLLO IP
    ret = recv(sd, (void*)&lmsg, sizeof (uint16_t), 0);      
    if(ret < 0){
        perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
        exit(-1);
    }

    len = ntohs(lmsg);
    ret = recv(sd, (void*)controlloIP, len, 0);
    if(ret < 0){
        perror("[ERR] Errore in fase di ricezione: \n");
        exit(-1);
    }

    if(ret == 0){
        printf("[LOG] Il tuo indirizzo IP risulta bloccato, riprova più tardi\n");
        exit(-1);
    }
    // )

    prompt();
    
    while(1){
        printf("Inserisci un comando: ");
        // pulisco buffer
        strcpy(buffer,"");
        strcpy(comando,"");
        strcpy(parola1,"");
        strcpy(parola2,"");
        strcpy(stato,"");

        // Attendo input da tastiera
        char* str = fgets(buffer, BUFFER_SIZE, stdin);
        if(str == NULL){
            printf("[LOG] Immetti un comando valido!\n");
            continue;
        }

        sscanf(buffer, "%s %s %s", comando, parola1, parola2);

        // printf("[DEBUG] Comando: %s\n", comando);

        if( (strlen(comando) == 0) || (strcmp(comando," ") == 0) ){
            printf("[ERR] Non hai inserito nessun comando! Prova ad inserire uno dei seguenti:\n");
            prompt();
            continue;
        }

        if(strcmp(comando, "!help") == 0){
            printf("[LOG] Inserito comando help\n");
            // printf("[DEBUG] Comando inserito: %s\n", (parola1 == NULL) ? "" : parola1);
            if(strcmp(parola1,"") == 0){
                prompt();
                continue;
            } else if(strcmp(parola1,"!signup") == 0){
                printf("Formato della !signup: !signup <username> <password>\n");
                continue;
            } else if(strcmp(parola1,"!login") == 0){
                printf("Formato !login: !login <username> <password>\n");
                printf("Fai attenzione, se fallisci il login per 3 volte verrai bloccato per 30 minuti\n");
                continue;
            } else if(strcmp(parola1,"!invia_giocata") == 0){
                printf("Formato !invia_giocata: !invia_giocata schedina\n");
                printf("Formato schedina: -r (ruote) -n (numeri) -i (importi)\n");
                printf("Ricordati di inserire un numero compreso tra 1 e 90 e non giocare numeri duplicati nella stessa schedina!\n");
                continue;
            } else if (strcmp(parola1,"!vedi_giocata") == 0){
                printf("Formato !vedi_giocata: !vedi_giocata tipo\n");
                printf("Tipo -> 0 per le giocate passate, Tipo -> 1 per le giocate in attesa di estrazione\n");
                continue;
            } else if (strcmp(parola1,"!vedi_estrazione") == 0){
                printf("Formato !vedi_estrazione: !vedi_estrazione n [ruota]\n");
                printf("Inserisci in 'n' il numero delle estrazioni che vuoi visualizzare. L'argomento ruota è opzionale, se non lo specifichi verranno mostrate tutte le ruote\n");
                continue;
            } else if(strcmp(parola1,"!vedi_vincite") == 0){
                printf("Formato !vedi_vincite: !vedi_vincite\n");
                continue;
            } else if(strcmp(parola1,"!esci") == 0){
                printf("Formato !esci: !esci\n");
                continue;
            } else {
                printf("[ERR] Comando non riconosciuto..\n");
                continue;
            }

        }

        // se sono qui non ho chiesto l'help, cose che posso fare da non registrato e loggato:
        // !signup, !login, !help
        
        if(strcmp(comando,"!signup") == 0 && (strlen(parola1) == 0 || strlen(parola2) == 0)){
            printf("[LOG] Immetti uno username e una password!\n");
            continue;
        }

        if(strcmp(comando,"!signup") != 0) {
            if(strcmp(comando,"!login") != 0){
                if(logOk != 1) {
                    printf("[LOG] Non sei loggato correttamente!\n");
                    continue;
                } 
            } else {
                if(logOk == 1) {
                    printf("[LOG] Sei già loggato correttamente!\n");
                    continue;
                }
            }
        } else {
            if(logOk == 1) {
                printf("[LOG] Sei già registrato e loggato correttamente\n");
                continue;
            }
        }

        // controllo il formato della invia_giocata 
        if(strcmp(comando,"!invia_giocata") == 0){
            if( (strlen(parola1) == 0) || (strlen(parola2) == 0) ){
                printf("[LOG] Immetti una giocata nel formato corretto\n");
                continue;
            }
        }

        if(strcmp(comando,"!vedi_giocate") == 0){
            if( strlen(parola1) == 0 ){
                printf("[LOG] Formato non valido per la !vedi_giocate\n");
                continue;
            }
        }

        if(strcmp(comando,"!vedi_vincite") == 0){
            if( (strlen(parola1) != 0) || (strlen(parola2) != 0) ){
                printf("[LOG] Formato non valido per la !vedi_vincite\n");
                continue;
            }
        }

        if(strcmp(comando,"!vedi_estrazioni") == 0){
            if(strlen(parola1) == 0){
                printf("[LOG] Formato non corretto per la !vedi_estrazioni\n");
                continue;
            }
        }

        // Invio al server la quantita di dati
        len = strlen(buffer) + 1; // Voglio inviare anche il carattere di fine stringa
        lmsg = htons(len);

        ret = send(sd, (void*) &lmsg, sizeof(uint16_t), 0);
        
        ret = send(sd, (void*) buffer, len, 0);

        // devo validare il sessionID
        if( (strcmp(comando,"!signup") != 0) && (strcmp(comando,"!login") != 0) ){

            len = strlen(sessionId) + 1; 
            lmsg = htons(len);

            ret = send(sd, (void*) &lmsg, sizeof(uint16_t), 0);
            
            printf("[LOG] Invio sessionId da verificare %s\n",sessionId);
            ret = send(sd, (void*) sessionId, len, 0);

            // Attendo risposta
            char isValid[10];
            ret = recv(sd, (void*)&lmsg, sizeof (uint16_t), 0);      
            if(ret < 0){
                perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
                exit(-1);
            }

            len = ntohs(lmsg);

            ret = recv(sd, (void*)isValid, len, 0);
            if(ret < 0){
                perror("[ERR] Errore in fase di ricezione: \n");
                exit(-1);
            }

            if(strcmp(isValid,"nonvalido") == 0){
                printf("[ERR] Session id non valido\n");
                exit(-1);
            }
        }

        // Attendo risposta al comando
        ret = recv(sd, (void*)&lmsg, sizeof (uint16_t), 0);      
        if(ret < 0){
            perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
            exit(-1);
        }

        len = ntohs(lmsg);

        ret = recv(sd, (void*)stato, len, 0);
        if(ret < 0){
            perror("[ERR] Errore in fase di ricezione: \n");
            exit(-1);
        }

        // ricevo l'esito del mio comando
        // printf("[LOG] Stato: %s\n", stato);

        if( strcmp(stato, "occupato") == 0 ){
            printf("[ERR] Username occupato, sceglierne un altro..\n");
            continue;
        } else if( strcmp(stato, "libero") == 0){
            printf("[LOG] Evvai, registrato!\n");
            continue;
        } else if( strcmp(stato,"tentativiEsauriti") == 0){
            printf("[ERR] Hai esaurito i tentativi, riprova tra 30 minuti\n");
            logOk = -1;
            break;
        } else if( strcmp(stato,"mancante") == 0){
            printf("[ERR] Non hai inserito lo username o la password, prova di nuovo\n");
            logOk = -1;
            continue;
        } else if( strcmp(stato,"loginNo") == 0){
            printf("[ERR] Hai sbagliato la password o il nome utente, prova a immetterli nuovamente\n");
            logOk = -1;
            continue;
        }else if( strcmp(stato,"bloccato") == 0){
            printf("[ERR] Non puoi fare il login perchè il tuo ip è bloccato, ritenta tra poco\n");
            logOk = -1;
            break;
        } else if( strcmp(stato,"loginOk") == 0){
            printf("[LOG] Login effettuato!\n");
            logOk = 1;

            // Attendo risposta sessionIDrisposta
            ret = recv(sd, (void*)&lmsg, sizeof (uint16_t), 0);      
            if(ret < 0){
                perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
                exit(-1);
            }

            len = ntohs(lmsg);         

            ret = recv(sd, (void*)sessionId, len, 0);
            if(ret < 0){
                perror("[ERR] Errore in fase di ricezione: \n");
                exit(-1);
            } 

            printf("[LOG] sessionID ricevuto %s\n", sessionId);
            continue;

        } else if(ret == 0){
            logOk = -1;
            printf("[LOG] Sei stato disconnesso!\n");
            break;
        } else if (strcmp(stato,"invioOk") == 0) {
            printf("[LOG] Invio giocata effettuato con successo!\n");
            continue;
        } else if (strcmp(stato,"formatoscorretto") == 0){
            printf("[LOG] Hai sbagliato ad inserire il comando, prova ad digitare !help se non te lo ricordi!\n");
            continue;
        } else if(strcmp(stato,"sbagliato") == 0) {
            printf("[ERR] Inserito un comando non riconosciuto!\n");
            continue;
        } else if (strcmp(stato,"inizioschedine") == 0) {
            printf("[LOG] Stampo le schedine giocate dall'utente..\n");

            while(1){
                // ricevo le schedine una alla volta
                ret = recv(sd, (void*)&lmsg, sizeof (uint16_t), 0);      
                if(ret < 0){
                    perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
                    exit(-1);
                } 
                len = ntohs(lmsg);
                char giocate[len];

                ret = recv(sd, (void*)giocate, len, 0);
                if(ret < 0){
                    perror("[ERR] Errore in fase di ricezione: \n");
                    exit(-1);
                } 

                if(strcmp(giocate,"finegiocate") == 0)
                    break;

                if(strlen(giocate) != 0){
                    printf("%s",giocate);
                    printf("**************************\n");
                }
            }

            printf("[LOG] Fine giocate\n");
        } else if(strcmp(stato,"disconnesso") == 0){
            printf("[LOG] Richiesto il logout\n");
            logOk = -1;
            strcpy(sessionId,"");
            break;
        } else if (strcmp(stato,"iniziovincite") == 0) {

            while(1){
                ret = recv(sd, (void*)&lmsg, sizeof (uint16_t), 0);      
                if(ret < 0){
                    perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
                    exit(-1);
                } 

                len = ntohs(lmsg);
                char vincite[len];

                ret = recv(sd, (void*)vincite, len, 0);
                if(ret < 0){
                    perror("[ERR] Errore in fase di ricezione: \n");
                    exit(-1);
                } 

                printf("%s\n", vincite);

                if(strcmp(vincite,"finevincite") == 0){
                    printf("[LOG] Fine vincite\n");
                    break;
                }
            }
            
            // attendo consuntivo delle vincite
            ret = recv(sd, (void*)&lmsg, sizeof (uint16_t), 0);      
            if(ret < 0){
                perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
                exit(-1);
            } 

            len = ntohs(lmsg);
            char consuntivo[len];

            ret = recv(sd, (void*)consuntivo, len, 0);
            if(ret < 0){
                perror("[ERR] Errore in fase di ricezione: \n");
                exit(-1);
            } 

            printf("%s\n",consuntivo);
        } else if(strcmp(stato,"nonvisualizzate") == 0){
            printf("[LOG] Il formato del comando è scorretto\n");
            continue;
        } else if(strcmp(stato,"inizioestrazioni") == 0){
            printf("[LOG] Stampo le ultime estrazioni\n");
            
            while(1){
                ret = recv(sd, (void*)&lmsg, sizeof (uint16_t), 0);      
                if(ret < 0){
                    perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
                    exit(-1);
                } 

                len = ntohs(lmsg);
                char estrazioni[len];

                ret = recv(sd, (void*)estrazioni, len, 0);
                if(ret < 0){
                    perror("[ERR] Errore in fase di ricezione: \n");
                    exit(-1);
                } 

                if(strcmp(estrazioni,"fineestrazioni") == 0){
                    printf("[LOG] Estrazioni finite\n");
                    break;
                }

                char formattato[BUFFER_SIZE];
                strcpy(formattato,estrazioni);

                // formattazione estrazione
                if(strlen(formattato) != 0)
                    printf("%s\n",formattato);
            }

            continue;
        } else{
            // non ci devo arrivare
            printf("[ERR] ??? %s\n", stato);
            continue;
        }
            
    }
    close(sd);   
	return 0;
}
    
    
