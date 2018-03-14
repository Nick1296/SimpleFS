#include <stdio.h>
#include <stdlib.h>
#include "simplefs.h"

#define MAX_COMMAND_SIZE 1024
#define MAX_NUM_TOK  64
#define MAX_NUM_COMMAND 512
#define QUIT 0
#define AGAIN 1

int shell_init(SimpleFS* fs){
    // chiedo il nome del file che memorizza il disco
    char* nameFile=(char*)malloc(sizeof(char)*FILENAME_MAX_LENGTH);
    memset(nameFile, 0,FILENAME_MAX_LENGTH);
    printf("inserisci il nome del file su disco -> ");
    CHECK_ERR(fgets(nameFile,FILENAME_MAX_LENGTH,stdin)==NULL,"can't read input filename");
    /*nameFile[0]='d';
    nameFile[1]='i';
    nameFile[2]='s';
    nameFile[3]='k';
    nameFile[4]='\n';*/
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

void make_argv(char* argv[MAX_NUM_COMMAND],
			   char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE],
			   int tok_num) {
    int i, j=0;
    char* app;
    char* now;
    char* suc;
    char* buffer;
    memset(argv, 0, MAX_NUM_TOK+1);
    for(i=0; i<tok_num; i++){
        now = tok_buf[i];
        now=strcat(now," ");
        if(strchr(now, '/')!=NULL){
            app=strchr(now, '/')+1;
            suc=strchr(app, ' ');
            while(suc!=NULL){
                buffer = (char*)malloc((suc-app)*sizeof(char));
                //memset(buffer, 0, suc-app);
                memcpy(buffer, app, suc-app);
                buffer[suc-app]='\0';
                argv[j]=buffer;
                j++;
                app=suc+1;
                suc=strchr(app, ' ');
            }
            argv[j]=NULL;
        }
    }
    i=0;
    while(argv[i]!=NULL){
        printf("argv[%d] = %s\n", i, argv[i]);
        i++;
    }
    
}

int do_cat(DirectoryHandle* dh, char* argv[MAX_NUM_COMMAND], int i_init){
    int i=i_init, i_end=i_init;
    FileHandle* fh;
    for(i=0; argv[i]!=NULL; i++){
        fh=SimpleFS_openFile(dh, argv[i]);
        if(fh==NULL){
            printf("Impossibile concatenare il file: %s\n",argv[i]);
            i_end++;
            continue;
        }
        char* buffer = (char*)malloc(512*sizeof(char));
        /*TODO
        while(SimpleFS_read(fh, buffer, 512)!=FAILED){
            printf("%s", buffer);
        }*/
        SimpleFS_close(fh);
    }
    return i_end+i_init==i ? FAILED : SUCCESS;
}

int do_copy_file(DirectoryHandle* dh, char* argv[MAX_NUM_COMMAND], int i_init){
    int toDisk=0, i=i_init, res, resS;

    if(argv[i_init]!=NULL && strcmp(argv[i_init], "-to-disk")==0) toDisk=1;
    if(argv[i_init]!=NULL && strcmp(argv[i_init], "-from-disk")==0) toDisk=2;
    i++;

    // Copia di file dall'esterno verso il nostro filesystem
    if(toDisk==1){
        int fd = open(argv[i], O_RDONLY);
        if(fd==-1){
            printf("Can't open file: %s\n",argv[i]);
            return FAILED;
        }
        i++;
        FileHandle* fh=SimpleFS_openFile(dh, argv[i]);
        if(fh==NULL){
            printf("Impossibile aprire dal Simplefs filesystem il file: %s\n",argv[i]);
            return FAILED;
        }

        int num_letti=0;
        resS=1;
        char* buffer = (char*)malloc(BLOCK_SIZE*sizeof(char));
        memset(buffer, 0, BLOCK_SIZE);
        res=read(fd, buffer, BLOCK_SIZE);
        num_letti=res;
        while(resS!=FAILED && res<=BLOCK_SIZE && res!=-1){
            /* TODO
            resS=SimpleFS_write(fh, buffer, BLOCK_SIZE);*/
            memset(buffer, 0, BLOCK_SIZE);
            res=read(fd , buffer, BLOCK_SIZE);
            if(res!=-1) num_letti=res;
        }

        while(resS!=FAILED && res==-1 && errno == EINTR){
            res=lseek(fd, num_letti, SEEK_SET);
            if(res==-1) return FAILED;
            res=read(fd , buffer+num_letti+1, BLOCK_SIZE-num_letti);
            /*TODO
            resS=SimpleFS_write(fh, buffer, BLOCK_SIZE);*/
            memset(buffer, 0, BLOCK_SIZE);
            if(res!=-1) num_letti=res;
        }

        if(res==-1 && errno != EINTR){
            if(resS!=FAILED) printf("Errore durante la scrittura del file su Simplefs\n");
            printf("Errore durante la lettura del file dall'host\n");
            return FAILED;
        }

        if(resS!=FAILED){
            printf("Errore durante la scrittura del file su Simplefs\n");
            return FAILED;
        }

        close(fd);
        SimpleFS_close(fh);
    }

    // Copia di file dall'interno verso l'esterno
    if(toDisk==2){
        FileHandle* fh=SimpleFS_openFile(dh, argv[i]);
        if(fh==NULL){
            printf("Impossibile aprire dal Simplefs filesystem il file: %s\n",argv[i]);
            return FAILED;
        }
        i++;
        int fd = open(argv[i], O_RDONLY);
        if(fd==-1){
            printf("Can't open file: %s\n",argv[i]);
            return FAILED;
        }

        res=1;
        char* buffer = (char*)malloc(BLOCK_SIZE*sizeof(char));
        memset(buffer, 0, BLOCK_SIZE);
        /* TODO
        resS=SimpleFS_read(fh, buffer, BLOCK_SIZE);*/
        while(resS!=FAILED && res<=BLOCK_SIZE && res!=-1){
            res=write(fd , buffer, BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);
            /* TODO
            resS=SimpleFS_read(fh, buffer, BLOCK_SIZE);*/
        }

        while(resS!=FAILED && res==-1 && errno == EINTR){
            /* TODO
            resS=SimpleFS_read(fh, buffer, BLOCK_SIZE);*/
            res=write(fd , buffer, BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);
        }

        if(res==-1 && errno != EINTR){
            if(resS!=FAILED) printf("Errore durante la scrittura del file su Simplefs\n");
            printf("Errore durante la lettura del file dall'host\n");
            return FAILED;
        }

        if(resS!=FAILED){
            printf("Errore durante la scrittura del file su Simplefs\n");
            return FAILED;
        }

        close(fd);
        SimpleFS_close(fh);
    }

    return SUCCESS;
}

