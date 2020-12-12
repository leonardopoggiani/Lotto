#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

#define MAX_LEN 1024 
#define POLLING_TIME 5*60 // default
#define MAX_TENTATIVI 3 
#define LUNGHEZZA_SESSION_ID 11
#define epsilon 0.0001 // per confronto tra timestamp

const char *PERCORSO_RELATIVO = "/giocatori/";
const char *POSIZIONE_FILE_REGISTRO = "./RegistroGiocatori/";
const char *fileBin = ".bin";
const char *registroLogin = "registroLogin.txt";

int ret, sd, new_sd;
socklen_t len;
uint16_t lmsg;
pid_t pid;
struct sockaddr_in my_addr, cl_addr;
int periodo;
FILE *fileLog;
FILE *registro;
FILE *dirRegistro;

char risposta[MAX_LEN];
char ris[MAX_LEN];

char utenteLoggato[MAX_LEN];
char savedSessionId[LUNGHEZZA_SESSION_ID];

// memorizza le informazioni relative ad un login fallito per 3 volte
struct fileLogin
{
    int addr_cl_temp;
    unsigned long int timestamp;
};
struct fileLogin logInfo;

struct estrazione {
    int ruota;
    int numeri[5];
    int timestamp;
};

// mette in utenteDaControllare il path del file binario del giocatore con lo username specificato
void trovaBinarioUtente(char *username, char utenteDaControllare[])
{
    char filename[MAX_LEN];
    char directory[MAX_LEN];

    // costruzione path
    strcpy(directory, POSIZIONE_FILE_REGISTRO);
    strcpy(filename, username);
    strcat(filename, fileBin);
    strcat(directory, filename);

    // printf("[DEBUG] File binario associato all'utente: %s\n", directory);
    strcpy(utenteDaControllare, directory);
}

int controlloUsername(char *username)
{
    const int usernameLibero = 0;
    const int usernameOccupato = -1;
    char utenteDaControllare[MAX_LEN];
    trovaBinarioUtente(username, utenteDaControllare);

    FILE* ok = fopen(utenteDaControllare, "rb");

    // printf("[DEBUG] Nome file controllato: %s\n", utenteDaControllare);

    // se non trovo un file binario con il nome dell'utente, lo username è libero
    int rv = ((ok == NULL) ? usernameLibero : usernameOccupato);

    if(ok != NULL)
        fclose(ok);

    return (rv == usernameLibero) ? usernameLibero : usernameOccupato;
}

// registra un giocatore con username e password creando il file binario corrispondente nella cartella RegistroGiocatori
void registra_giocatore(char *username, char *password)
{
    // printf("[DEBUG] Username: %s, Password: %s\n", username, password);
    char reg[MAX_LEN];
    trovaBinarioUtente(username, reg);

    FILE *registrazioneSuRegistro = fopen(reg, "ab+");

    printf("[LOG] Utente: %s registrato con successo!\n", username);
    fprintf(registrazioneSuRegistro, "%s %s\n", username, password);
    fclose(registrazioneSuRegistro);

    return;
}

//controlla che l'user inserito dall'utente che intende registrarsi sia libero
int controllaUsernameRegistrazione(char *username)
{
    const int usernameOccupato = -1;
    const int usernameLibero = 0;

    if (controlloUsername(username) == usernameOccupato)
    {
        printf("[LOG] Username gia presente! Sceglierne uno nuovo!\n");
        return usernameOccupato;
    }
    else
    {
        printf("[LOG] Username ok! Puoi proseguire\n");
        return usernameLibero;
    }
}

// controlla che lo username dell'utente che intende fare login è corretto
int controllaUsernameLogin(char *username)
{
    const int correttamenteRegistrato = 0;
    const int nonRegistrato = -1;

    // se l'username non è presente (controlloUsername == 0) allora non è correttamente loggato
    if (controlloUsername(username) != correttamenteRegistrato)
    {
        // printf("[DEBUG] L'utente è correttamente registrato, cerco la password..\n");
        return correttamenteRegistrato;
    }
    else
    {
        printf("[ERR] Utente non registrato\n");
        return nonRegistrato;
    }
}

int tentativiRestanti = MAX_TENTATIVI;
int logOk = -1;

int login(char *username, char *password)
{   
    const int nonRegistrato = -1;
    const int tentativiEsauriti = -2;
    const int tentativoFallito = -1;

    // printf("[DEBUG] !LOGIN avviata, username: %s, password: %s\n", username, password);

    if (controllaUsernameLogin(username) == nonRegistrato)
    {
        tentativiRestanti--;
        printf("[ERR] Utente non registrato, non può fare il login. Tentativi rimanenti: %d\n", tentativiRestanti);
        if (tentativiRestanti == 0)
        {
            return tentativiEsauriti;
        }
        else
        {
            return tentativoFallito;
        }
    }

    char fileBinario[MAX_LEN];
    trovaBinarioUtente(username, fileBinario);

    FILE *controlloPassword = fopen(fileBinario, "rb");

    if (controlloPassword == NULL)
    {
        tentativiRestanti--;
        printf("[ERR] File di registrazione non presente. Tentativi rimanenti: %d\n ", tentativiRestanti);
        if (tentativiRestanti == 0)
        {
            return tentativiEsauriti;
        }
        else
        {
            return tentativoFallito;
        }
    }

    char res[MAX_LEN];
    char *str = fgets(res, MAX_LEN, controlloPassword);

    if (str == NULL)
    {
        tentativiRestanti--;
        printf("[ERR] Errore in lettura del file. Tentativi rimanenti: %d\n", tentativiRestanti);
        if (tentativiRestanti == 0)
        {
            fclose(controlloPassword);
            return tentativiEsauriti;
        }
        else
        {
            fclose(controlloPassword);
            return tentativoFallito;
        }
    }

    char passwordRicevuta[MAX_LEN], usernameRicevuto[MAX_LEN];

    sscanf(str, "%s %s", usernameRicevuto, passwordRicevuta);
    // printf("[LOG] str: %s\n", passwordRicevuta);

    const int loggato = 0;

    if (strcmp(passwordRicevuta, password) == 0)
    {   
        strcpy(utenteLoggato, username);
        logOk = 1;
        fclose(controlloPassword);
        printf("[LOG] Ok, login effettuato!\n");
        return loggato;
    }
    else
    {
        tentativiRestanti--;
        printf("[ERR] Password sbagliata, login fallito. Tentativi rimanenti: %d\n", tentativiRestanti);
        if (tentativiRestanti == 0)
        {
            fclose(controlloPassword);
            return tentativiEsauriti;
        }
        else
        {
            fclose(controlloPassword);
            return tentativoFallito;
        }
    }

    const int errore = -3;
    // se tutto è andato bene qui non dovrei arrivarci
    return errore;
}

void generaSessionID(char id[])
{
    strcpy(id,"");
    srand(time(0));
    int i = 0;

    while (i < 3)
    {
        id[i] = 48 + (rand() % 10);
        i++;
    }

    while (i < 10)
    {
        id[i] = 97 + rand() % 26;
        i++;
    }

    id[10] = '\0';

    printf("[LOG] SessionID creato: %s \n", id);
    return;
}

struct giocata
{
    int ruoteGiocate[11];
    int numeriGiocati[10];
    double importiGiocati[5];
};
struct giocata g;

static inline void inizializza_schedina()
{
    memset(g.ruoteGiocate, 0, 11*sizeof(int));
    memset(g.numeriGiocati, 0, 10*sizeof(int));
    memset(g.importiGiocati, 0, 5*sizeof(double));
}

