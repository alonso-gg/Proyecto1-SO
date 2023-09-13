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
  int filePosition;
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
    if(fp==NULL){ exit(1); }

    if (regcomp(&regex, re, REG_NEWLINE))
        exit(EXIT_FAILURE);

    for (int i=1; i<=NUM_PROCESS; i++) {
        if (fork() == 0){
            for(int z=0; z<2; z++){
                msgrcv(msqid, &msg, MSG_SIZE, i, 0);

                fseek(fp, msg.filePosition, SEEK_SET);

                char buffer[BUFFER_SIZE];

                fread(buffer, sizeof(char), sizeof(buffer), fp);

                actualPosition = msg.filePosition + BUFFER_SIZE;

                int j = 0;
                while(buffer[BUFFER_SIZE-1+j]!='\n'){
                    buffer[BUFFER_SIZE-1+j] = 0;
                    j--;
                }

                fseek(fp, j, SEEK_CUR);

                actualPosition += j;      
                printf("%d : %s\n", i, buffer);        

                msg.filePosition = actualPosition;
                msg.type = 100;
                msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition), IPC_NOWAIT);
            }
            exit(0); 
        }
    }

    for(int z=0; z<2; z++){
        for (int i=1; i<=NUM_PROCESS; i++){
            msg.type = i;
            msg.filePosition = actualPosition;
            msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition), IPC_NOWAIT);

            msgrcv(msqid, &msg, MSG_SIZE, 100, 0);
            printf("padre %d", actualPosition);
            actualPosition = msg.filePosition;
        }
    }

    //wait(&status);
    fclose(fp);
    exit(0); 
}
