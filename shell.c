#include <stdio.h>
#include <stdlib.h>
#include "simplefs.h"

#define MAX_COMMAND_SIZE 1024
#define MAX_NUM_TOK  64
#define QUIT 0
#define AGAIN 1

int shell_init(SimpleFS* fs){
    // chiedo il nome del file che memorizza il disco
    char* nameFile=(char*)malloc(sizeof(char)*FILENAME_MAX_LENGTH);
    memset(nameFile, 0,FILENAME_MAX_LENGTH);
    //printf("inserisci il nome del file su disco -> ");
    //CHECK_ERR(fgets(nameFile,FILENAME_MAX_LENGTH,stdin)==NULL,"can't read input filename");
    nameFile[0]='d';
    nameFile[1]='i';
    nameFile[2]='s';
    nameFile[3]='k';
    nameFile[4]='\n';
    nameFile[strlen(nameFile)-1]='\0';
    fs->filename = nameFile;

    int res;
    char format;
    fs->disk = (DiskDriver*)malloc(sizeof(DiskDriver));
    memset(fs->disk, 0, sizeof(DiskDriver));
    // carico il disco
    res=DiskDriver_load(fs->disk,fs->filename);

    // se non riesco a caricarlo significa che non esiste un disco con quel nome
    // allora chiedo se vuole crearlo
    if(res==FAILED){
        printf("file non presente o filesystem non riconosiuto, formattare il file? (s,n)-> ");
        scanf("%c", &format);
        if(format!='s') return FAILED;

        printf("inserisci il numero di blocchi del \"disco\" -> ");
        scanf("%d",&(fs->block_num));
        // to get '/n' left in stdin
        getchar();
        // format the disk
        SimpleFS_format(fs);
    }
    return SUCCESS;
}

void parse_command(char* command, char* Mytoken,
                    char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE], int* tok_num_ptr){
    // Inizializzo le varibili e copio i parametri
	int tok_num = 0;
	char buf[MAX_COMMAND_SIZE];
	// Copio i comandi
    strcpy(buf, command);
    // Tokenizzo buffer
    char* token = strtok(buf, Mytoken);
    char* app;

    // Iterazioni per trovare tutti i comandi dati
	while (app != NULL) {
		strcpy(tok_buf[tok_num], token);
		app = strtok(NULL, token);
        if(app!=NULL) token=app;
		tok_num++;
	}
    token=strtok(token,"\n");//cosi togliamo l'ultimo e unico '\n'
    if(token!=NULL){
        strcpy(tok_buf[tok_num], token);
        tok_num++;
        // Copio il numero di token trovati
        *tok_num_ptr = tok_num;
    }
}

int make_argv(char* argv[MAX_NUM_TOK+1],
			   char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE],
			   int tok_num) {


//variabili utili
    int i=0;
    int c;
    char* temp;
    const char flag='/';
    int stop;
    char* posizione;
    int return_value;
    char* appoggio;
    //scorro tutte le righe di comandoin tok_buf
    for(c=0;c<tok_num;c++){
        stop=0;
        i=0;
        // mi prendo la stringa c-esima e la metto in temp
        temp=tok_buf[c];
        //strcat(temp," ");//aggiungiamo uno spazio cosi non ignora l'ultimo pezzo

        if(strchr(temp, flag)!=NULL){ //se c'è la prima '/' in temp
            temp=strchr(temp, flag)+1; //ho la stringa di comando effettiva
            return_value=return_value+1;
            while(stop!=1){
                posizione=strchr(temp, ' ');//posizione primo spazio
                if (posizione==NULL){
                    argv[i]=temp;
                    stop=1;
                }
                else{

                    argv[i]=temp;
                    temp=posizione+1;//aggiorno temp saltando lo spazio
                    i=i+1;
                }
            }
        }
        //se non c'è '/' o non c'è scritto nulla --> comando ignorato
    }
    return return_value;
}

int do_cmd(SimpleFS* fs, DirectoryHandle* dh, char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE], int tok_num){
    int i, j, res;
    char* argv[MAX_NUM_TOK+1];
    memset(argv, 0, MAX_NUM_TOK+1);
    for(i=0; i<tok_num; i++){
        make_argv(argv, tok_buf, tok_num);
        for(j=0; argv[j]!=NULL; j++){
            if(strcmp(argv[j], "quit") == 0) return QUIT;
            if(strcmp(argv[j], "ls")==0){
                char* names;
                res=SimpleFS_readDir(&names, dh);
                if(res!=FAILED) printf("%s\n", names);
                else printf("Errore nell'esecuzione di ls\n");
                return AGAIN;
            }
            if(strcmp(argv[j], "cd")==0){
                // TODO call SimpleFS_changeDir
                if(res==FAILED) printf("Errore nell'esecuzione di cd\n");
                return AGAIN;
            }
            if(strcmp(argv[j], "mkdir")==0){
                // TODO call SimpleFS_mkdir
                if(res==FAILED) printf("Errore nell'esecuzione di mkdir\n");
                return AGAIN;
            }
        }
    }
    return 1;
}

int main(int arg, char** args){
    //dichiarazione varibili utili
    int res;
    //questa variabile indica la funzione attualmente selezionata dall'utente
    int control=1;
    SimpleFS* fs=(SimpleFS*)malloc(sizeof(SimpleFS));
    DirectoryHandle* dh;

    //caricamento/inizializzazione del disco
    res=shell_init(fs);
    CHECK_ERR(res==FAILED,"can't initialize the disk(1)");
    dh=SimpleFS_init(fs,fs->disk);
    CHECK_ERR(dh==NULL,"can't initialize the disk(2)");

    //per ottenere i comandi inseriti dall'utente
    char command[1024];
    char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE];
    int tok_num=0;
    //char token[] = "\n";

    printf("\n\tI comandi di questa shell inziano per '/', esempio /ls, /cd, /mkdir, ecc.\n\n");

    while(control){
        printf("progettoSO_shell:%s>",dh->dcb->fcb.name);

        //lettura dei comandi da tastiera
        CHECK_ERR(fgets(command,MAX_COMMAND_SIZE,stdin)==NULL,"can't read command from input");
        /*command[0]='l';
        command[1]='s';
        command[2]='\n';*/
        parse_command(command,"; ",tok_buf,&tok_num);

        // tok_num nullo allora ricomincio
        if(tok_num<=0) continue;

        // Chiamo do_cmd che esegue i comandi
        control=do_cmd(fs, dh, tok_buf, tok_num);

        memset(command, 0, MAX_COMMAND_SIZE);
        memset(tok_buf, 0, MAX_NUM_TOK);
    }
    return 0;
}
