#include <stdio.h>
#include <stdlib.h>
#include "simplefs.h"

#define MAX_COMMAND_SIZE 1024
#define MAX_NUM_TOK  64
#define MAX_NUM_COMMAND 512
#define QUIT 0
#define AGAIN 1

// Permette la creazione o il caricamento di un disco
// sul quale verrà memorizzato il nostro filesystem
int shell_init(SimpleFS *fs) {
	// chiedo il nome del file che memorizza il disco
	char *nameFile = (char *) malloc(sizeof(char) * FILENAME_MAX_LENGTH);
	memset(nameFile, 0, FILENAME_MAX_LENGTH);
	printf("inserisci il nome del file su disco -> ");
	CHECK_ERR(fgets(nameFile, FILENAME_MAX_LENGTH, stdin) == NULL, "can't read input filename");
	while (strcmp(nameFile, "\n") == 0) {
		printf("inserisci il nome del file su disco -> ");
		CHECK_ERR(fgets(nameFile, FILENAME_MAX_LENGTH, stdin) == NULL, "can't read input filename");
	}
	nameFile[strlen(nameFile) - 1] = '\0';
	fs->filename = nameFile;

	int res;
	char format;
	fs->disk = (DiskDriver *) malloc(sizeof(DiskDriver));
	memset(fs->disk, 0, sizeof(DiskDriver));
	// carico il disco
	res = DiskDriver_load(fs->disk, fs->filename);

	// se non riesco a caricarlo significa che non esiste un disco con quel nome
	// allora chiedo se vuole crearlo
	if (res == FAILED) {
		printf("file non presente o filesystem non riconosiuto, formattare il file? (s,n)-> ");
		scanf("%c", &format);
		while (format != 's' && format != 'n') {
			//to get the \n left in stdin
			getchar();
			printf("file non presente o filesystem non riconosiuto, formattare il file? (s,n)-> ");
			scanf("%c", &format);
			printf("%c\n", format);
		}
		if (format != 's') return FAILED;

		printf("inserisci il numero di blocchi del \"disco\" -> ");
		scanf("%d", &(fs->block_num));
		while (fs->block_num < 0) {
			printf("inserisci il numero di blocchi del \"disco\"-> ");
			scanf("%d", &(fs->block_num));
		}
		// to get '/n' left in stdin
		getchar();
		// format the disk
		SimpleFS_format(fs);
	}
	return SUCCESS;
}

// Fornisce informazioni sul disco
void shell_info(DiskDriver *disk) {
	printf("\t Disk info:\n");
	printf("num blocks: %d\n", disk->header->num_blocks);
	printf("num bitmap blocks: %d\n", disk->header->bitmap_blocks);
	printf("bitmap entries: %d\n", disk->header->bitmap_entries);
	printf("num free blocks: %d\n", disk->header->free_blocks);
	printf("first free block: %d\n", disk->header->first_free_block);

}

// Fornisce le informazioni in riguardo alla
// bitmap del disco
void bitmap_info(DiskDriver *disk) {
	int i, j, printed = 0, print;
	uint8_t mask;
	printf("\t Bitmap of disk info\n");
	print = disk->header->num_blocks;
	for (i = 0; i < (disk->header->num_blocks + 7) / 8; i++) {
		mask = 128;
		printf("block %d:\n", i);
		for (j = 0; j < 8 && printed < print; j++) {
			printf("%c", ((mask & disk->bmap->entries[i]) ? '1' : '0'));
			mask = mask >> 1;
			printed++;
		}
		printf("\n");
	}
}

// Funzione che pulisce la memori utilizzata dalla shell
void shell_shutdown(SimpleFS *fs, DirectoryHandle *dh) {
	if (fs != NULL) {
		if (fs->disk != NULL) {
			if (fs->disk->header != NULL) DiskDriver_shutdown(fs->disk);
			free(fs->disk);
		}
		free(fs->filename);
		free(fs);
	}
	if (dh != NULL) {
		free(dh->dcb);
		free(dh->current_block);
		free(dh);
	}
}

void parse_command(char *command, char *Mytoken,
                   char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE], int *tok_num_ptr) {
	// Inizializzo le varibili e copio i parametri
	int tok_num = 0;
	char buf[MAX_COMMAND_SIZE];
	// Copio i comandi
	strcpy(buf, command);

	if (strlen(buf) > 1) {
		// Tokenizzo buffer
		char *token = strtok(buf, Mytoken);
		while (token != NULL) {
			strcpy(tok_buf[tok_num], token);
			tok_num++;
			token = strtok(NULL, Mytoken);
		}
	}
	// Copio il numero di token trovati
	*tok_num_ptr = tok_num;
}