static inline int da_nome_a_codifica_citta(char citta[])
{
    // printf("[DEBUG] Decodifico nome citta inserito..\n");
    if (strcmp(citta, "Bari") == 0 || strcmp(citta, "bari") == 0)
    {
        // printf("[DEBUG] Bari -> 5\n");
        return 5;
    }
    else if (strcmp(citta, "Cagliari") == 0 || strcmp(citta, "cagliari") == 0)
    {
        // printf("[DEBUG] Cagliari -> 10\n");
        return 10;
    }
    else if (strcmp(citta, "Firenze") == 0 || strcmp(citta, "firenze") == 0)
    {
        // printf("[DEBUG] Firenze -> 15\n");
        return 15;
    }
    else if (strcmp(citta, "Genova") == 0 || strcmp(citta, "genova") == 0 )
    {
        // printf("[DEBUG] Genova -> 20\n");
        return 20;
    }
    else if (strcmp(citta, "Milano") == 0 || strcmp(citta, "milano") == 0 )
    {
        // printf("[DEBUG] Milano -> 25\n");
        return 25;
    }
    else if (strcmp(citta, "Napoli") == 0 || strcmp(citta, "napoli") == 0)
    {
        // printf("[DEBUG] Napoli -> 30\n");
        return 30;
    }
    else if (strcmp(citta, "Palermo") == 0 || strcmp(citta, "palermo") == 0 )
    {
        // printf("[DEBUG] Palermo -> 35\n");
        return 35;
    }
    else if (strcmp(citta, "Roma") == 0 || strcmp(citta, "roma") == 0 )
    {
        // printf("[DEBUG] Roma -> 40\n");
        return 40;
    }
    else if (strcmp(citta, "Torino") == 0 || strcmp(citta, "torino") == 0 )
    {
        // printf("[DEBUG] Torino -> 45\n");
        return 45;
    }
    else if (strcmp(citta, "Venezia") == 0 || strcmp(citta, "venezia") == 0 )
    {
        // printf("[DEBUG] Venezia -> 50\n");
        return 50;
    }
    else if (strcmp(citta, "Nazionale") == 0 || strcmp(citta, "nazionale") == 0 )
    {
        // printf("[DEBUG] Nazionale -> 55\n");
        return 55;
    }
    else if (strcmp(citta, "Tutte") == 0 || strcmp(citta, "tutte") == 0)
    {
        // printf("[DEBUG] Tutte le ruote -> 60\n");
        return 60;
    }
    else
    {
        // città non valida
        return 0;
    }
}

void stampa_schedina_su_file(FILE *registroLoggato)
{
	int i;

    // printf("[DEBUG] Stampo schedina su file\n");

    if(g.ruoteGiocate[0] == 60)
    {
        // l'utente ha giocato tutte le ruote
        for(i = 0; i < 11; i++){
            fprintf(registroLoggato,"%d ", (i*5) +5);
        }
    } else 
    {
        for (i = 0; i < 11; i++) 
        {
            if(g.ruoteGiocate[i] != 0)    
                fprintf(registroLoggato, "%d ", g.ruoteGiocate[i]);
            else 
                fprintf(registroLoggato, "%s ", "-");
        }
    }

    for (i = 0; i < 10; i++)
    {
        if(g.numeriGiocati[i] != 0)
            fprintf(registroLoggato, "%d ", g.numeriGiocati[i]);
        else
            fprintf(registroLoggato,"%s ", "-");
    }

    for ( i = 0; i < 5; i++)
    {   
        if(g.importiGiocati[i] != 0)
            fprintf(registroLoggato, "%f ", g.importiGiocati[i]);
        else
            fprintf(registroLoggato, "%s ", "-");
    }

    fprintf(registroLoggato,"t: %d",(int)time(NULL));

    fprintf(registroLoggato, "\n");
}

static inline void da_numero_a_nome_citta(int numero,char citta[])
{
    strcpy(citta,"");

    if (numero == 0)
    {
        strcpy(citta, "");
    }
    else if (numero == 5)
    {
        strcpy(citta, "Bari");
    }
    else if (numero == 10)
    {
        strcpy(citta, "Cagliari");
    }
    else if (numero == 15)
    {
        strcpy(citta, "Firenze");
    }
    else if (numero == 20)
    {
        strcpy(citta, "Genova");
    }
    else if (numero == 25)
    {
        strcpy(citta, "Milano");
    }
    else if (numero == 30)
    {
        strcpy(citta, "Napoli");
    }
    else if (numero == 35)
    {
        strcpy(citta, "Palermo");
    }
    else if (numero == 40)
    {
        strcpy(citta, "Roma");
    }
    else if (numero == 45)
    {
        strcpy(citta, "Torino");
    }
    else if (numero == 50)
    {
        strcpy(citta, "Venezia");
    }
    else if (numero == 55)
    {
        strcpy(citta, "Nazionale");
    }
    else if (numero == 60)
    {
        strcpy(citta, "tutte");
    }
    else
    {
        printf("[ERR] Errore in fase di decodifica del numero della citta\n");
    }

    // printf("[DEBUG] Restituisco la citta %s\n", citta);
}

struct estrazione fittizio[11];

