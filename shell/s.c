
//con scanf comando spazio altra cosa
//no comandi esterni
//funzione d'inizio che mi carica il disco(file)
//carico il file simple fs init per caricarlo
//simple fs format e poi di nuovo init
//create file(creo nome comando) ,delete file
//leggo nome
//con scanf
//directory handle directory attuale
/// strtok dove va a finire il file
//strtok con /

//numero blocchi  //scanf
//nome file

#include <stdio.h>
#include <stdlib.h>
#include "../simplefs.h"

int shell_init(SimpleFS* fs){

    fs->filename=(char*)malloc(sizeof(char)*FILENAME_MAX_LENGTH);
    printf("inserisci il nome del file su disco:\n->");
    CHECK_ERR(fgets(fs->filename,FILENAME_MAX_LENGTH,stdin)==NULL,"can't read input filename");
    int res;
    char format
    res=DiskDriver_load(fs->disk,fs->filename);
    if(res==FAILED){
        printf("file non presente o filesystem non riconosiuto, formattare il file? (s,n)\n->");
        scanf("%c",&format);
        if(format!="s"){
            return FAILED;
        }
        printf("inserisci il numero di blocchi del \"disco\"\n");
        scanf_s("%d",fs->num_blocks);
        SimpleFS_format(fs);
    }
    return SUCCESS;

}

void parse_command(char* command,char token,char** tok_buf,int* tok_num){

}

int main(){
    //dichiarazione varibili utili
    int res;
    //questa variabile indica la funzione attualmente selezionata dall'utente
    int control=1;
    SimpleFS* fs=(SimpleFS*)malloc(sizeof(SimpleFS));
    DirectoryHandle* dh;
    //caricamento/inizializzazione del disco
    res=shell_init(fs);
    CHECK_ERR(res==FAILED,"can't initialize the disk");
    dh=SimpleFS_init(fs,fs->disk);
    CHECK_ERR(dh==NULL,"can't initialize the disk");
    //comando inserito dall'utente
    char command[1024];
    char **tok_buf;
    int tok_num;

    while(c){
        //lettura dei comandi da tastiera
        CHECK_ERR(fgets(command,1024,stdin)==NULL,"can't read command from input");
        control=parse_command(command,' ',tok_buf,&tok_num);
        switch(control){

        }
    }

    }
    //una volta uscito da questo if avremo tutto allocato -->ho gia root in memoria
    //ora aspetto l'utente che vuole fare?
    //strtok -->char*strtok(char*s, char *sep);
    while(1){
        printf("shell(scrivi 'termina' per uscire): ");
		scanf("%s", comandoutente);

		if (strcmp(comandoutente, "termina") == 0)	break;


    }

}
//int load_fs(SimpleFS* fs, DiskDriver* disk){
//    DirectoryHandle* dh;
//    //SimpleFS* fs, DiskDriver* disk--> me li devono passare come input vero?
//    dh=SimpleFS_init(fs,disk);
//}

