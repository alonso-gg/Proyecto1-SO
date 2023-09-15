#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/stat.h>


#define MSG_SIZE 128
#define NUM_PROCESS 20
#define BUFFER_SIZE 8192
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
//Estructura del mensaje
struct message {
  long type;
  int filePosition;
  int Termino;
  int imprimir;
  int hijo;
} msg;

int main(int argc, char *argv[]){
    pid_t fileSeekers[NUM_PROCESS];
    int status, actualPosition = 0;

    key_t msqkey = 9992;
    int msqid = msgget(msqkey, IPC_CREAT | S_IRUSR | S_IWUSR);

    regex_t regex;
    const char* const re = "Mancha";

    regmatch_t  pmatch[1];
    int hijo = 0;
    FILE* fp;
    fp = fopen("quijote.txt", "r");
    if(fp==NULL){ exit(1); }

    msg.Termino = 0;
    if (regcomp(&regex, re, REG_NEWLINE))
        exit(EXIT_FAILURE);

    for (int i=1; i<=NUM_PROCESS; i++) {
        if (fork() == 0){
            while(1){
                msgrcv(msqid, &msg, MSG_SIZE, i, 0);
                if(msg.Termino ==1){
                    fclose(fp);
                    exit(0);
                }
                fseek(fp, msg.filePosition, SEEK_SET);

                char buffer[BUFFER_SIZE];
                char *inicioLInea  = buffer;
                char *finalLiena = buffer;
                char *inicioArchivo = buffer;

                fread(buffer, sizeof(char), sizeof(buffer), fp);
                
                actualPosition = msg.filePosition + BUFFER_SIZE;

                int j = 0;
                while(buffer[BUFFER_SIZE-1+j]!='\n'){
                    buffer[BUFFER_SIZE-1+j] = 0;
                    j--;
                }

                fseek(fp, j, SEEK_CUR);

                
                if(feof(fp) != 0 ){
                    msg.Termino =1;
                }
                else{
                    actualPosition += j;      
                    msg.filePosition = actualPosition;
                    msg.imprimir=0;
                    msg.type = 100;
                    msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition) + sizeof(msg.hijo) + sizeof(msg.imprimir) + sizeof(msg.Termino), IPC_NOWAIT);
                }

                //evaluar 
                regoff_t    off, len;
                for (unsigned int k = 0; ; k++) {
                    if (regexec(&regex, inicioLInea, ARRAY_SIZE(pmatch), pmatch, 0))
                        break;

                    len = pmatch[0].rm_eo - pmatch[0].rm_so;
                    
                     
                    finalLiena = inicioLInea;
                    while( *(inicioLInea + pmatch[0].rm_so) != '\n' ){
                        pmatch[0].rm_so--;
                        len++;
                    }
                    while( *(finalLiena + pmatch[0].rm_eo) != '\n' && finalLiena + pmatch[0].rm_eo != inicioArchivo + BUFFER_SIZE -1){
                        len++;
                        pmatch[0].rm_eo++;
                    }
                    msg.imprimir = 1;
                    msg.hijo =i;
                    msg.type = 100;
                    msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition) + sizeof(msg.hijo) + sizeof(msg.imprimir) + sizeof(msg.Termino), IPC_NOWAIT);
                    msgrcv(msqid, &msg, MSG_SIZE, i+NUM_PROCESS, 0);
                    printf("%.*s\"\n", len, inicioLInea + pmatch[0].rm_so);
                    msg.type =101;
                    msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition) + sizeof(msg.hijo) + sizeof(msg.imprimir) + sizeof(msg.Termino), IPC_NOWAIT);
                    inicioLInea += pmatch[0].rm_so + len;
                }
                if(feof(fp)==0){
                    msg.Termino =1;
                    msg.type = 100;
                    msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition) + sizeof(msg.hijo) + sizeof(msg.imprimir) + sizeof(msg.Termino), IPC_NOWAIT);
                }
            }
        }
    }
    
    int i = 1;
    msg.type = i;
    msg.filePosition = actualPosition;
    i++;
    msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition), IPC_NOWAIT);
    while(1){
       msgrcv(msqid, &msg, MSG_SIZE, 100, 0);
        if(msg.Termino ==1){
            for (int j=1; j<=NUM_PROCESS; j++)
            {
                msg.type = j;
                msg.Termino =1;
                msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition) + sizeof(msg.hijo) + sizeof(msg.imprimir) + sizeof(msg.Termino), IPC_NOWAIT);
            }
            fclose(fp);
            msgctl(msqkey, IPC_RMID, NULL);
            exit(0); 
        }
        else if(msg.imprimir ==1){
            msg.type = msg.hijo +NUM_PROCESS;
            msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition) + sizeof(msg.hijo) + sizeof(msg.imprimir) + sizeof(msg.Termino), IPC_NOWAIT);
            msgrcv(msqid, &msg, MSG_SIZE, 101, 0);
        }
        else{
            if(i == NUM_PROCESS){
                i=1;
            }
            msg.type = i;
            actualPosition = msg.filePosition;
            msg.filePosition = actualPosition;
            msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition) + sizeof(msg.hijo) + sizeof(msg.imprimir) + sizeof(msg.Termino), IPC_NOWAIT);
        }
       
    }

}