int visualizza_schedine(int tipo)
{
    const int errore = -1;

    char daVisualizzare[MAX_LEN];
    char res[MAX_LEN], ritorno[MAX_LEN];
    char* line;
    line = (char *)malloc(sizeof(char) * 200);

    strcpy(res,"");
    strcpy(ritorno,"");

    // printf("[DEBUG] Utente loggato: %s\n", utenteLoggato);

    trovaBinarioUtente(utenteLoggato, daVisualizzare);

    // printf("[DEBUG] File utente loggato %s\n", daVisualizzare);

    FILE *toOpen = fopen(daVisualizzare, "rb");
    if (toOpen == NULL)
    {
        printf("[ERR] File non esistente\n");
        free(line);
        return errore;
    }

    int ultimaEstrazione = 0;

    // leggo timestamp ultima estrazione
    FILE* estrazioni = fopen("fileEstrazione.bin","rb");

    if(estrazioni == NULL)
    {
        // non è ancora avvenuta un'estrazione
        printf("[LOG] Nessuna estrazione avvenuta\n");
        if(tipo == 1) {
            // tutte le giocate sono attive
            ultimaEstrazione = 0;   
        } else {
            // non sono avvenute estrazioni, non ho giocate da mostrare
            fclose(toOpen);
            free(line);

            printf("[LOG] Fine giocate\n");
            char finegiocate[MAX_LEN];
            strcpy(finegiocate,"finegiocate");
            len = strlen(finegiocate) + 1;
            lmsg = htons(len);
            ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
            ret = send(new_sd, (void *)finegiocate, len, 0);

            return errore;            
        }
    } 
    else 
    {
        size_t letti_t = 0;
        ssize_t read = 0;
        
        read = getline(&line,&letti_t,estrazioni);
        int j = 0;

        char* controllo = strstr(line,"*****"); // delimitatore della riga
        while(read > 0){
            while(controllo == NULL){
                // leggo una schedina
                // printf("[DEBUG] Leggo letti line: %s\n",line);
                sscanf(line,"%d",&ultimaEstrazione);
                read = getline(&line,&letti_t,estrazioni);
                controllo = strstr(line,"*****");
                j++;
                continue;
            }
            read = getline(&line,&letti_t,estrazioni);
            controllo = strstr(line,"*****");
        }
    }

    // printf("[DEBUG] Ultimo timestamp estrazione: %d\n", ultimaEstrazione);

    if (tipo == 0)
    {
        // giocate relative a estrazioni già avvenute
        // printf("[DEBUG] Devo mostrare le schedine relative a estrazioni già avvenute\n");
        int i = 0;
        // estrazioni da effettuare
        char* str = fgets(res,MAX_LEN,toOpen);
        while(str != NULL){
            char tmp[MAX_LEN];
            strcpy(tmp,"");
            
            if(i == 0){
                // printf("[DEBUG] Nome utente e password: %s\n", res); 
                i++;
                str = fgets(res,MAX_LEN,toOpen);
                continue;
            }

            i++;
            // printf("[DEBUG] Schedina: %s\n",res);
            char* tok = strtok(str," ");

            if(tok != NULL){
                // printf("[DEBUG] Token %s\n", tok);
                int j = 0;

                while(strcmp(tok,"-") != 0 && j < 11){
                    // finchè non trovo un campo vuoto o ho finito le ruote
                    int codificaCitta = atoi(tok);
                    char citta[MAX_LEN];
                    strcpy(citta,"");
                    da_numero_a_nome_citta(codificaCitta,citta);
                    j++;
                    strcat(tmp,citta);
                    strcat(tmp," ");
                    tok = strtok(NULL," ");
                }

                // printf("[DEBUG] Finite le città\n");
                
                while(j < 11){
                    // se ho interrotto prima di arrivare alla fine delle ruote
                    tok = strtok(NULL," ");
                    j++;
                }

                strcat(tmp," * ");

                j = 0;
                while(strcmp(tok,"-") != 0 && j < 10){
                    // per i numeri giocati
                    strcat(tmp, tok);
                    strcat(tmp," ");
                    j++;
                    tok = strtok(NULL," ");
                }
                 
                // printf("[DEBUG] Finiti i numeri\n");

                while(j < 10){
                    
                    tok = strtok(NULL," ");
                    j++;
                }
                strcat(tmp," * ");

                j = 0;
                while(tok != NULL && j < 5){
                    // per gli importi
                    if(strcmp(tok,"-") == 0)
                        strcat(tmp,"0");
                    else
                        strcat(tmp, tok);

                    strcat(tmp," ");

                    if( (j == 0) && (strcmp(tok,"-") != 0) )
                        strcat(tmp,"estratto *");
                    else if((j == 1) && (strcmp(tok,"-") != 0) )
                        strcat(tmp,"ambo *");
                    else if((j == 2) && (strcmp(tok,"-") != 0))
                        strcat(tmp,"terno *");
                    else if((j == 3) && (strcmp(tok,"-") != 0))
                        strcat(tmp,"quaterna *");
                    else if((j == 4) && (strcmp(tok,"-") != 0))
                        strcat(tmp,"cinquina *");

                    j++;
                    tok = strtok(NULL," ");
                }
                // printf("[DEBUG] Finiti gli importi\n");
            }

            tok = strtok(NULL," ");
            int timestampSchedina = atoi(tok);
            // printf("[DEBUG] Timestamp della schedina: %d\n", timestampSchedina);

            if(timestampSchedina < ultimaEstrazione)
                strcpy(ritorno,tmp);
            else
                strcpy(ritorno,"");

            // printf("[DEBUG] Ritorno: %s", ritorno);

            strcat(ritorno,"\n");
            len = strlen(ritorno) + 1;
            lmsg = htons(len);
            ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
            ret = send(new_sd, (void *)ritorno, len, 0);

            str = fgets(res,MAX_LEN,toOpen);
        }
    } else if (tipo == 1) {
        // giocate relative a estrazioni che non sono ancora avvenute
        // printf("[DEBUG] Devo mostrare le schedine ancora attive\n");
        int i = 0;

        char* str = fgets(res,MAX_LEN,toOpen);
        while(str != NULL){
            char tmp[MAX_LEN];
            strcpy(tmp,"");

            if(i == 0){
                // printf("[DEBUG] Nome utente e password: %s\n", res); 
                i++;
                str = fgets(res,MAX_LEN,toOpen);
                continue;
            }

            i++;
            // printf("[DEBUG] Schedina: %s\n",res);
            char* tok = strtok(str," ");

            if(tok != NULL){
                // printf("[DEBUG] Token %s\n", tok);
                int j = 0;

                while(strcmp(tok,"-") != 0 && j < 11){
                    int codificaCitta = atoi(tok);
                    // printf("Codifica città: %d\n",codificaCitta);
                    char citta[MAX_LEN];
                    strcpy(citta,"");
                    da_numero_a_nome_citta(codificaCitta,citta);
                    // printf("Citta %s\n",citta);
                    j++;
                    strcat(tmp,citta);
                    strcat(tmp," ");
                    tok = strtok(NULL," ");
                }
                // printf("[LOG] Finite le città\n");
                
                while(j < 11){
                    tok = strtok(NULL," ");
                    j++;
                }

                strcat(tmp," * ");

                j = 0;
                while(strcmp(tok,"-") != 0 && j < 10){
                    // printf("Tok: %s\n",tok);
                    strcat(tmp, tok);
                    strcat(tmp," ");
                    j++;
                    tok = strtok(NULL," ");
                }
                 
                // printf("[LOG] Finiti i numeri\n");

                while(j < 10){
                    tok = strtok(NULL," ");
                    j++;
                }
                strcat(tmp," * ");

                j = 0;
                while(tok != NULL && j < 5){
                    if(strcmp(tok,"-") == 0)
                        strcat(tmp,"0");
                    else
                        strcat(tmp, tok);

                    strcat(tmp," ");
                    if( (j == 0) && (strcmp(tok,"-") != 0) )
                        strcat(tmp,"estratto *");
                    else if((j == 1) && (strcmp(tok,"-") != 0) )
                        strcat(tmp,"ambo *");
                    else if((j == 2) && (strcmp(tok,"-") != 0))
                        strcat(tmp,"terno *");
                    else if((j == 3) && (strcmp(tok,"-") != 0))
                        strcat(tmp,"quaterna *");
                    else if((j == 4) && (strcmp(tok,"-") != 0))
                        strcat(tmp,"cinquina *");

                    j++;
                    tok = strtok(NULL," ");
                }
                // printf("[LOG] Finiti gli importi\n");
            }

            tok = strtok(NULL," ");
            int timestampSchedina = atoi(tok);
            // printf("[DEBUG] Timestamp della schedina: %d\n", timestampSchedina);
            // printf("ritorno: %s\n tmp %s\n",ritorno,tmp);
            if(timestampSchedina > ultimaEstrazione){
                strcpy(ritorno,tmp);
                strcat(ritorno,"\n");
            } else
                strcpy(ritorno,"");

           
            len = strlen(ritorno) + 1;
            lmsg = htons(len);
            ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
            ret = send(new_sd, (void *)ritorno, len, 0);

            str = fgets(res,MAX_LEN,toOpen);
        }
    } else {
        printf("[ERR] Errore nel tipo di giocate da mostrare\n");

        if(estrazioni != NULL)
            fclose(estrazioni);

        fclose(toOpen);
        free(line);
    
        printf("[LOG] Fine giocate\n");
        char finegiocate[MAX_LEN];
        strcpy(finegiocate,"finegiocate");
        len = strlen(finegiocate) + 1;
        lmsg = htons(len);
        ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
        ret = send(new_sd, (void *)finegiocate, len, 0);

        return -1;
    }

    free(line);

    if(estrazioni != NULL)
        fclose(estrazioni);

    fclose(toOpen);

    printf("[LOG] Fine giocate\n");
    char finegiocate[MAX_LEN];
    strcpy(finegiocate,"finegiocate");
    len = strlen(finegiocate) + 1;
    lmsg = htons(len);
    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
    ret = send(new_sd, (void *)finegiocate, len, 0);

    // printf("[DEBUG] Ritorno %s, Giocate %s\n", ritorno, giocate);
    return 1;
}

// array di estrazioni, una per ogni ruota
struct estrazione estratti[11];

static inline void svuota_schedina(){
	int i, j;
    for(i = 0; i < 11; i++){
        estratti[i].ruota = 0;
        estratti[i].timestamp = 0;
        for(j = 0; j < 5; j++){
            estratti[i].numeri[j] = 0;
        }
    }
}

void salva_estrazione_su_file(){	
	int i ,j;
    FILE* fileEstrazioni = fopen("fileEstrazione.bin","ab+");
    
    for(i = 0; i < 11; i++){
        // stampo su file delle estrazioni il timestamp e il numero della ruota
        fprintf(fileEstrazioni,"%d %d ",estratti[0].timestamp, estratti[i].ruota);
        for(j = 0; j < 5; j++){
            // per ogni ruota stampo i numeri
            fprintf(fileEstrazioni,"%d ", estratti[i].numeri[j]);
        }

        fprintf(fileEstrazioni,"\n");
    }
    fprintf(fileEstrazioni,"********************\n"); // fine estrazione
    fclose(fileEstrazioni);
}

void estrai(){
	int i,j,k;
    srand(time(NULL));

    for(i = 0; i < 11; i++){
        estratti[i].timestamp = time(NULL);
        estratti[i].ruota = i;
        for(j = 0; j < 5; j++){
            estratti[i].numeri[j] = rand() % 90 + 1;
            for(k = 0; k < j; k++){
                // controllo duplicati su estrazione
                while(estratti[i].numeri[k] == estratti[i].numeri[j]){
                    estratti[i].numeri[j] = rand() % 90 + 1;
                }
            }
        }
    }   

    salva_estrazione_su_file();
}

struct estrazione letti[11]; // ultima estrazione letta
struct giocata* letta; 

