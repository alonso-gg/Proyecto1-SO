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

    if (regcomp(&regex, re, REG_NEWLINE)) {
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
 
                while (bandera == 1 || info.msg_qnum == 0){
                    msgrcv(msqid, &msg, sizeof(msg), 440, 0);

                    if (msg.mensaje[1] == 1) {
                        bandera = 0;
                    }

                    printf("%s\n", msg.contenido);
                    msgctl(msqid,IPC_STAT,&info);
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
                msgrcv(msqid, &msg, sizeof(msg), i, 0);

                if (msg.mensaje[1] == 1) {
                    fclose(fp);
                    exit(0);
                }

                //Lectura del archivo
                fseek(fp, msg.mensaje[0], SEEK_SET);
                size_t bytesRead = fread(buffer, sizeof(char), sizeof(buffer), fp);

                if (bytesRead != sizeof(buffer)) {
                    msg.mensaje[1] = 1;
                }

                actualPosition = msg.mensaje[0] + bytesRead;

                int j = 0;
                while (buffer[bytesRead + j] != '\n') {
                    buffer[bytesRead + j] = 0;
                    j--;
                }

                fseek(fp, j, SEEK_CUR);
                actualPosition += j+1;
                msg.mensaje[0] = actualPosition;
                msg.mensaje[2] = 0;
                msg.mensaje[3] = 0;
                msg.type = 100;

                msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);

                //Analizar la regex
                regoff_t len;
                for (unsigned int k = 0;; k++) {
                    if (regexec(&regex, inicioLinea, ARRAY_SIZE(pmatch), pmatch, 0)) {
                        break;
                    }

                    len = pmatch[0].rm_eo - pmatch[0].rm_so;

                    finalLinea = inicioLinea;
                    while (*(inicioLinea + pmatch[0].rm_so) != '\n' || inicioArchivo != inicioArchivo) {
                        pmatch[0].rm_so--;
                        len++;
                    }
                    while (*(finalLinea + pmatch[0].rm_eo) != '\n') {
                        len++;
                        pmatch[0].rm_eo++;
                    }

                    msg.mensaje[2] = 0;
                    msg.mensaje[1] = 0;
                    msg.mensaje[3] = 0;
                    msg.mensaje[4] = i;
                    msg.type = 440;

                    // Copiamos el contenido al mensaje para que el padre lo imprima
                    memset(msg.contenido, 0, sizeof(msg.contenido));

                    strncpy(msg.contenido, inicioLinea + pmatch[0].rm_so, len);

                    msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);

                    inicioLinea += pmatch[0].rm_so + len;
                }

                //Hijo listo para volver a leer
                msg.mensaje[2] = 0;
                msg.mensaje[1] = 0;
                msg.mensaje[3] = 1;
                msg.mensaje[4] = i;
                msg.type = 100;

                msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), 0);
            }
        }
    }
    
    //El padre
    int i = 1;
    msg.type = i;
    msg.mensaje[0] = actualPosition;
    msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
    int trabajando = 1;
    int hijosTrabajando = 1;

    while (trabajando == 1 || hijosTrabajando == 1) {
        msgrcv(msqid, &msg, sizeof(msg), 100, 0);

        if (msg.mensaje[1] == 1 && trabajando==1) { //El archivo se terminó de leer
            trabajando = 0;
        } else if (msg.mensaje[3] == 1) { //Un hijo terminó de analizar
            hijos[msg.mensaje[4] - 1] = 0;
            hijosTrabajando = 0;
            for (int j = 0; j < NUM_PROCESS-1; j++) { //Revisa si otro aún está trabajando
                if (hijos[j] == 1) {
                    hijosTrabajando = 1;
                }
            }
        } else { //Coordina la lectura del archivo
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

    //Notifica al hijo pintor que ya puede terminar (exit/morir)
    msg.type = 440;
    msg.mensaje[1] = 1;
    msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), 0);

    //Espera a que todos los hijos terminen
    for (int j = 1; j <= NUM_PROCESS; j++) {
        wait(&status);
    }

    //El padre termina
    fclose(fp);
    msgctl(msqkey, IPC_RMID, NULL);
    exit(0);
}