// Ottiene i comandi e gli argomenti degli stessi
// insieriti dall'utente da riga di comando
void make_argv(char *argv[MAX_NUM_COMMAND],
               char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE],
               int tok_num) {
	int i, j = 0, len = 0;
	char *app;
	char *now;
	char *suc;
	char *buffer;
	memset(argv, 0, MAX_NUM_TOK + 1);
	// Scorro tutti i token
	for (i = 0; i < tok_num; i++) {
		now = tok_buf[i];
		now = strcat(now, " ");
		if (now[0] == ' ') now = now + 1;
		if (strchr(now, '\n') != NULL) *(strchr(now, '\n')) = ' ';
		// Ogni '/' che trovo quello mi indica
		// l'inizio di un nuovo comando
		if (strchr(now, '/') != NULL) {
			app = strchr(now, '/') + 1;
			suc = strchr(app, ' ');
			while (suc != NULL) {
				len = (int) (suc - app + 1);
				buffer = (char *) malloc(len * sizeof(char));
				memcpy(buffer, app, (size_t) len);
				buffer[len - 1] = '\0';
				argv[j] = buffer;
				j++;
				app = suc + 1;
				suc = strchr(app, ' ');
			}
			argv[j] = app;
			j++;
		}
	}
	argv[j] = NULL;
}

int do_cat(DirectoryHandle *dh, char *argv[MAX_NUM_COMMAND], int i_init) {
	int i = i_init;
	FileHandle *fh;
	// Cerco di contenare tutti i file passati
	for (; argv[i] != NULL && strcmp(argv[i], "\0") != 0; i++) {
		// Apro il file e mi metto all'inizio dello stesso
		fh = SimpleFS_openFile(dh, argv[i]);
		if (fh == NULL) printf("Impossibile concatenare il file: %s\n", argv[i]);
		else {
			if (SimpleFS_seek(fh, 0) == FAILED) printf("Impossibile concatenare il file: %s\n", argv[i]);
			else {
				// Se tutto e' andato bene allora leggo l'intero contenuto del file
				int letti = 0;
				int dim_let = 512;
				char *buffer = (char *) malloc(dim_let * sizeof(char));
				int daLeggere = fh->fcb->fcb.size_in_bytes;
				printf("da leggere: %d\n", daLeggere);
				while (letti < daLeggere) {
					memset(buffer, 0, dim_let * sizeof(char));
					if (daLeggere - letti >= dim_let) letti += SimpleFS_read(fh, buffer, dim_let);
					else letti += SimpleFS_read(fh, buffer, daLeggere - letti);
					printf("%s", buffer);
				}
				printf("letti: %d\n", letti);
				// Libero la memoria occupata
				free(buffer);
			}
			// Chiudo il file
			SimpleFS_close(fh);
		}
	}
	return i;
}

