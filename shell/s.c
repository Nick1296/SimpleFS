
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

#include <stdio.h>
#include <stdlib.h>


int load_fs(SimpleFS* fs, DiskDriver* disk){
    DirectoryHandle* dh;
    //SimpleFS* fs, DiskDriver* disk--> me li devono passare come input vero?
    dh=SimpleFS_init(fs,disk);  
}

