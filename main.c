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
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
//Estructura del mensaje
struct message {
  long type;
  int filePosition;
  int Termino;
  char content[MSG_SIZE];
} msg;

int main(int argc, char *argv[]){
    pid_t fileSeekers[NUM_PROCESS];
    int status, actualPosition = 0;

    key_t msqkey = 999;
    int msqid = msgget(msqkey, IPC_CREAT | S_IRUSR | S_IWUSR);

    regex_t regex;
    const char* const re = "Mancha";

    regmatch_t  pmatch[1];

    FILE* fp;
    fp = fopen("quijote.txt", "r");
    if(fp==NULL){ exit(1); }

    char*inicioLinea;

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
                if(feof(fp) != 0 ){
                    msg.Termino =1;
                }
                actualPosition = msg.filePosition + BUFFER_SIZE;

                int j = 0;
                while(buffer[BUFFER_SIZE-1+j]!='\n'){
                    buffer[BUFFER_SIZE-1+j] = 0;
                    j--;
                }

                fseek(fp, j, SEEK_CUR);

                actualPosition += j;      
                msg.filePosition = actualPosition;
                msg.type = 100;
                msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition), IPC_NOWAIT);


                //evaluar 
                regoff_t    off, len;
                for (unsigned int i = 0; ; i++) {
                    if (regexec(&regex, inicioLInea, ARRAY_SIZE(pmatch), pmatch, 0))
                        break;

                    len = pmatch[0].rm_eo - pmatch[0].rm_so;
                    
                     
                    finalLiena = inicioLInea;
                    while( *(inicioLInea + pmatch[0].rm_so) != '\n' && inicioLInea + pmatch[0].rm_so !=  inicioArchivo){
                        pmatch[0].rm_so--;
                        len++;
                    }
                    while( *(finalLiena + pmatch[0].rm_eo) != '\n' && finalLiena + pmatch[0].rm_eo != inicioArchivo + BUFFER_SIZE -1){
                        len++;
                        pmatch[0].rm_eo++;
                    }


                    printf("%.*s\"\n", len, inicioLInea + pmatch[0].rm_so);

                    inicioLInea += pmatch[0].rm_so + len;
                }
            }
            exit(0); 
        }
    }
    
    while(1){
        for (int i=1; i<=NUM_PROCESS; i++){
            msg.type = i;
            msg.filePosition = actualPosition;
            msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition), IPC_NOWAIT);

            msgrcv(msqid, &msg, MSG_SIZE, 100, 0);
            if(msg.Termino ==1){
                for (int j=1; j<=NUM_PROCESS; j++)
                {
                    msg.Termino =1;
                    msgsnd(msqid, (void *)&msg, sizeof(msg.filePosition), IPC_NOWAIT);
                }
                fclose(fp);
                exit(0); 
                
            }
            printf("%i",msg.filePosition);
            actualPosition = msg.filePosition;
        }
    }

    //wait(&status);

}
