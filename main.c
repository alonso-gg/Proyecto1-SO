#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#define MSG_SIZE 128
#define NUM_PROCESS 6
#define BUFFER_SIZE 8192
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

struct message {
    long type;
    int mensaje[5];
    //0. Posición de lectura | 1. Terminó el archivo | 2. Desuso | 3. Terminó de analizar | 4. i del hijo
    char contenido[MSG_SIZE]; // Agregamos contenido al mensaje
} msg;

int main(int argc, char *argv[]) {
    int status, actualPosition = 0;

    //Creación de la cola de msg
    key_t msqkey = getpid();
    int msqid = msgget(msqkey, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    //Abrir el archivo
    FILE *fp;
    fp = fopen(argv[2], "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    //Pool de hijos
    int hijos[NUM_PROCESS];
    memset(hijos, 0, sizeof(hijos));

    //Compilar la Regex
    regex_t regex;
    const char *const re = argv[1];

    regmatch_t pmatch[1];

    if (regcomp(&regex, re, REG_NEWLINE|REG_EXTENDED)) {
        perror("regcomp");
        exit(EXIT_FAILURE);
    }

    //Crear hijos
    for (int i = 1; i <= NUM_PROCESS; i++) {
        if (fork() == 0) {
            //Hijo pintor
            if(i == NUM_PROCESS){
                struct msqid_ds info;
                int bandera = 1;

                while (bandera == 1){
                    //Recibe mensajes de su tipo exclusivo
                    if (msgrcv(msqid, &msg, sizeof(msg), 440, IPC_NOWAIT) != -1){
                        //Esto ocurre si el padre lo manda a terminar
                        if (msg.mensaje[1] == 1) {
                            bandera = 0;
                        }
                        printf("%s\n", msg.contenido);
                        msgctl(msqid, IPC_STAT, &info);
                    }
                }

                fclose(fp);
                exit(0);
            }

            //Hijos lectores
            char buffer[BUFFER_SIZE];
            char *inicioLinea = buffer;
            char *finalLinea = buffer;
            char *inicioArchivo = buffer;
            while (1) {
                //Espera un mensaje para leer
                msgrcv(msqid, &msg, sizeof(msg), i, 0);

                //Si ya se terminó de leer todo el archivo, el hijo se sale
                if (msg.mensaje[1] == 1) {
                    fclose(fp);
                    exit(0);
                }

                //Lectura del archivo
                fseek(fp, msg.mensaje[0], SEEK_SET);
                size_t bytesRead = fread(buffer, sizeof(char), sizeof(buffer), fp);

                //Si leyó menos de 8K es porque llegó al fin de línea
                if (bytesRead != sizeof(buffer)) {
                    msg.mensaje[1] = 1;
                }

                //Suma lo que leyó a la posición anterior
                actualPosition = msg.mensaje[0] + bytesRead;

                //Recorta el buffer hasta la última línea completa
                int j = 0;
                while (buffer[bytesRead + j] != '\n') {
                    buffer[bytesRead + j] = 0;
                    j--;
                }

                fseek(fp, j, SEEK_CUR);
                actualPosition += j+1;

                //Envía el mensaje al padre con la nueva posición
                msg.mensaje[0] = actualPosition;
                msg.mensaje[2] = 0;
                msg.mensaje[3] = 0;
                msg.type = 100;

                msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);

                //Analiza la regex
                regoff_t len;
                char* tokens = strtok(buffer, "\n");

                while (tokens != NULL) {
                    //Si hay una coincidencia, envía el mensaje al pintor
                    if (regexec(&regex, tokens, 0, NULL, 0)==0) {
                        msg.mensaje[2] = 0;
                        msg.mensaje[1] = 0;
                        msg.mensaje[3] = 0;
                        msg.mensaje[4] = i;
                        msg.type = 440;

                        //Copiamos el contenido al mensaje para que el pintor lo imprima
                        memset(msg.contenido, 0, sizeof(msg.contenido));
                        strcpy(msg.contenido, tokens);

                        msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
                    }

                    tokens = strtok(NULL, "\n");
                }

                //Envía un mensaje al padre avisando que terminó de analizar
                msg.mensaje[2] = 0;
                msg.mensaje[1] = 0;
                msg.mensaje[3] = 1;
                msg.mensaje[4] = i;
                msg.type = 100;

                msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), 0);

                //El hijo queda listo para volver a leer
            }
        }
    }
    
    //El padre
    int i = 1;

    //Envía al hijo un mensaje para que lea desde la posición 0
    msg.type = i;
    msg.mensaje[0] = actualPosition;
    msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);

    int trabajando = 1; //El padre aún debe coordinar la lectura
    int hijosTrabajando = 1; //Algún hijo aún está analizando

    while (trabajando == 1 || hijosTrabajando == 1) {
        //Recibe mensajes de los hijos
        msgrcv(msqid, &msg, sizeof(msg), 100, 0);

        if (msg.mensaje[1] == 1) { //El archivo se terminó de leer
            trabajando = 0;
        } else if (msg.mensaje[3] == 1) { //Un hijo terminó de analizar
            hijos[msg.mensaje[4] - 1] = 0;
            hijosTrabajando = 0;
            for (int j = 0; j < NUM_PROCESS-1; j++) { //Revisa si otro aún está trabajando
                if (hijos[j] == 1) {
                    hijosTrabajando = 1;
                }
            }
        } else { //Manda al siguiente hijo a leer
            hijos[i - 1] = 1;
            i++;
            if (i == NUM_PROCESS) {
                i = 1;
            }
            msg.type = i;
            actualPosition = msg.mensaje[0];
            msg.mensaje[0] = actualPosition;
            msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
        }
    }

    //Notifica a los hijos lectores que ya pueden terminar (exit/morir)
    for (int j = 1; j < NUM_PROCESS; j++) {
        msg.type = j;
        msg.mensaje[1] = 1;
        msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), 0);
    }

    //Espera a que todos los hijos terminen
    for (int j = 1; j < NUM_PROCESS; j++) {
        wait(&status);
    }

    //Notifica al hijo pintor que ya puede terminar (exit/morir)
    msg.type = 440;
    msg.mensaje[1] = 1;
    msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), 0);

    //Espera a que el pintor termine
    wait(&status);

    //El padre termina
    fclose(fp);
    msgctl(msqkey, IPC_RMID, NULL);
    exit(0);
}
