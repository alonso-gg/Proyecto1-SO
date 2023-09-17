#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>

#define MSG_SIZE 256
#define NUM_PROCESS 5
#define BUFFER_SIZE 8192
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

struct message {
    long type;
    int mensaje[5];
} msg;

// Definir el semáforo para coordinar printf
sem_t *printMutex;

void imprimir_mensaje(int numero_proceso, const char *mensaje) {
    // Esperar por el semáforo antes de imprimir
    sem_wait(printMutex);

    // Imprimir el mensaje
    printf("Proceso %d: %s\n", numero_proceso, mensaje);

    // Liberar el semáforo después de imprimir
    sem_post(printMutex);
}

int main(int argc, char *argv[]) {
    pid_t fileSeekers[NUM_PROCESS];
    int status, actualPosition = 0;

    key_t msqkey = 771;
    msgctl(msqkey, IPC_RMID, NULL);
    sleep(1);
    int msqid = msgget(msqkey, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    regex_t regex;
    const char *const re = "Capítulo";

    regmatch_t pmatch[1];

    FILE *fp;
    fp = fopen("quijote.txt", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int hijos[NUM_PROCESS];
    memset(hijos, 0, sizeof(hijos));

    // Agregar un semáforo para la cola de mensajes
    sem_t *mutex;
    mutex = sem_open("/my_semaphore", O_CREAT | O_EXCL, 0666, 1);
    if (mutex == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // Inicializar el semáforo para printf
    printMutex = sem_open("/print_semaphore", O_CREAT | O_EXCL, 0666, 1);
    if (printMutex == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

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
                // Utilizar el semáforo antes de acceder a la cola de mensajes
                sem_wait(mutex);
                msgrcv(msqid, &msg, MSG_SIZE, i, 0);
                sem_post(mutex);

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

                int j = 0;
                while (buffer[bytesRead + j] != '\n') {
                    buffer[bytesRead + j] = 0;
                    j--;
                }

                fseek(fp, j, SEEK_CUR);
                actualPosition += j;
                msg.mensaje[0] = actualPosition;

                msg.mensaje[2] = 0;
                msg.mensaje[3] = 0;
                msg.type = 100;

                // Utilizar el semáforo antes de enviar un mensaje
                sem_wait(mutex);
                msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje), IPC_NOWAIT);
                sem_post(mutex);

                regoff_t off, len;
                for (unsigned int k = 0;; k++) {
                    if (regexec(&regex, inicioLinea, ARRAY_SIZE(pmatch), pmatch, 0)) {
                        break;
                    }

                    len = pmatch[0].rm_eo - pmatch[0].rm_so;

                    finalLinea = inicioLinea;
                    while (*(inicioLinea + pmatch[0].rm_so) != '\n') {
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

                    // Utilizar el semáforo antes de enviar un mensaje
                    sem_wait(mutex);
                    msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje), 0);
                    sem_post(mutex);

                    // Imprimir un mensaje usando la función personalizada
                    imprimir_mensaje(i, inicioLinea + pmatch[0].rm_so);

                    inicioLinea += pmatch[0].rm_so + len;
                }

                msg.mensaje[2] = 0;
                msg.mensaje[1] = 0;
                msg.mensaje[3] = 1;
                msg.mensaje[4] = i;
                msg.type = 100;

                // Utilizar el semáforo antes de enviar un mensaje
                sem_wait(mutex);
                msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje), IPC_NOWAIT);
                sem_post(mutex);
            }
        } else {
            hijos[i - 1] = 0;
        }
    }

    int i = 1;
    msg.type = i;
    msg.mensaje[0] = actualPosition;

    // Utilizar el semáforo antes de enviar un mensaje
    sem_wait(mutex);
    msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje), IPC_NOWAIT);
    sem_post(mutex);

    int trabajando = 1;
    int hijosTrabajando = 1;

    while (trabajando == 1 || hijosTrabajando == 1) {
        msgrcv(msqid, &msg, MSG_SIZE, 100, 0);
        if (msg.mensaje[1] == 1) {
            trabajando = 0;
            printf("Hijo terminó\n");
        } else if (msg.mensaje[3] == 1) {
            hijos[msg.mensaje[4] - 1] = 0;
            hijosTrabajando = 0;
            for (int j = 0; j < NUM_PROCESS; j++) {
                if (hijos[j] == 1) {
                    hijosTrabajando = 1;
                }
            }
        }
        if (msg.mensaje[2] == 1) {
            // Handle msg.mensaje[2] == 1 case here
        } else {
            hijos[i - 1] = 1;
            i++;
            if (i == NUM_PROCESS) {
                i = 1;
            }
            msg.type = i;
            actualPosition = msg.mensaje[0];
            msg.mensaje[0] = actualPosition;

            // Utilizar el semáforo antes de enviar un mensaje
            sem_wait(mutex);
            msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje), IPC_NOWAIT);
            sem_post(mutex);
        }
    }

    printf("Se salió del ciclo principal\n");
    fflush(stdout);

    for (int j = 1; j < NUM_PROCESS; j++) {
        msg.type = j;
        msg.mensaje[1] = 1;

        // Utilizar el semáforo antes de enviar un mensaje
        sem_wait(mutex);
        msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje), 0);
        sem_post(mutex);
    }

    // Cerrar y eliminar el semáforo para la cola de mensajes al final
    sem_close(mutex);
    sem_unlink("/my_semaphore");

    // Cerrar y eliminar el semáforo de printf al final
    sem_close(printMutex);
    sem_unlink("/print_semaphore");

    fclose(fp);
    msgctl(msqkey, IPC_RMID, NULL);

    exit(0);
}