int do_copy_file(DirectoryHandle *dh, char *argv[MAX_NUM_COMMAND], int i_init) {
	int toDisk = 0, i = i_init, res = FAILED, resS = FAILED;
	// Selezioni che consentono di selezionare la giusta operazione da fare
	// secondo l'opzione scelta dall'utente
	if (argv[i_init] != NULL && strcmp(argv[i_init], "-to-disk") == 0) toDisk = 1;
	if (argv[i_init] != NULL && strcmp(argv[i_init], "-from-disk") == 0) toDisk = 2;
	// Nel caso di opzione non scelta stampo l'help del comando
	if (toDisk == 0) {
		if (argv[i_init] != NULL && strcmp(argv[i_init], "--help") == 0) printf("\ncp --help");
		else printf("\nNessuna opezione selezionata, copia annullata.");
		printf("\nOpzioni selezionabili:"
		       "\n\t-to-disk per la copia verso SimpleFS"
		       "\n\t-from-disk per la copia da SimpleFS"
		       "\n\t--help"
		       "\n\nUso: cp [OPT] src dest\n\n");
		return i;
	}
	i++;

	// Copia di file dall'esterno verso il nostro filesystem
	if (toDisk == 1) {
		// Cerco di aprire entrambi i file
		int fd = open(argv[i], O_RDONLY);
		if (fd == -1) {
			printf("Impossibile aprire il file %s\n", argv[i]);
			return i;
		}
		i++;
		FileHandle *fh = SimpleFS_openFile(dh, argv[i]);
		if (fh == NULL) {
			fh = SimpleFS_createFile(dh, argv[i]);
			if (fh == NULL) {
				printf("Impossibile aprire dal Simplefs filesystem il file: %s\n", argv[i]);
				return i;
			}
		}

		// Se tutto è andato a buon fine leggo il file passato come
		// sorgente dall'inzio e lo scrivo su file destinazione
		int daLeggere = (int) lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		int num_letti = 0;
		char *buffer = (char *) malloc(BLOCK_SIZE * sizeof(char));
		// Iterazioni per leggere tutto il file
		while (num_letti < daLeggere) {
			memset(buffer, 0, BLOCK_SIZE);
			res = (int) read(fd, buffer, BLOCK_SIZE);
			if (res == -1 && errno == EINTR) continue;
			if (res > 0) {
				num_letti += res;
				resS = SimpleFS_write(fh, buffer, res);
				if (resS == FAILED) {
					printf("Impossibile scrivere su file\n");
					num_letti = daLeggere;
				}
			} else num_letti = daLeggere;
			printf("\r[%d/%d", num_letti, daLeggere);
			fflush(stdout);
		}
		printf("]\n");

		// Se la read è andata in errore comunico l'accaduto
		if (res == -1 && errno != EINTR) {
			if (resS != FAILED) {
				printf("Errore durante la scrittura del file su Simplefs\n");
				SimpleFS_close(fh);
			}
			close(fd);
			printf("Errore durante la lettura del file dall'host\n");
			return i;
		}

		// Se la write è andata in errore comunico l'accaduto
		if (resS == FAILED) {
			SimpleFS_close(fh);
			printf("Errore durante la scrittura del file su Simplefs\n");
			return i;
		}

		// Libero le risorse
		close(fd);
		SimpleFS_close(fh);
		free(buffer);
	}

	// Copia di file dall'interno verso l'esterno
	if (toDisk == 2) {
		// Cerco di aprire entrambi i file
		FileHandle *fh = SimpleFS_openFile(dh, argv[i]);
		if (fh == NULL) {
			printf("Impossibile aprire dal Simplefs filesystem il file: %s\n", argv[i]);
			return i;
		}
		i++;
		int fd = open(argv[i], O_WRONLY | O_DIRECTORY, 0666);
		if (fd == -1) {
			fd = open(argv[i], O_WRONLY | O_TRUNC | O_CREAT, 0666);
			if (fd == -1) {
				printf("Impossibile aprire il file: %s\n", argv[i]);
				return i;
			}
		}

		// Se tutto è andato a buon fine leggo il file passato come
		// sorgente dall'inzio e lo scrivo su file destinazione
		if (SimpleFS_seek(fh, 0) == FAILED) {
			printf("Impossibile leggere il file sorgente\n");
			return i;
		}
		char *buffer = (char *) malloc(BLOCK_SIZE * sizeof(char));
		resS = 0;
		int daLeggere = fh->fcb->fcb.size_in_bytes;
		int attuale = 0;
		printf("\n[");
		// Iterazioni per leggere tutto il file
		while (resS < daLeggere) {
			memset(buffer, '\0', BLOCK_SIZE);
			res = resS;
			if (daLeggere - resS > BLOCK_SIZE) resS += SimpleFS_read(fh, buffer, BLOCK_SIZE);
			else resS += SimpleFS_read(fh, buffer, daLeggere - resS);
			attuale = 0;
			// Iterazione per scrivere correttamente i byte letti
			// sul file destinazione
			while (res < resS) {
				res += write(fd, buffer + attuale, (size_t) (resS - res));
				attuale = res;
				if (res == -1 && errno == EINTR) continue;
				// In caso di errore diverso da interruzzione
				// comunico che non posso scrivere
				if (res == -1) {
					printf("Impossibile scrivere su file\n");
					res = resS;
					resS = daLeggere;
				}
			}
			printf("\r[%d/%d", resS, daLeggere);
			fflush(stdout);
		}
		printf("]\n");

		// Se la write è andata in errore comunico l'accaduto
		if (res == -1 && errno != EINTR) {
			if (resS != FAILED) {
				printf("Errore durante la lettura del file su Simplefs\n");
				SimpleFS_close(fh);
			}
			close(fd);
			printf("Errore durante la scrittura del file sull'host\n");
			return i;
		}

		// Libero le risorse
		close(fd);
		SimpleFS_close(fh);
		free(buffer);
	}

	return i;
}