int do_cmd(SimpleFS* fs, DirectoryHandle* dh, char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE], int tok_num){
    int i, j, res;
    char* argv[MAX_NUM_COMMAND];
    memset(argv, 0, MAX_NUM_TOK+1);
    make_argv(argv, tok_buf, tok_num);
    for(i=0; i<tok_num; i++){
        if(argv[i]!=NULL && strcmp(argv[i], "quit") == 0) return QUIT;
        if(argv[i]!=NULL && strcmp(argv[i], "ls")==0){
            char* names;
            res=SimpleFS_readDir(&names, dh);
            if(res!=FAILED) printf("%s\n", names);
            else printf("Errore nell'esecuzione di ls\n");
        }
        if(argv[i]!=NULL && strcmp(argv[i], "cd")==0){
            for(j=i+1; argv[j]!=NULL; j++){
                res=SimpleFS_changeDir(dh, argv[j]);
                if(res==FAILED) printf("Errore nell'esecuzione di cd\n");
            }
            i=j+1;
        }
        if(argv[i]!=NULL && strcmp(argv[i], "mkdir")==0){
printf("riconociuto mkdir\n");
            for(j=i+1; argv[j]!=NULL; j++){
                // TODO Va in errore quando tenta di farlo
                res=SimpleFS_mkDir(dh, argv[j]);
                if(res==FAILED) printf("Errore nell'esecuzione di mkdir\n");
            }
            i=j+1;
        }
        if(argv[i]!=NULL && strcmp(argv[i], "rm")==0){
            for(j=i+1; argv[j]!=NULL; j++){
                res=SimpleFS_remove(dh, argv[j]);
                if(res==FAILED) printf("Errore nell'esecuzione di rm\n");
            }
            i=j+1;
        }
        if(argv[i]!=NULL && strcmp(argv[i], "touch")==0){
printf("riconociuto touch\n");
            for(j=i+1; argv[j]!=NULL; j++){
                // TODO non funziona correttamente perche'
                // caso di creazione del file se non esiste non
                // e' stato implementato
                FileHandle*fh=SimpleFS_openFile(dh, argv[j]);
                if(fh==NULL) printf("Errore nell'esecuzione di touch\n");
                else SimpleFS_close(fh);
            }
            i=j+1;
        }
        if(argv[i]!=NULL && strcmp(argv[i], "cat")==0){
printf("riconociuto cat\n");
            res=do_cat(dh, argv, i+1);
            if(res==FAILED) printf("Errore nell'esecuzione di cat\n");
        }
        if(argv[i]!=NULL && strcmp(argv[i], "cp")==0){
printf("riconociuto cp\n");
            res=do_copy_file(dh, argv, i+1);
            if(res==FAILED) printf("Errore nell'esecuzione di cp\n");
        }
    }
    return AGAIN;
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

    printf("\n\tI comandi di questa shell inziano per '/', esempio /ls, /cd, /mkdir, ecc.\n\n");

    while(control){
        printf("progettoSO_shell:%s>",dh->dcb->fcb.name);

        //lettura dei comandi da tastiera
        CHECK_ERR(fgets(command,MAX_COMMAND_SIZE,stdin)==NULL,"can't read command from input");
        // Per un esezione senza input
        /*command[0]='/';
        command[1]='m';
        command[2]='k';
        command[3]='d';
        command[4]='i';
        command[5]='r';
        command[6]=' ';
        command[7]='c';
        command[8]='i';
        command[9]='a';
        command[10]='o';
        command[11]='\n';*/
        /*command[0]='/';
        command[1]='c';
        command[2]='d';
        command[3]=' ';
        command[4]='c';
        command[5]='i';
        command[6]='a';
        command[7]='o';
        command[8]='\n';*/
        parse_command(command,"\n",tok_buf,&tok_num);

        // tok_num nullo allora ricomincio
        //if(tok_num<=0) continue;

        // Chiamo do_cmd che esegue i comandi
        control=do_cmd(fs, dh, tok_buf, tok_num);

        memset(command, 0, MAX_COMMAND_SIZE);
        //memset(tok_buf, 0, MAX_NUM_TOK);
    }
    free(fs);
    free(dh);
    return 0;
}
