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
  while(strcmp(nameFile, "\n")==0){
    printf("inserisci il nome del file su disco -> ");
    CHECK_ERR(fgets(nameFile,FILENAME_MAX_LENGTH,stdin)==NULL,"can't read input filename");
  }
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
    while(format=='\n' && (format!='s' || format!='n')){
      printf("file non presente o filesystem non riconosiuto, formattare il file? (s,n)-> ");
      scanf("%c", &format);
    } 
    if(format!='s') return FAILED;

    printf("inserisci il numero di blocchi del \"disco\" -> ");
    scanf("%d",&(fs->block_num));
    while(fs->block_num<0){
      printf("inserisci il numero di blocchi del \"disco\"-> ");
      scanf("%d",&(fs->block_num));
    }
    // to get '/n' left in stdin
    getchar();
    // format the disk
    SimpleFS_format(fs);
  }
  return SUCCESS;
}

void shell_info(DiskDriver *disk){
  printf("\t Disk info:\n");
  printf("num blocks: %d\n",disk->header->num_blocks );
  printf("num bitmap blocks: %d\n",disk->header->bitmap_blocks );
  printf("bitmap entries: %d\n",disk->header->bitmap_entries );
  printf("num free blocks: %d\n",disk->header->free_blocks );
  printf("first free block: %d\n",disk->header->first_free_block );
  
  int i,j,printed=0,print;
  uint8_t mask;
  printf("\t Bitmap of disk info\n");
  print=disk->header->num_blocks;
  for(i=0;i<(disk->header->num_blocks+7)/8;i++){
    mask=128;
    printf("block %d:\n",i);
    for(j=0;j<8 && printed<print;j++){
      printf("%c",((mask & disk->bmap->entries[i])? '1' :'0'));
      mask=mask>>1;
      printed++;
    }
    printf("\n");
  }

}

void shell_shutdown(SimpleFS* fs, DirectoryHandle* dh){
  if(fs!=NULL){
    if(fs->disk!=NULL){
      if(fs->disk->header!=NULL) DiskDriver_shutdown(fs->disk);
      free(fs->disk);
    }
    free(fs->filename);
    free(fs);
  }
  if(dh!=NULL){
    free(dh->dcb);
    free(dh->current_block);
    free(dh);
  }
}

void parse_command(char* command, char* Mytoken,
                    char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE], int* tok_num_ptr){
  // Inizializzo le varibili e copio i parametri
  int tok_num = 0;
  char buf[MAX_COMMAND_SIZE];
  // Copio i comandi
  strcpy(buf, command);

  if(strlen(buf)>1){
    // Tokenizzo buffer
    char* token = strtok(buf, Mytoken);
    while(token!=NULL){
      strcpy(tok_buf[tok_num], token);
      tok_num++;
      token = strtok(NULL, Mytoken);
    }
  }
  // Copio il numero di token trovati
  *tok_num_ptr = tok_num;
}

void make_argv(char* argv[MAX_NUM_COMMAND],
                char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE],
                int tok_num) {
  int i, j=0, len=0;
  char* app;
  char* now;
  char* suc;
  char* buffer;
  memset(argv, 0, MAX_NUM_TOK+1);
  for(i=0; i<tok_num; i++){    
    now = tok_buf[i];
    now=strcat(now," ");
    if(now[0]==' ') now=now+1;
    if(strchr(now, '\n')!=NULL) *(strchr(now, '\n')) = '\0';
    if(strchr(now, '/')!=NULL){
      app=strchr(now, '/')+1;
      suc=strchr(app, ' ');
      while(suc!=NULL){
        len = suc-app+1;
        buffer = (char*)malloc(len*sizeof(char));
        memcpy(buffer, app, len);
        buffer[len-1]='\0';
        argv[j]=buffer;
        j++;
        app=suc+1;
        suc=strchr(app, ' ');
      }
      argv[j]=app;
      j++;
    }
  }
  if(argv[j]!=NULL && strcmp(argv[j], "\0")!=0){
    argv[j]="\0";
    j++;
  }
  argv[j]=NULL;
  
  //for(i=0; argv[i]!=NULL; i++) printf("argv[%d] >%s<\n",i, argv[i]);
}