// decodifica la schedina passata come parametro
int leggi_schedina(char* schedina){
    // printf("[DEBUG] Schedina da leggere: %s\n",schedina);

    char* tok = strtok(schedina," ");
    
    if(tok != NULL){
        // printf("[DEBUG] Token %s\n", tok);
        int j = 0;

        while(strcmp(tok,"-") != 0 && j < 11){
            int codificaCitta = atoi(tok);
            letta->ruoteGiocate[j] = codificaCitta;
            j++;
            tok = strtok(NULL," ");
        }
    
        while(j < 11){
            tok = strtok(NULL," ");
            letta->ruoteGiocate[j] = 0;
            j++;
        }

        j = 0;
        while(strcmp(tok,"-") != 0 && j < 10){
            letta->numeriGiocati[j] = atoi(tok);
            j++;
            tok = strtok(NULL," ");
        }
                 
        while(j < 10){
            letta->numeriGiocati[j] = 0;
            tok = strtok(NULL," ");
            j++;
        }
                
        j = 0;
        while(tok != NULL && j < 5){
            if(strcmp(tok,"-") == 0)
                letta->importiGiocati[j] = 0;
            else
                letta->importiGiocati[j] = atof(tok);

            j++;
            tok = strtok(NULL," ");
        }

        while(strcmp(tok,"t:") != 0)
            tok = strtok(NULL," ");
        
        tok = strtok(NULL, " ");

        return (time_t)atoi(tok);
              
    }

    return 0;
}

char *mesi[] = {"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno",
    "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"};

char* stampa_estrazione(){
    char *str_to_ret = malloc (sizeof (char) * MAX_LEN);
    strcpy(str_to_ret,"");

    strcat(str_to_ret,"Estrazione del ");
    time_t rawtime = letti[0].timestamp;
	struct tm* leggibile;
      
	leggibile = localtime(&rawtime);
    char data[MAX_LEN];
	sprintf(data,"%d %s %d ore %d:%d",leggibile->tm_mday,mesi[leggibile->tm_mon],leggibile->tm_year+1900, leggibile->tm_hour, leggibile->tm_min);
    strcat(str_to_ret,data);

    // printf("[DEBUG] Finita la stampa dell'estrazione: ret %s\n",str_to_ret);

    return str_to_ret;
}

void stampa_schedina_letta(){
	int i,h;	

    for(i = 0; i < 11; i++){
        int j = letta->ruoteGiocate[i];

        // ruota non giocata
        if(j == 0)
            continue;

        // printf("[DEBUG] Ruota giocata %d\n",j);

        for(h = 0; h < 10; h++){
            printf(" %d\n",letta->numeriGiocati[h]);
        }

        printf("\n");
    }
}

static inline int fattoriale(int n) {
  if (n < 0) 
    return -1; 

  if (n == 0) 
    return 1;
  else 
    return n*fattoriale(n-1);
}

static inline int combinazioni_semplici(int n, int k){
    int numeratore = fattoriale(n);
    int denominatore = fattoriale(n-k) * fattoriale(k);
    return numeratore/denominatore;
}

double importiVinti[5];

// aggiunge righe alla stringa composta dalle schedine vincenti
char* aggiungi_a_stringa(char* username,int timestampEstrazione){

    if(strcmp(username,utenteLoggato) != 0){
        printf("[ERR] Non è dell'utente loggato\n");
        return NULL;
    }

    char *str_to_ret = malloc (sizeof (char) * MAX_LEN);
    strcpy(str_to_ret,"");
    char path[MAX_LEN];
    strcpy(path,POSIZIONE_FILE_REGISTRO);
    strcat(path,"Vincite");
    strcat(path,username);
    strcat(path,".bin");

    // printf("[DEBUG] File: %s\n", path);
    FILE* importi = fopen(path,"ab+");
    fprintf(importi,"estratto: %f ambo: %f terno: %f quaterna: %f cinquina: %f t: %d\n",importiVinti[0] ,importiVinti[1],importiVinti[2],importiVinti[3],importiVinti[4],(int)time(NULL));
    char vincita[MAX_LEN];
    
    sprintf(vincita,"estratto: %f ambo: %f terno: %f quaterna: %f cinquina: %f \n",importiVinti[0],importiVinti[1],importiVinti[2],importiVinti[3],importiVinti[4]);
    strcat(str_to_ret,vincita);
    fprintf(importi,"%s\n","**************************");

    // printf("[DEBUG] Fine scrittura vincite\n");
    fclose(importi);
   
    return str_to_ret;
}    

void inizializza_importiVinti(){
	int i;
    for(i = 0; i < 5; i++)
        importiVinti[i] = 0;
}

struct consuntivo_vincite {
    double quantoVinto[5];
} cv;

void inizializza_consuntivo_vincite(){
    int i = 0;
    for(i = 0; i < 5; i++){
        cv.quantoVinto[i] = 0;
    }
}

// calcola le vincite dell'utente specificato
char* controlla_vincite(char* username,int timestampEstrazione){
	int i,k,h,l;
    double vincita = 0;
    char *str_to_ret = malloc (sizeof (char) * MAX_LEN);
    strcpy(str_to_ret,"");
    inizializza_importiVinti();

    for(i = 0; i < 11; i++){
        int numeriPresi = 0;
        int j = letta->ruoteGiocate[i];

        // ruota non giocata
        if(j == 0)
            continue;

        // printf("[DEBUG] Ruota giocata %d, indice di letti: %d\n",j, (j-5)/5);
        int numeriVincenti[5];
        memset(&numeriVincenti,0,sizeof(int)*5);
        int codificaRuota = 0;

        for(k = 0; k < 5; k++){
            for(h = 0; h < 10; h++){
                // indice dell'array letti (j-5)/5
                if( (letti[(j-5)/5].numeri[k] == letta->numeriGiocati[h]) && (letta->numeriGiocati[h] != 0) ){
                    // printf("[DEBUG] AZZECCATO!\n");
                    // printf("Il numero azzeccato è il %d\n",letta->numeriGiocati[h]);
                    numeriVincenti[k] = letta->numeriGiocati[h];
                    codificaRuota = j;
                    numeriPresi++;
                }
            }    
        }

        if(numeriPresi == 0){
            // printf("[DEBUG] Ho perso\n");
            continue;
        }

        int numeriGiocati = 0;
        for(l = 0;l < 10; l++){
            if(letta->numeriGiocati[l] != 0)
                numeriGiocati++;
        }

        int ruoteGiocate = 0;
        for(l = 0; l < 11; l++){
            if(letta->ruoteGiocate[l] != 0)
                ruoteGiocate++;
        }

        // printf("[DEBUG] Ho giocato %d numeri e %d ruote\n", numeriGiocati,ruoteGiocate);
        // so quanti ne ho presi sulla ruota, se ho giocato in quella modalità, ho vinto
        int index = (letta->importiGiocati[numeriPresi-1] != 0) ? numeriPresi-1 : -1;
        char tipoVincita[MAX_LEN];
        strcpy(tipoVincita,"");

        inizializza_importiVinti();

        switch(index){
           case -1:
                // printf("[DEBUG] Non hai vinto\n");
                strcpy(tipoVincita,"niente");
                break;
            case 0:
                // printf("[DEBUG] Estratto!\n");
                strcpy(tipoVincita,"estratto");
                vincita = (letta->importiGiocati[numeriPresi-1]*11.23)/(numeriGiocati*ruoteGiocate);
                importiVinti[0] = vincita;
                cv.quantoVinto[0] += vincita;
                // printf("[DEBUG] Ho vinto %f euro!\n",vincita);
                break;
            case 1:
                // printf("[DEBUG] Ambo!\n");
                strcpy(tipoVincita,"ambo");
                vincita = (letta->importiGiocati[numeriPresi-1]*250)/(ruoteGiocate*combinazioni_semplici(numeriGiocati,2));
                importiVinti[1] = vincita;
                cv.quantoVinto[1] += vincita;
                // printf("[DEBUG] Ho vinto %f euro!\n",vincita);
                break;
            case 2:
                // printf("[DEBUG] Terno!\n");
                strcpy(tipoVincita,"terno");
                vincita = (letta->importiGiocati[numeriPresi-1]*4500)/(ruoteGiocate*combinazioni_semplici(numeriGiocati,3));
                importiVinti[2] = vincita;
                cv.quantoVinto[2] += vincita;
                // printf("[DEBUG] Ho vinto %f euro!\n",vincita);
                break;
            case 3:
                // printf("[DEBUG] Quaterna!\n");
                strcpy(tipoVincita,"quaterna");
                vincita = (letta->importiGiocati[numeriPresi-1]*120000)/(ruoteGiocate*combinazioni_semplici(numeriGiocati,4));
                importiVinti[3] = vincita;
                cv.quantoVinto[3] += vincita;
                // printf("[DEBUG] Ho vinto %f euro!\n",vincita);
                break;
            case 4:
                // printf("[DEBUG] Cinquina!\n");
                strcpy(tipoVincita,"cinquina");
                vincita = (letta->importiGiocati[numeriPresi-1]*6000000)/(ruoteGiocate*combinazioni_semplici(numeriGiocati,5));
                importiVinti[4] = vincita;
                cv.quantoVinto[4] += vincita;
                // printf("[DEBUG] Ho vinto %f euro!\n",vincita);
                break;
            default:
                // printf("[DEBUG] Non hai vinto\n");
                strcpy(tipoVincita,"niente");
                break;
        }

        if(strcmp(tipoVincita,"niente") != 0){
            // printf("[LOG] Stampo vincite\n");
            char* singola = aggiungi_a_stringa(username,timestampEstrazione);
            
            if(singola == NULL) {
                singola = (char*)malloc(sizeof(char)* MAX_LEN);
                strcpy(singola,"nessuna vincita");
            }

            strcat(str_to_ret,singola);
            char numeri[MAX_LEN];
            char ruota[MAX_LEN];

            da_numero_a_nome_citta(codificaRuota,ruota);

            sprintf(numeri,"%d %d %d %d %d Ruota: ",numeriVincenti[0],numeriVincenti[1],numeriVincenti[2],numeriVincenti[3],numeriVincenti[4]);
            strcat(str_to_ret,"Numeri vincenti: ");
            strcat(str_to_ret,numeri);   
            strcat(str_to_ret,ruota);
            strcat(str_to_ret,"\n");
            free(singola);
        }
    }

    // printf("[DEBUG] Fine conteggio vincita\n");
    return str_to_ret;
}

