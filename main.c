#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <string.h>

#define MSG_SIZE 128
#define NUM_PROCESS 100
#define BUFFER_SIZE 8192
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

struct message {
    long type;
    int mensaje[5];
    char contenido[MSG_SIZE]; // Agregamos contenido al mensaje
} msg;

int main(int argc, char *argv[]) {
    int status, actualPosition = 0;

    key_t msqkey = 7887;
    int msqid = msgget(msqkey, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    regex_t regex;
    const char *const re = argv[1];

    regmatch_t pmatch[1];

    FILE *fp;
    fp = fopen(argv[2], "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int hijos[NUM_PROCESS];
    memset(hijos, 0, sizeof(hijos));

    if (regcomp(&regex, re, REG_NEWLINE)) {
        perror("regcomp");
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < NUM_PROCESS; i++) {
        if (fork() == 0) {
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

                fseek(fp, msg.mensaje[0], SEEK_SET);
                size_t bytesRead = fread(buffer, sizeof(char), sizeof(buffer), fp);

                if (bytesRead != sizeof(buffer)) {
                    msg.mensaje[1] = 1;
                }

                actualPosition = msg.mensaje[0] + bytesRead;
                //printf("%i \n",actualPosition);
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

                msgsnd(msqid, (void *)&msg, sizeof(msg), IPC_NOWAIT);

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
                    msg.mensaje[2] = 1;
                    msg.mensaje[1] = 0;
                    msg.mensaje[3] = 0;
                    msg.mensaje[4] = i;
                    msg.type = 100;

                    // Copiamos el contenido al mensaje para que el padre lo imprima
                    memset(msg.contenido, 0, sizeof(msg.contenido));

                    strncpy(msg.contenido, inicioLinea + pmatch[0].rm_so, len);

                    msgsnd(msqid, (void *)&msg, sizeof(msg), 0);

                    inicioLinea += pmatch[0].rm_so + len;
                }

                msg.mensaje[2] = 0;
                msg.mensaje[1] = 0;
                msg.mensaje[3] = 1;
                msg.mensaje[4] = i;
                msg.type = 100;

                msgsnd(msqid, (void *)&msg, sizeof(msg), 0);
            }
        }
    }

    struct msqid_ds info;
    int i = 1;
    msg.type = i;
    msg.mensaje[0] = actualPosition;
    msgsnd(msqid, (void *)&msg, sizeof(msg), IPC_NOWAIT);
    int trabajando = 1;
    int hijosTrabajando = 1;

    while (trabajando == 1 || hijosTrabajando == 1 || info.msg_qnum == 0) {
        msgrcv(msqid, &msg, sizeof(msg), 100, 0);
        
        if (msg.mensaje[1] == 1 && trabajando==1) {
            trabajando = 0;
            printf("Hijo termino, quedan %ld mensajes\n", info.msg_qnum);
            
        } else if (msg.mensaje[3] == 1) {
            hijos[msg.mensaje[4] - 1] = 0;
            hijosTrabajando = 0;
            for (int j = 0; j < NUM_PROCESS; j++) {
                if (hijos[j] == 1) {
                    hijosTrabajando = 1;
                }
            }
        }else if (msg.mensaje[2] == 1) {
            printf("%s\n", msg.contenido);
            fflush(stdout);
            //sleep(1);
        } else {
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
        msgctl(msqid,IPC_STAT,&info);
    }

    printf("Se salió del ciclo principal\n");

    for (int j = 1; j < NUM_PROCESS; j++) {
        msg.type = j;
        msg.mensaje[1] = 1;
        msgsnd(msqid, (void *)&msg, sizeof(msg) - sizeof(long), 0);
    }
    for (int j = 1; j < NUM_PROCESS; j++) {
        wait(&status);
    }

    fclose(fp);
    msgctl(msqkey, IPC_RMID, NULL);

    exit(0);
}