int do_cat(DirectoryHandle* dh, char* argv[MAX_NUM_COMMAND], int i_init){
  int i=i_init, i_end=0;
  FileHandle* fh;
  for(; argv[i]!=NULL && strcmp(argv[i], "\0")!=0; i++){
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
  free(argv[i]);
  return i_end+i_init==i ? FAILED : SUCCESS;
}

int do_copy_file(DirectoryHandle* dh, char* argv[MAX_NUM_COMMAND], int i_init){
  int toDisk=0, i=i_init, res, resS;

  if(argv[i_init]!=NULL && strcmp(argv[i_init], "-to-disk")==0) toDisk=1;
  if(argv[i_init]!=NULL && strcmp(argv[i_init], "-from-disk")==0) toDisk=2;
  if(toDisk==0){
    if(argv[i_init]!=NULL && strcmp(argv[i_init], "--help")==0) printf("\ncp --help");
    else printf("\nNessuna opezione selezionata, copia annullata.");
    printf("\nOpzioni selezionabili:"
          "\n\t-to-disk per la copia verso SimpleFS,"
          "\n\t-from-disk per la copia da SimpleFS"
          "\n\t--help"
          "\n\nUso: cp [OPT] src dest\n\n");
    return SUCCESS;
  }
  i++;

  // Copia di file dall'esterno verso il nostro filesystem
  if(toDisk==1){
    int fd = open(argv[i], O_RDONLY);
    if(fd==-1){
      printf("Impossibile aprire il file %s\n",argv[i]);
      return FAILED;
    }
    free(argv[i]);
    i++;
    FileHandle* fh=SimpleFS_openFile(dh, argv[i]);
    if(fh==NULL){
      printf("Impossibile aprire dal Simplefs filesystem il file: %s\n",argv[i]);
      return FAILED;
    }
    free(argv[i]);

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
      if(resS!=FAILED){
        printf("Errore durante la scrittura del file su Simplefs\n");
        SimpleFS_close(fh);
      }
      close(fd);
      printf("Errore durante la lettura del file dall'host\n");
      return FAILED;
    }

    if(resS==FAILED){
      SimpleFS_close(fh);
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
    int fd = open(argv[i], O_WRONLY|O_DIRECTORY, 0666);
    if(fd==-1){
      fd = open(argv[i], O_WRONLY|O_CREAT, 0666);
      if(fd==-1){
        printf("Impossibile aprire il file: %s\n",argv[i]);
        return FAILED;
      }
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
      if(resS!=FAILED){
        printf("Errore durante la lettura del file su Simplefs\n");
        SimpleFS_close(fh);
      }
      close(fd);
      printf("Errore durante la scrittura del file sull'host\n");
      return FAILED;
    }

    if(resS==FAILED){
      SimpleFS_close(fh);
      close(fd);
      printf("Errore durante la lettura del file su Simplefs\n");
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
  for(i=0; argv[i]!=NULL; i++){
    if(strcmp(argv[i], "quit") == 0){
      free(argv[i]);
      shell_shutdown(fs, dh);
      return QUIT;
    }
    if(strcmp(argv[i], "ls")==0){
      char* names;
      res=SimpleFS_readDir(&names, dh);
      if(res!=FAILED){
        printf("%s\n", names);
        free(names);
      }
      else printf("Errore nell'esecuzione di ls\n");
      free(argv[i]);
      i++;
    }
    if(strcmp(argv[i], "cd")==0){
      free(argv[i]);
      for(j=i+1; argv[j]!=NULL && strcmp(argv[j], "\0")!=0; j++){
          res=SimpleFS_changeDir(dh, argv[j]);
          if(res==FAILED) printf("Errore nell'esecuzione di cd\n");
          free(argv[j]);
      }
      i=j;
    }
    if(strcmp(argv[i], "mkdir")==0){
      free(argv[i]);
      for(j=i+1; argv[j]!=NULL && strcmp(argv[j], "\0")!=0; j++){
          res=SimpleFS_mkDir(dh, argv[j]);
          if(res==FAILED) printf("Errore nell'esecuzione di mkdir\n");
          free(argv[j]);
      }
      i=j;
    }
    if(strcmp(argv[i], "rm")==0){
      free(argv[i]);
      for(j=i+1; argv[j]!=NULL && strcmp(argv[j], "\0")!=0; j++){
          res=SimpleFS_remove(dh, argv[j]);
          if(res==FAILED) printf("Errore nell'esecuzione di rm su file/directory: %s\n",argv[j]);
          free(argv[j]);
      }
      i=j;
    }
    if(strcmp(argv[i], "touch")==0){
      free(argv[i]);
      for(j=i+1; argv[j]!=NULL && strcmp(argv[j], "\0")!=0; j++){
          FileHandle*fh=SimpleFS_createFile(dh, argv[j]);
          if(fh==NULL) printf("Errore nell'esecuzione di touch su file: %s\n",argv[j]);
          else SimpleFS_close(fh);
          free(argv[j]);
      }
      i=j;
    }
    if(strcmp(argv[i], "cat")==0){
      free(argv[i]);
      res=do_cat(dh, argv, i+1);
      if(res==FAILED) printf("Errore nell'esecuzione di cat\n");
    }
    if(strcmp(argv[i], "cp")==0){
      free(argv[i]);
      res=do_copy_file(dh, argv, i+1);
      if(res==FAILED) printf("Errore nell'esecuzione di cp\n");
    }
    if(strcmp(argv[i], "info")==0){
      shell_info(fs->disk);
      free(argv[i]);
      i++;
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
  DirectoryHandle* dh=NULL;

  //caricamento/inizializzazione del disco
  res=shell_init(fs);
  if(res==FAILED) shell_shutdown(fs, dh);
  CHECK_ERR(res==FAILED,"can't initialize the disk(1)");
  dh=SimpleFS_init(fs,fs->disk);
  if(dh==NULL) shell_shutdown(fs, dh);
  CHECK_ERR(dh==NULL,"can't initialize the disk(2)");

  //per ottenere i comandi inseriti dall'utente
  char command[1024];
  char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE];
  int tok_num=0;
  char* ret;

  printf("\n\tI comandi di questa shell inziano per '/', esempio /ls, /cd, /mkdir, ecc.\n\n");

  while(control){
    printf("[progettoSO_shell:%s]>",dh->dcb->fcb.name);

    //lettura dei comandi da tastiera
    ret=fgets(command,MAX_COMMAND_SIZE,stdin);
    if(ret==NULL) shell_shutdown(fs, dh);
    CHECK_ERR(ret==NULL,"can't read command from input");
    
    if(strchr(command, ';')!=NULL) parse_command(command,";",tok_buf,&tok_num);
    else parse_command(command,"\n",tok_buf,&tok_num);
    
    // tok_num nullo allora ricomincio
    if(tok_num<=0) continue;
    
    // Chiamo do_cmd che esegue i comandi
    control=do_cmd(fs, dh, tok_buf, tok_num);

    memset(command, 0, MAX_COMMAND_SIZE);
    tok_num=0;
  }
  
  return 0;
}