// penultimo spazio di argv vedere se contiene >    ====> se contiene > l'ultimo spazio contiene file destinazione
int echo(char *argv[MAX_NUM_TOK + 1], DirectoryHandle *d, int i_init) {
	//variabili utili
	int i = i_init;
	int control = 0;//variabile che se è settata a 1 mi indica che bisogna scrivere su file
	char *result = malloc(sizeof(char) * MAX_COMMAND_SIZE);
	FileHandle *fh;
	int b_scritti;

	for (; argv[i] != NULL && strcmp(argv[i], "\0") != 0; i++) {
		//composizione stringa finale in result, se si trova '>' result dovrà essere scritto  su file
		if (strcmp(argv[i], ">") != 0) {
			result = strcat(result, argv[i]);
			result = strcat(result, " ");
		} else {
			control = 1;
			i++;
			//se manca file di destinazione
			if (argv[i] == NULL) {
				printf("file destinazione non presente.\n%s", result);
				return i;
			}
			fh = SimpleFS_createFile(d, argv[i]);
			if (fh == NULL) {
				fh = SimpleFS_openFile(d, argv[i]);
				if (fh == NULL) {
					printf("errore sulla open del file\n");
					return i;
				}
			}
			b_scritti = SimpleFS_write(fh, (void *) result, (int) strlen(result));
			while (b_scritti == -1 || b_scritti != strlen(result)) {
				b_scritti = SimpleFS_write(fh, (void *) result + b_scritti, (int) (strlen(result) - b_scritti));
			}
			SimpleFS_close(fh);
			//Al completamento con esito positivo, la funzione deve aprire il file e restituire un numero intero non negativo che rappresenta
			//la corretta esecuzione della funzione.
			//Altrimenti, FAILED deve essere restituito.
			//Nessun file deve essere creato o modificato se la funzione restituisce FAILED.
		}
	}
	//uscito da questo ciclo, se c'era il '>', il file sarà gia stato scritto(con successo o insuccesso) e quindi non sarà necessario fare niente
	//altrimenti sarà necessaria una printf di result
	if (control != 1) {
		//non devo mettere niente su nessun file==>printf
		printf("\n %s \n", result);
	}
	free(result);
	return i;
}