double importiLetti[5];

void inizializza_importiLetti(){
	int i;
    for(i = 0; i < 5; i++)
        importiLetti[i] = 0;
}

/* SOLO PER DEBUG
char* leggi_vincite(char* username){
    inizializza_importiLetti();
    
    char path[MAX_LEN];
    strcpy(path,POSIZIONE_FILE_REGISTRO);
    strcat(path,username);
    strcat(path,"Vincite.bin");

    FILE* importi = fopen(path,"rb");
    size_t letti = fread(&importiLetti,sizeof(double),5,importi);
    fclose(importi);

    for(int i = 0; i < 5; i++){
        printf("Tipo giocata: %d, importo vinto: %f\n",i,importiLetti[i]);
    }

    return NULL;
}
*/

void stampa_letti()
{
    time_t rawtime = letti[0].timestamp;
	struct tm* leggibile;
      
	leggibile = localtime(&rawtime);
    char data[MAX_LEN];
	sprintf(data,"%d %s %d ore %d:%d",leggibile->tm_mday,mesi[leggibile->tm_mon],leggibile->tm_year+1900, leggibile->tm_hour, leggibile->tm_min);
    // printf("[DEBUG] Estrazione del %s\n", data);

    /*
    for(i = 0; i < 11; i++){
        printf("Ruota %d\n",letti[i].ruota);
        for(j = 0; j < 5; j++){
            printf("%d ", letti[i].numeri[j]);
        }
        printf("\n");
    }
    */

    printf("\n");

}

void aggiorna_vincite(){
    char *str_to_ret = malloc (sizeof (char) * MAX_LEN);
    char usr[MAX_LEN],pass[MAX_LEN];

    letta = (struct giocata*)malloc(sizeof(struct giocata));

    DIR* directoryGiocatori = opendir("RegistroGiocatori/");
    if(directoryGiocatori == NULL){
        printf("[ERR] Errore in apertura della cartella\n");
        exit(-1);
    }

    struct dirent* scorri;
    char controllo[MAX_LEN];
    
    while( (scorri = readdir(directoryGiocatori)) != NULL){
        // leggo tutti i file nella directory fino a che non trovo quello dell'utente loggato
        strcpy(controllo,utenteLoggato);
        strcat(controllo,".bin");
        if(strcmp(scorri->d_name,controllo) != 0){
            continue;
        }

        char* fileVincita = strstr(scorri->d_name,"Vincite");

        // salto i file relativi alle vincite 
        if( (strcmp(scorri->d_name,"..") == 0) || (strcmp(scorri->d_name,".") == 0) || (fileVincita != NULL) )
            continue;

        // printf("[DEBUG] File %s\n", scorri->d_name);

        char path[MAX_LEN];
        strcpy(path,POSIZIONE_FILE_REGISTRO);
        strcat(path,scorri->d_name);
        
        FILE* curr = fopen(path,"a+");
        
        if(curr == NULL){
            printf("[ERR] Errore in apertura del file %s\n",path);
            exit(-1);
        }

        // controllo le estrazioni
        char res[MAX_LEN];
        char* str = fgets(res,MAX_LEN,curr);
        int i = 0;

        while(str != NULL){
            if(i == 0){
                // printf("[DEBUG] Nome utente e password %s\n", str);
                sscanf(str,"%s %s",usr,pass);
                i++;
                str = fgets(res,MAX_LEN,curr);
                continue;
            }

            i++;
            // printf("[DEBUG] Schedina: %s\n",res);
           
            int timestamp = (int)leggi_schedina(res);
            // printf("[DEBUG] Timestamp: %d\n", timestamp);

            // stampa_schedina_letta();

            FILE* estrazioni = fopen("fileEstrazione.bin","rb+");
            if(estrazioni == NULL){
                printf("[ERR] Non sono ancora avvenute estrazioni, non ci sono vincite\n");
                return;
            } else {
                // printf("[DEBUG] File delle estrazioni aperto\n");
            }

            size_t letti_t = 0;
            ssize_t read = 0;
            char* line;
            line = (char *)malloc(sizeof(char) * 200);

            char* vincite;
            char* estrazione;
            strcpy(str_to_ret,"");
            read = getline(&line,&letti_t,estrazioni);
            
            // printf("[DEBUG] Linea letta\n");
            char* controllo = strstr(line,"*****");
            while(read > 0){
                int i = 0;
                while(controllo == NULL || i != 11){
                    // leggo una schedina
                    // printf("[DEBUG] Leggo letti line: %s\n",line);
                    sscanf(line,"%d %d %d %d %d %d %d",&letti[i].timestamp,&letti[i].ruota,&letti[i].numeri[0],&letti[i].numeri[1],&letti[i].numeri[2],&letti[i].numeri[3],&letti[i].numeri[4]);
                    read = getline(&line,&letti_t,estrazioni);
                    controllo = strstr(line,"*****");
                    i++;
                    continue;
                }

                // printf("[DEBUG] Stampo i letti\n");

                stampa_letti();

                // leggo tutte le estrazioni
                // printf("[DEBUG] letti[0].timestamp: %d - %d\n",letti[0].timestamp, timestamp);
                if(letti[0].timestamp < timestamp){
                    // sono in un'estrazione precedente
                    // printf("[DEBUG] Estrazione passata di %d\n", abs(timestamp - letti[0].timestamp) );
                    read = getline(&line,&letti_t,estrazioni);
                    continue;
                }    

                // trovato la prima estrazione utile e valida
                // printf("[DEBUG] Trovata estrazione valida per la giocata, estrazione passata da %d\n", abs(letti[0].timestamp - timestamp));
                estrazione = stampa_estrazione();
                strcat(str_to_ret,estrazione);
            
                vincite = controlla_vincite(usr,timestamp);

                if(strlen(vincite) == 0) {
                    strcat (str_to_ret,": nessuna vincita");
                    strcpy(vincite,"");
                }

                strcat(str_to_ret,"\n");
                strcat(str_to_ret,vincite);

                len = strlen(str_to_ret) + 1;
                lmsg = htons(len);
                ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                ret = send(new_sd, (void *)str_to_ret, len, 0);

                // printf("[DEBUG] Finito di controllare estrazione\n");
                break;
            }

            str = fgets(res,MAX_LEN,curr);
            /*
            if( str != NULL)
                printf("[DEBUG] Stringa letta %s\n", str);
            */
        }

        fclose(curr);
    }

    closedir(directoryGiocatori);
    return;
}

// ciclo infinito che attende per periodo ed esegue l'estrazione
void estrazione(){
    while(1){
        sleep(periodo);
        printf("[LOG] ESTRAZIONE!\n");
        svuota_schedina();
        estrai();
        // printf("[DEBUG] Estrazione finita\n");
    }
}

// conta le righe del file delle estrazioni
int contaRighe (FILE *fp)
{
    int i = 0;
    char conta;
    while (1)
    {
        fread ((void *)&conta,sizeof(char),1,fp);

        if (feof(fp))
            break;
        if (conta == '\n')
            i++;
    }
    // printf("[DEBUG] Contate le righe\n");
    return i;
}

