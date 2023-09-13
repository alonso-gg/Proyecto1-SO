#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/stat.h>


#define MSG_SIZE 128
#define NUM_PROCESS 5
#define BUFFER_SIZE 8192

//Estructura del mensaje
struct message {
  long type;
  long filePosition;
  char content[MSG_SIZE];
} msg;

int main(int argc, char *argv[]){
    pid_t fileSeekers[NUM_PROCESS];
    int status, actualPosition = 0;

    key_t msqkey = 999;
    int msqid = msgget(msqkey, IPC_CREAT | S_IRUSR | S_IWUSR);

    regex_t regex;
    const char* const re = "John.*o";

    FILE* fp;
    fp = fopen("quijote.txt", "r");
    if(fp==NULL){
        printf("ERROR");
        exit(1);
    }else{
        printf("gud");
    }

    if (regcomp(&regex, re, REG_NEWLINE))
        exit(EXIT_FAILURE);
    else
        printf("Regex compilada");

    for (int i=1; i<=NUM_PROCESS; i++) {
        if (fork() == 0){
            while(1){
                msgrcv(msqid, &msg, MSG_SIZE, i, 0);

                fseek(fp, msg.filePosition-actualPosition, SEEK_CUR);

                char buffer[100];

                fread(buffer, sizeof(char), 100, fp);

                actualPosition += 100;

                int j = 0;
                while(buffer[99+j]!='\n'){
                    buffer[99+j] = 0;
                    j--;
                }

                actualPosition += j;      
                printf("proc: %d, pos: %d", i, actualPosition);         

                msg.filePosition = actualPosition;
                msg.type = 100;
                msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition), IPC_NOWAIT);

                exit(0); 
            }
        }
    }

    while(1){
        for (int i=1; i<=NUM_PROCESS; i++){
            msg.type = i;
            msg.filePosition = actualPosition;
            msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition), IPC_NOWAIT);

            msgrcv(msqid, &msg, MSG_SIZE, 100, 0);
        }
        exit(0); 
    }

    wait(&status);
    fclose(fp);
    exit(0); 
}