int do_cmd(SimpleFS *fs, DirectoryHandle *dh, char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE], int tok_num) {
	int i, j, res;
	char *argv[MAX_NUM_COMMAND];
	memset(argv, 0, MAX_NUM_TOK + 1);
	// Ottengo i comandi con i loro argomenti
	make_argv(argv, tok_buf, tok_num);
	// Iterazioni che scorrono tutti i possibili comandi dati
	for (i = 0; argv[i] != NULL; i++) {
		if (argv[i] != NULL && strcmp(argv[i], "help") == 0) {
			printf("\n[progettoSO_shell] HELP\n"
			       "I comandi che puoi dare in questa semplice shell sono:\n"
			       "\t- /help per mostrare questo messaggio\n"
			       "\t- /ls per mostrare l'elenco dei file contenuti nella working directory\n"
			       "\t- /info per avere informazioni sul disco\n"
			       "\t- /info -bmap per avere informazioni sulla bitmap del disco\n"
			       "\t- /touch per creare un file vuoto\n"
			       "\t- /echo per scrivere un messaggio su un file\n"
			       "\t- /cat per concatenare i file e scriverli sullo schermo\n"
			       "\t- /mkdir per creare una nuova directory\n"
			       "\t- /cd per cambiare la directory di lavoro\n"
			       "\t- /rm per rimuovere un file o una directory,"
			       " rimuove una directory in maniera ricorsiva se questa non e' vuota\n"
			       "\t- /cp per copiare un file da o verso SimpleFs\n\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "quit") == 0) {
			shell_shutdown(fs, dh);
			free(argv[i]);
			return QUIT;
		}
		if (argv[i] != NULL && strcmp(argv[i], "ls") == 0) {
			char *names;
			res = SimpleFS_readDir(&names, dh);
			if (res != FAILED) {
				printf("\n%s\n", names);
				free(names);
			} else printf("Errore nell'esecuzione di ls\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "cd") == 0) {
			for (j = i + 1; argv[j] != NULL && strcmp(argv[j], "\0") != 0; j++) {
				res = SimpleFS_changeDir(dh, argv[j]);
				if (res == FAILED) printf("Errore nell'esecuzione di cd\n");
			}
			i = j;
		}
		if (argv[i] != NULL && strcmp(argv[i], "mkdir") == 0) {
			for (j = i + 1; argv[j] != NULL && strcmp(argv[j], "\0") != 0; j++) {
				res = SimpleFS_mkDir(dh, argv[j]);
				if (res == FAILED) printf("Errore nell'esecuzione di mkdir\n");
			}
			i = j;
		}
		if (argv[i] != NULL && strcmp(argv[i], "rm") == 0) {
			for (j = i + 1; argv[j] != NULL && strcmp(argv[j], "\0") != 0; j++) {
				res = SimpleFS_remove(dh, argv[j]);
				if (res == FAILED) printf("Errore nell'esecuzione di rm su file/directory: %s\n", argv[j]);
			}
			i = j;
		}
		if (argv[i] != NULL && strcmp(argv[i], "touch") == 0) {
			for (j = i + 1; argv[j] != NULL && strcmp(argv[j], "\0") != 0; j++) {
				FileHandle *fh = SimpleFS_createFile(dh, argv[j]);
				if (fh == NULL) printf("Errore nell'esecuzione di touch su file: %s\n", argv[j]);
				else SimpleFS_close(fh);
			}
			i = j;
		}
		if (argv[i] != NULL && strcmp(argv[i], "cat") == 0) {
			i += do_cat(dh, argv, i + 1);
		}
		if (argv[i] != NULL && strcmp(argv[i], "cp") == 0) {
			i += do_copy_file(dh, argv, i + 1);
		}
		if (argv[i] != NULL && strcmp(argv[i], "info") == 0) {
			if (argv[i + 1] != NULL && strcmp(argv[i + 1], "-bmap") == 0) {
				bitmap_info(fs->disk);
			} else {
				shell_info(fs->disk);
			}
		}
		if (argv[i] != NULL && strcmp(argv[i], "echo") == 0) {
			i += echo(argv, dh, i + 1);
		}
	}

	// Libero la memoria dai vari comandi
	for (i = 0; argv[i] != NULL; i++) if (strcmp(argv[i], "\0") != 0) free(argv[i]);

	return AGAIN;
}

int main(int arg, char **args) {
	//dichiarazione varibili utili
	int res;
	//questa variabile indica la funzione attualmente selezionata dall'utente
	int control = 1;
	SimpleFS *fs = (SimpleFS *) malloc(sizeof(SimpleFS));
	DirectoryHandle *dh = NULL;

	//caricamento/inizializzazione del disco
	res = shell_init(fs);
	if (res == FAILED) shell_shutdown(fs, dh);
	CHECK_ERR(res == FAILED, "can't initialize the disk(1)");
	dh = SimpleFS_init(fs, fs->disk);
	if (dh == NULL) shell_shutdown(fs, dh);
	CHECK_ERR(dh == NULL, "can't initialize the disk(2)");

	//per ottenere i comandi inseriti dall'utente
	char command[1024];
	char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE];
	int tok_num = 0;
	char *ret;

	printf("\n\tI comandi di questa shell inziano per '/', esempio /ls, /cd, /mkdir, ecc.\n\n");

	while (control) {
		printf("[progettoSO_shell:%s]>", dh->dcb->fcb.name);

		//lettura dei comandi da tastiera
		ret = fgets(command, MAX_COMMAND_SIZE, stdin);
		if (ret == NULL) shell_shutdown(fs, dh);
		CHECK_ERR(ret == NULL, "can't read command from input");

		if (strchr(command, ';') != NULL) parse_command(command, ";", tok_buf, &tok_num);
		else parse_command(command, "\n", tok_buf, &tok_num);

		// tok_num nullo allora ricomincio
		if (tok_num <= 0) continue;

		// Chiamo do_cmd che esegue i comandi
		control = do_cmd(fs, dh, tok_buf, tok_num);

		memset(command, 0, MAX_COMMAND_SIZE);
		tok_num = 0;
	}

	return 0;
}