char* leggi_estrazioni(char* quante,char* ruota){
    char *str_to_ret = malloc (sizeof (char) * MAX_LEN);
    strcpy(str_to_ret,"");
    int numeroRuota = 0;

    numeroRuota = da_nome_a_codifica_citta(ruota);
    // printf("[DEBUG] Ruota richiesta -> %d\n",numeroRuota);

    long int begin;
    long int end;
    long int len;

    int q = atoi(quante);
    int lette = 0;

    FILE* estrazioni = fopen("fileEstrazione.bin","r");
    if(estrazioni == NULL){
        printf("[ERR] Ancora non ho avuto estrazioni\n");
        free(str_to_ret);
        return NULL;
    }

    len = contaRighe(estrazioni);
   
    // totale delle estrazioni
    int totale = len/12;

    // ho richiesto più estrazioni di quelle che ci sono
    q = (q > totale) ? totale : q;

    // printf("[DEBUG] Calcolate estrazioni: %d\n",q);
    if(totale == 0){
        printf("[LOG] Non ci sono estrazioni\n");
        fclose(estrazioni);
        free(str_to_ret);
        return NULL;
    }

    // calcolo dimensione del file estrazioni
    fseek(estrazioni, 0, SEEK_SET);
    begin = ftell(estrazioni);
    fseek(estrazioni, 0, SEEK_END);
    end = ftell(estrazioni);

    int dim = end - begin;

    // dimensione singola estrazione
    int sizeestrazione = dim / totale;

    // offset per la lettura
    int off = q*sizeestrazione;

    // printf("[DEBUG] Righe %d - Estrazioni %d - Offset %d, dim: %d\n",len, totale,-off,dim);

    fseek(estrazioni,-off,SEEK_END);

    char linea[MAX_LEN];
    strcpy(linea,"");
    char* controllo;
    
    fscanf(estrazioni,"%s ",linea);
    controllo = strstr(linea,"***");
    // printf("[DEBUG] Linea %s\n",linea);
    
    // numero del token che sto leggendo
    int k = 0;
    int ruotaDaLeggere = 0;
    int ruotaAttuale = 0;

    while(lette < q){
        while(controllo == NULL){

            fscanf(estrazioni,"%s ",linea);
            k++;
            controllo = strstr(linea,"***");

            if(k == 1){
                // devo entrare solo se sto leggendo il numero della ruota
				int l;
                // numero della ruota
                ruotaAttuale = atoi(linea)*5+5;
                // printf("[DEBUG]  Città -> %d\n", ruotaAttuale);

                if((numeroRuota != 60) && (numeroRuota != 0) && (ruotaAttuale != numeroRuota)){
                   
                    // printf("[DEBUG] Non è la ruota che cerco\n");

                    for(l = 0; l < 5; l++) {
                        // leggo tutti i numeri della ruota che non mi interessano
                        fscanf(estrazioni,"%s ",linea);
                        controllo = strstr(linea,"***");
                        // printf("[DEBUG] Controllo: %s, Linea %s\n", controllo,linea);
                        k++;
                    }

                    ruotaDaLeggere = -1;
                } else
                {
                    ruotaDaLeggere = 0;
                }           
            }

            if(ruotaDaLeggere == 0){

                if(k == 1)
                    da_numero_a_nome_citta(ruotaAttuale,linea);

                strcat(str_to_ret,linea);
                strcat(str_to_ret," ");
                // printf("[DEBUG] Linea letta %s\n", linea);
                
            }

            if(k == 6){
                k = 0;          
                fscanf(estrazioni,"%s ",linea);
                controllo = strstr(linea,"***");
                if( (numeroRuota == 60) || (numeroRuota == 0) )
                    strcat(str_to_ret,"\n");
             }

        }

        fscanf(estrazioni,"%s ",linea);
        controllo = strstr(linea,"***");
        // printf("[DEBUG] Linea fine %d\n",linea); 

        // letta una estrazione la invio
        // printf("[DEBUG] Estrazione %s\n", str_to_ret);
        char invio[MAX_LEN];

        strcpy(invio,str_to_ret);
        len = strlen(invio) + 1;
        lmsg = htons(len);
        ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
        ret = send(new_sd, (void *)invio, len, 0);

      
        // printf("[DEBUG] Lette %d\n",lette);
        lette++;
        strcpy(str_to_ret,"");
        k = 0;
        ruotaDaLeggere = 0;
    }

    // printf("[DEBUG] Lista estrazioni %s\n", str_to_ret);

    strcpy(str_to_ret,"");
    k = 0;
    ruotaDaLeggere = 0;

    free(str_to_ret);
    fclose(estrazioni);
    return str_to_ret;
}

