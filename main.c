#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/stat.h>


#define MSG_SIZE 256
#define NUM_PROCESS 5
#define BUFFER_SIZE 8192
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
//Estructura del mensaje
struct message {
  long type;
  int mensaje[5];
  /*
  int filePosition;
  int Termino;
  int imprimir;
  int hijo;
  int TerminoImprimir;
  */
} msg;

int main(int argc, char *argv[]){
    pid_t fileSeekers[NUM_PROCESS];
    int status, actualPosition = 0;

    key_t msqkey = 771;
    msgctl(msqkey, IPC_RMID, NULL);
    sleep(1);
    int msqid = msgget(msqkey, IPC_CREAT | 0666);

    regex_t regex;
    const char* const re = "Capítulo";

    regmatch_t  pmatch[1];
    int hijo = 0;
    FILE* fp;
    fp = fopen("quijote.txt", "r");
    if(fp==NULL){ exit(1); }

    int hijos[NUM_PROCESS];


    msg.mensaje[1] = 0;
    if (regcomp(&regex, re, REG_NEWLINE))
        exit(EXIT_FAILURE);

    for (int i=1; i<NUM_PROCESS; i++) {
        if (fork() == 0){
            char buffer[BUFFER_SIZE];
            char *inicioLInea  = buffer;
            char *finalLiena = buffer;
            char *inicioArchivo = buffer;
            while(1){
                msgrcv(msqid, &msg, MSG_SIZE, i, 0);
                if(msg.mensaje[1] ==1){
                    fclose(fp);
                    exit(0);
                }
                //printf("Este hijo esta por la linea: %i \n", msg.mensaje[0]);
                fseek(fp, msg.mensaje[0], SEEK_SET);

                if(fread(buffer, sizeof(char), sizeof(buffer), fp) != BUFFER_SIZE){
                    msg.mensaje[1] =1;
                };
                
                actualPosition = msg.mensaje[0] + BUFFER_SIZE;
                

                int j = 0;
                while(buffer[BUFFER_SIZE+j]!='\n'){
                    buffer[BUFFER_SIZE+j] = 0;
                    j--;
                }
                
                fseek(fp, j, SEEK_CUR);
                
                actualPosition += j;      
                msg.mensaje[0] = actualPosition;
               // printf("Este hijo esta por la linea: %i \n", msg.mensaje[0]);
                msg.mensaje[2]=0;
                msg.mensaje[3]=0;
                msg.type = 100;
                msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje) , IPC_NOWAIT);
                
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
                    while( *(finalLiena + pmatch[0].rm_eo) != '\n'){
                        len++;
                        pmatch[0].rm_eo++;
                    }
                    msg.mensaje[2] = 1;
                    msg.mensaje[1] = 0;
                    msg.mensaje[3] = 0;
                    msg.mensaje[4] =i;
                    msg.type = 100;
                    msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje) , 0);
                    printf("%.*s\"\n", len, inicioLInea + pmatch[0].rm_so);
                    
                    inicioLInea += pmatch[0].rm_so + len;
                }
                
                msg.mensaje[2] = 0;
                msg.mensaje[1] = 0;
                msg.mensaje[3] = 1;
                msg.mensaje[4] =i;
                msg.type = 100;
                msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje) , IPC_NOWAIT);
            }
        }else{
            hijos[i-1]=0;
        }
    }
    
    int i = 1;
    msg.type = i;
    msg.mensaje[0] = actualPosition;
    msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje) , IPC_NOWAIT);
    int trabajando =1;
    int hijosTrabajando =1;

    while(trabajando ==1 || hijosTrabajando ==1){
        msgrcv(msqid, &msg, MSG_SIZE, 100, 0);
        if(msg.mensaje[1] ==1){
            trabajando=0;
            printf("Hijo termino \n");
        }
        else if(msg.mensaje[3] == 1){
            hijos[msg.mensaje[4] -1] = 0;
            hijosTrabajando =0;
            for (int j = 0; j < NUM_PROCESS; j++)
            {
                if(hijos[j] == 1){
                    hijosTrabajando=1;
                }
            }
        } if(msg.mensaje[2] ==1){
            
        }
        else{
            hijos[i-1]=1;
            i++;
            if(i == NUM_PROCESS){
                i=1;
            }
            msg.type = i;
            actualPosition = msg.mensaje[0];
            msg.mensaje[0] = actualPosition;
            msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje) , IPC_NOWAIT);
        }
       
    }
    printf("Se salio del cicloprincipal \n");
    fflush(stdout);
    for (int j=1; j<NUM_PROCESS; j++)
    {
        msg.type = j;
        msg.mensaje[1] =1;
        msgsnd(msqid, (void *)&msg, sizeof(msg.mensaje) , 0);
    }
    for (int j=1; j<NUM_PROCESS; j++)
    {
        wait(&status);    
    }
    
    fclose(fp);
    msgctl(msqkey, IPC_RMID, NULL);
    exit(0); 

}
