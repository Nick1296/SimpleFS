
//con scanf comando spazio altra cosa
//no comandi esterni 
//funzione d'inizio che mi carica il disco(file)
//carico il file simple fs init per caricarlo 
//simple fs format e poi di nuovo init
//create file(creo nome comando) ,delete file 
//leggo nome 
//con scanf 
//directory handle directory attuale
/// str took dove va a finire il file
//str took con /

//numero blocchi  //scanf 
//nome file 

#include <stdio.h>
#include <stdlib.h>

int main(){
    //dichiarazione varibili utili 
    int num_blocks;
    const char* filename;
    DiskDriver* disk;
    int control,decisione;
    SimpleFS* fs=(SimpleFS*)malloc(sizeof(SimpleFS));
    DirectoryHandle* dh;
    //inserimento parametri
    printf("inserire numero di blocchi: ");   
	scanf("%d", num_blocks);  

    printf("inserire nome file: ");   
	scanf("%s", filename);      


    //#define FAILED -1
    //#define SUCCESS 0

    //typedef struct {
    //DiskDriver* disk;
    //int block_num;
    //char* filename;
    //} SimpleFS;
    control=DiskDriver_load(disk,filename,num_blocks);
    if(control==-1){
        printf("errore nel caricamento del diskDriver\n");
        //devo chiedere se vuole formattare il disco 
        printf("si vuole resettare il disco?(in serire 1 per 'Si' , 0 per 'No'): ");   
	    scanf("%d", decisione); 
        if(decisione==1){
            //inserisco i campi nel mio fs
            fs->block_num=num_blocks;
            fs->filename=filename;
            SimpleFS_format(fs);
            //init puo restituire : se va tutto bene-->directoryhandle oppure null se succedono cose brutte
            //DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk);
            dh=SimpleFS_init(fs,disk);
        }
        else{
            //0 o altri simboli 
            printf("Terminazione in corso\n")
            return 0;// si schianta 
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