int main(int argc, char *argv[])
{
    uint16_t portaServer = atoi(argv[1]);
    if (argc - 1 != 2)
    {
        periodo = POLLING_TIME;
    }
    else
    {
        periodo = atoi(argv[2])*60;
    }

    pid_t figlio_estrazione = fork();
    if(figlio_estrazione == 0){
        // devo far partire l'estrazione
        estrazione();
    } 

    // Creazione socket 
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        perror("[ERR] Errore in fase di creazione del socket: \n");
        exit(-1);
    }
    else
    {
        printf("[LOG] Socket d'ascolto creato correttamente sd:%d\n", sd);
    }

    // Creazione indirizzo di bind 
    memset(&my_addr, 0, sizeof(my_addr)); // Pulizia
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(portaServer);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    // serve per riusare la porta utilizzata impostando la SO_REUSEADDR
    int yes = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
    {
        perror("setsockopt");
        exit(-1);
    }

    ret = bind(sd, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0)
    {
        perror("[ERR] Errore in fase di bind: \n");
        exit(-1);
    }
    else
    {
        printf("[LOG] Bind eseguita correttamente\n");
    }

    // crea la coda di ascolto
    ret = listen(sd, 10);
    if (ret < 0)
    {
        perror("[ERR] Errore in fase di bind: \n");
        exit(-1);
    }
    else
    {
        printf("[LOG] Listen eseguita correttamente\n");
    }

    while (1)
    {

        len = sizeof(cl_addr);

        // Accetto nuove connessioni
        printf("[LOG] Aspetto connessioni..\n");

        new_sd = accept(sd, (struct sockaddr *)&cl_addr, &len);
        if (new_sd < 0)
        {
            perror("[ERR] Errore in fase di accept: \n");
            exit(-1);
        }
        else
        {
            printf("[LOG] Connessione accettata new_sd: %d\n", new_sd);
        }

        // (controllo sull'indirizzo ip
        fileLog = fopen("registroLogin.txt", "a+");
        if (fileLog == NULL)
        {
            printf("[ERR] Il registro degli accessi non è stato aperto correttamente\n");
            exit(-1);
        }

        char *recordFileLogin;
        recordFileLogin = (char *)malloc(sizeof(char) * 100);

        ssize_t read = 0; //byte
        size_t lenght = 0;

        read = getline(&recordFileLogin, &lenght, fileLog);

        char ip[MAX_LEN];
        char timeStamp[MAX_LEN];
        char controlloIP[MAX_LEN];

        while (read > 0)
        {
            // sto controllando il file con i tentativi di login
            sscanf(recordFileLogin, "%s %s", ip, timeStamp);
            logInfo.addr_cl_temp = atoi(ip);
            logInfo.timestamp = atoi(timeStamp);

            if (logInfo.addr_cl_temp == cl_addr.sin_addr.s_addr)
            {
                // match dell'indirizzo ip
                if (time(NULL) - logInfo.timestamp < 30*60)
                {
                    // blocco la connessione perchè è passato troppo poco tempo
                    printf("[LOG] Ancora %d minuti da aspettare..\n", (int)(30*60 - (time(NULL) - logInfo.timestamp))/60);
                    // printf("[DEBUG] Connessione da bloccare!\n");
                    strcpy(controlloIP, "connessionebloccata");
                    break;
                }
                else
                {
                    strcpy(controlloIP, "connessioneok");
                }
            }
            read = getline(&recordFileLogin, &lenght, fileLog);
        }

        if (strcmp(controlloIP, "connessionebloccata") == 0)
        {
            close(new_sd);
            printf("[LOG] Connessione chiusa\n");
            fclose(fileLog);
            continue;
        }
        else
        {
            printf("[LOG] Connessione accettata\n");
            strcpy(controlloIP, "connessioneok");
        }

        // printf("[DEBUG] Controllo IP: %s\n", controlloIP);
        len = strlen(controlloIP) + 1;
        lmsg = htons(len);
        ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
        ret = send(new_sd, (void *)controlloIP, len, 0);

        free(recordFileLogin);
        fclose(fileLog);
        // fine controllo ip)

        pid = fork();

        if (pid == 0)
        {
            // Sono nel processo figlio
            int chiusura = close(sd);
            if (chiusura < 0)
            {
                perror("[ERR] Errore in chiusura: \n");
                exit(-1);
            }
            else
            {
                printf("[LOG] Socket d'ascolto %d chiuso\n", sd);
            }

            while (1)
            {

                char buffer[MAX_LEN];
                char cmd[MAX_LEN];
                char stringa1[MAX_LEN];
                char stringa2[MAX_LEN];
                strcpy(buffer, "");
                    
                int risultato = -1;

                // ( Attendo dimensione del comando
                printf("[LOG] Aspetto la dimensione del comando..\n");
                ssize_t ricevuti = recv(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                if (ricevuti < 0)
                {
                    perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
                    continue;
                }
                else if (ricevuti == 0)
                {
                    printf("[LOG] Connessione con l'host interrotta..\n");
                    break;
                }

                len = ntohs(lmsg); // Rinconverto in formato host
                if (len == 0)
                {
                    perror("[ERR] Lunghezza non valida: \n");
                    continue;
                }

                // ( Ricevo il comando
                printf("[LOG] Aspetto il comando..\n");
                ret = recv(new_sd, (void *)buffer, len, 0);

                if (ret < 0)
                {
                    perror("[ERR] Errore in fase di ricezione: \n");
                    continue;
                }
                else if (ret == 0)
                {
                    printf("[LOG] Connessione chiusa\n");
                    break;
                }

                // ricevuto il comando)

                // Controllo quale tipo di comando ho ricevuto
                sscanf((char *)buffer, "%s", cmd);
           
                // (validazione del session id
                char rispostaSessionId[10];
                if( (strcmp(cmd,"!login") != 0) && (strcmp(cmd,"!signup") != 0) )
                {
                    // aspetto di ricevere il session id
                    ssize_t ricevuti = recv(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    if (ricevuti < 0)
                    {
                        perror("[ERR] Errore in fase di ricezione(lunghezza): \n");
                        continue;
                    }

                    len = ntohs(lmsg); // Rinconverto in formato host
                    if (len == 0)
                    {
                        perror("[ERR] Lunghezza non valida: \n");
                        continue;
                    }
                    char receivedSessionId[LUNGHEZZA_SESSION_ID];
                    ret = recv(new_sd, (void *)receivedSessionId, len, 0);

                    if (ret < 0)
                    {
                        perror("[ERR] Errore in fase di ricezione: \n");
                        continue;
                    }

                    int ok = -1;
                    if(strcmp(receivedSessionId,savedSessionId) != 0){
                        printf("[LOG] Session ID non valido\n");
                        strcpy(rispostaSessionId,"nonvalido");
                    }else{
                        printf("[LOG] Session ID valido\n");
                        strcpy(rispostaSessionId,"valido");
                        ok = 1;
                    }

                    // invio la risposta
                    len = strlen(rispostaSessionId) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)rispostaSessionId, len, 0);

                    if( ok == -1 ){
                        continue;
                    }                    
                }
                // session id validato)

                // printf("[DEBUG] Comando: %s\n", cmd);

                if ((strlen(cmd) == 0) || (strcmp(cmd, " ") == 0))
                {
                    printf("[ERR] L'utente non ha inserito nessun comando\n");
                    strcpy(ris, "nessuncomando");
                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);
                    continue;
                }

                if (strcmp(cmd, "!signup") == 0)
                {
                    // !SIGNUP
                    sscanf(buffer, "%s %s %s", cmd, stringa1, stringa2);
                    printf("[LOG] !signup ricevuta\n");

                    if( (strcmp(stringa1," ") == 0)|| (strcmp(stringa2," ") == 0) || (strlen(stringa1) == 0) || (strlen(stringa2) == 0))
                    {
                        printf("[LOG] Non è stato inserito nessuno username e nessuna password");
                        strcpy(ris, "mancante");
                    }
                    else if (controllaUsernameRegistrazione(stringa1) == 0)
                    {
                        registra_giocatore(stringa1, stringa2);
                        printf("[LOG] Inviata risposta: libero\n");
                        strcpy(ris, "libero");
                    }
                    else
                    {
                        printf("[LOG] Inviata risposta: occupato\n");
                        strcpy(ris, "occupato");
                    }

                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);

                    continue;
                }
                else if (strcmp(cmd, "!login") == 0)
                {
                    // !LOGIN
                    strcpy(stringa1, "");
                    strcpy(stringa2, "");

                    printf("[LOG] !login ricevuto username %s password %s\n", stringa1, stringa2);

                    sscanf(buffer, "%s %s %s", cmd, stringa1, stringa2);
                    char ris[MAX_LEN];

                    if (strlen(stringa1) == 0 || strlen(stringa2) == 0 || strcmp(stringa1, " ") == 0 || strcmp(stringa2, " ") == 0)
                    {
                        strcpy(ris, "loginNo");
                        goto exitlogin;
                    }

                    int log = login(stringa1, stringa2);

                    if (log == 0)
                    {
                        // login effettuato con successo
                        printf("[LOG] Login andato a buon fine\n");
                        strcpy(ris, "loginOk");
                        tentativiRestanti = 3;
                    }
                    else if (log == -1)
                    {
                        // login fallito per un errore dell'utente
                        printf("[LOG] Login non andata a buon fine\n");
                        strcpy(ris, "loginNo");
                    }
                    else if (log == -2)
                    {
                        printf("[LOG] Tentativi esauriti, blocco il client\n");
                        strcpy(ris, "tentativiEsauriti");

                        fileLog = fopen("registroLogin.txt", "ab+");
                        if (fileLog == NULL)
                        {
                            printf("[ERR] Apertura del file non riuscita!\n");
                            exit(-1);
                        }

                        logInfo.timestamp = time(NULL);
                        // printf("[DEBUG] Indirizzo IP: %d, Timestamp: %d\n", cl_addr.sin_addr.s_addr, (int)logInfo.timestamp);
                        fprintf(fileLog, "%d %d\n", cl_addr.sin_addr.s_addr, (int)logInfo.timestamp);
                        fclose(fileLog);
                    }
                    else if (log == -3)
                    {
                        // login fallito perchè l'ip dell'utente è bloccato
                        printf("[LOG] Client bloccato, non può accedere ancora per %d\n", (int)( time(NULL) - logInfo.timestamp) );
                        strcpy(ris, "bloccato");
                    }
                    else
                    {
                        printf("[ERR] Qualcosa è andato storto..\n");
                    }

                exitlogin:
                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    // printf("[DEBUG] Risposta: %s\n", ris);

                    ret = send(new_sd, (void *)ris, len, 0);
 
       
                    if (strcmp(ris, "loginOk") == 0)
                    {
                        strcpy(buffer,"");
                        // se il login è andato a buon fine genero il sessionID
                        tentativiRestanti = 3;
                        char sessionID[LUNGHEZZA_SESSION_ID];
                        generaSessionID(sessionID);

                        strcpy(savedSessionId, sessionID);

                        // printf("[DEBUG] SessionID: %s\n", savedSessionId);
                        len = strlen(savedSessionId) + 1;
                        lmsg = htons(len);
                        ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                        ret = send(new_sd, (void *)savedSessionId, len, 0);
                    }

                    continue;
                }
                else if (strcmp(cmd, "!invia_giocata") == 0)
                {
                    printf("[LOG] !invia_giocata iniziata\n");
                    char *token;
                    token = strtok((char *)buffer, " ");
                    int i = 0;

                    inizializza_schedina();

                    if (token != NULL)
                    {
                        token = strtok(NULL, " ");
                        // printf("[DEBUG] Token ricevuto: %s\n", token);

                        // decodifica della schedina
                        if (strcmp(token, "-r") == 0)
                        {
                            // printf("[LOG] Le ruote giocate sono le seguenti..\n");
                            while (strcmp(token, "-n") != 0)
                            {
                                token = strtok(NULL, " ");
                                // codifica delle citta
                                if (strcmp(token, "-n") == 0)
                                    break;

                                if (i < 11)
                                    g.ruoteGiocate[i++] = da_nome_a_codifica_citta(token);
                                else
                                    printf("[LOG] Hai già inserito tutte le ruote\n");

                                if (g.ruoteGiocate[i - 1] == 0)
                                {
                                    printf("[ERR] Citta non riconosciuta\n");
                                    strcpy(ris, "formatoscorretto");
                                    goto fineinvio;
                                }
                                strcpy(ris, "invioOk");
                                // printf("[DEBUG] Ruota: %s, numero: %d\n", token, g.ruoteGiocate[i - 1]);
                            }
                        }
                        else
                        {
                            printf("[ERR] Formato citta sbagliato\n");
                            strcpy(ris, "formatoscorretto");
                            goto fineinvio;
                        }
                        int quantiGiocati = 0;
                        i = 0;
                        if (strcmp(token, "-n") == 0)
                        {
                            // printf("[LOG] I numeri giocati sono i seguenti..\n");
                            while (strcmp(token, "-i") != 0)
                            {
                                token = strtok(NULL, " ");
                                // codifica delle citta
                                if (strcmp(token, "-i") == 0)
                                    break;

                                int numero = atoi(token);
								int p = 0;

                                if ((numero <= 0) || (numero > 90))
                                {
                                    printf("[ERR] Numero non valido\n");
                                    strcpy(ris, "formatoscorretto");
                                    goto fineinvio;
                                }

                                // printf("[DEBUG] Numero: %d\n", numero);

                                for( p = 0; p < i; p++){
                                    if(numero == g.numeriGiocati[p]){
                                        printf("[ERR] Trovato un duplicato\n");
                                        strcpy(ris, "formatoscorretto");
                                        goto fineinvio;
                                    }
                                }

                                if (i < 10){
                                    g.numeriGiocati[i++] = numero;
                                    if(numero != 0)
                                        quantiGiocati++;
                                } else
                                    printf("[LOG] Hai già giocato 10 numeri\n");

                                strcpy(ris, "invioOk");
                            }
                        }
                        else
                        {
                            printf("[ERR] Formato numeri sbagliato\n");
                            strcpy(ris, "formatoscorretto");
                            goto fineinvio;
                        }

                        i = 0;
                        if (strcmp(token, "-i") == 0)
                        {
                            // printf("[LOG] Gli importi giocati sono i seguenti..\n");

                            while (token != NULL)
                            {
                                token = strtok(NULL, " ");
                                if (token == NULL)
                                {
                                    break;
                                }
                                double importo = atof(token);
                                // printf("[DEBUG] Importo: %f\n", importo);

                                if (i < 5)
                                    g.importiGiocati[i++] = importo;
                                else
                                    printf("[LOG] Hai già giocato il massimo numero di importi disponibili\n");

                                if ((g.importiGiocati[i - 1] < 0)) 
                                {
                                    printf("[ERR] Importo non valido\n");
                                    strcpy(ris, "formatoscorretto");
                                    goto fineinvio;
                                }

                                // printf("[DEBUG] Importo: %f, quantiGiocati %d, i-1: %d\n",g.importiGiocati[i-1], quantiGiocati,i);

                                if( (g.importiGiocati[i - 1] != 0) && (i > quantiGiocati) ){
                                    printf("[ERR] Giocata sbagliata!\n");
                                    strcpy(ris,"formatoscorretto");
                                    goto fineinvio;
                                }
                            }
                            strcpy(ris, "invioOk");
                        }
                        else
                        {
                            printf("[ERR] Formato importo sbagliato\n");
                            strcpy(ris, "formatoscorretto");
                            goto fineinvio;
                        }
                    }
                    else
                    {
                        printf("[ERR] !invia_giocata vuota\n");
                        strcpy(ris, "formatoscorretto");
                        goto fineinvio;
                    }

                    // printf("[DEBUG] Giocatore loggato: %s\n", utenteLoggato);

                    char log[MAX_LEN];
                    trovaBinarioUtente(utenteLoggato, log);
                    FILE *registroLoggato = fopen(log, "ab+");

                    // stampa schedina sul file del giocatore loggato
                    stampa_schedina_su_file(registroLoggato);

                    fclose(registroLoggato);

                    printf("[LOG] Registrazione giocata avvenuta con successo\n");

                fineinvio:
                    if (strcmp(ris, "formatoscorretto") != 0)
                    {
                        // printf("[DEBUG] Invio ok!\n");
                        strcpy(ris, "invioOk");
                    }
                    else
                    {
                        // printf("[DEBUG] Pulisco tutto perchè il formato è scorretto\n");
                        inizializza_schedina();
                    }

                    // printf("[DEBUG] Ris %s\n", ris);
                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);

                    continue;
                }
                else if (strcmp(cmd, "!vedi_giocate") == 0) {
                    strcpy(stringa1, "");

                    sscanf(buffer, "%s %s", cmd, stringa1);

                    int tipo = atoi(stringa1);

                    // printf("[DEBUG] Tipo di giocate da mostrare: %d\n", tipo);

                    if (tipo != 0 && tipo != 1)
                    {
                        // inserito un tipo non valido
                        printf("[LOG] Tipo inserito non valido\n");
                        strcpy(ris, "nonvisualizzate");
                    }
                    else
                    {
                        strcpy(ris,"inizioschedine");
                        len = strlen(ris) + 1;
                        lmsg = htons(len);
                        ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                        ret = send(new_sd, (void *)ris, len, 0);
                         
                        risultato = visualizza_schedine(tipo);  

                        if(risultato != -1){
                            printf("[LOG] Giocate stampate correttamente\n");
                        }                  
                        else 
                        {
                            printf("[ERR] Giocate non stampate\n");
                        }
                    }
                } else if (strcmp(cmd,"!esci") == 0) {
                    logOk = -1;
                    strcpy(utenteLoggato,"");
                    strcpy(savedSessionId,"");

                    printf("[LOG] Logout effettuato con successo\n");

                    strcpy(ris,"disconnesso");
                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);
                    
                } else if (strcmp(cmd,"!vedi_vincite") == 0) {

                    strcpy(ris,"iniziovincite");
                    inizializza_consuntivo_vincite();
                    
                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);

                    aggiorna_vincite();
        
                    strcpy(ris,"finevincite");

                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);

                    char consuntivo[MAX_LEN];
                    strcpy(consuntivo,"");
                    sprintf(consuntivo,"Vincite su ESTRATTO: %f \n Vincite su AMBO: %f \n Vincite su TERNO: %f \n Vincite su QUATERNA: %f \n Vince su CINQUINA: %f \n",cv.quantoVinto[0],cv.quantoVinto[1],cv.quantoVinto[2],cv.quantoVinto[3],cv.quantoVinto[4]);
                    printf("Consuntivo: %s\n", consuntivo);
                    len = strlen(consuntivo) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)consuntivo, len, 0);
                } else if(strcmp(cmd,"!vedi_estrazione") == 0){
                    strcpy(stringa1,"");
                    strcpy(stringa2,"");
                    sscanf(buffer, "%s %s %s", cmd, stringa1, stringa2);

                    strcpy(ris, "inizioestrazioni");
                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);

                    char* rit;

                    rit = leggi_estrazioni(stringa1,stringa2);

                    if(rit == NULL){
                        printf("[LOG] Nessuna estrazione da stampare\n");
                    }

                    strcpy(ris,"fineestrazioni");

                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);

                } else {
                    printf("[ERR] Comando non riconosciuto, inserisci un comando valido! \n");
                    strcpy(ris, "sbagliato");
                    len = strlen(ris) + 1;
                    lmsg = htons(len);
                    ret = send(new_sd, (void *)&lmsg, sizeof(uint16_t), 0);
                    ret = send(new_sd, (void *)ris, len, 0);

                    continue;
                }
                
            }
            printf("[LOG] Chiudo socket di connessione processo figlio\n");
            chiusura = close(new_sd);
            if (chiusura < 0)
            {
                perror("[ERR] Errore in chiusura: \n");
                exit(1);
            }
            else
            {
                printf("[LOG] Socket %d chiuso correttamente\n", sd);
            }

            exit(-1);
        }
        else
        {
            // Processo padre
            printf("[LOG] Chiudo socket di connessione processo padre\n");
            int chiusura = close(new_sd);
            if (chiusura < 0)
            {
                perror("[ERR] Errore in chiusura: \n");
                exit(-1);
            }
            else
            {
                printf("[LOG] Socket %d chiuso correttamente\n", sd);
            }
        }
    }
}
