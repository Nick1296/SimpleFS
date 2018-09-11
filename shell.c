#include <stdio.h>
#include "simplefs_shell_apis.h"

#define MAX_COMMAND_SIZE 1024
#define MAX_NUM_TOK  64
#define MAX_NUM_COMMAND 512
#define QUIT 0
#define AGAIN 1
#define APPEND 1
#define OVERWRITE 0

// Permette la creazione o il caricamento di un disco
// sul quale verrà memorizzato il nostro filesystem
int shell_init(SimpleFS *fs) {
	//we initialize the shell as root
	int root_user = ROOT;
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
	res = shell_loadDisk(fs->disk, fs->filename);
	
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
		shell_formatDisk(fs, root_user);
	}
	if (res == PERM_ERR) {
		printf("permessi insufficienti per completare l'operazione\n");
		return FAILED;
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
void shell_shutdown(SimpleFS *fs, DirectoryHandle *dh, Wallet *wallet) {
	//we destroy the wallet
	destroy_wallet(wallet);
	if (fs != NULL) {
		if (fs->disk != NULL) {
			if (fs->disk->header != NULL) shell_shutdownDisk(fs->disk);
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
		app = now;
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
	argv[j] = NULL;
}

int do_cat(DirectoryHandle *dh, char *argv[MAX_NUM_COMMAND], int i_init, Wallet *wallet) {
	int i = i_init;
	FileHandle *fh;
	// Cerco di contenare tutti i file passati
	for (; argv[i] != NULL && strcmp(argv[i], "&&") != 0 && strcmp(argv[i], "\0") != 0; i++) {
		// Apro il file e mi metto all'inizio dello stesso
		fh = shell_openFile(dh, argv[i], wallet);
		if (fh == NULL) printf("Impossibile concatenare il file: %s\n", argv[i]);
		else {
			int res = shell_seekFile(fh, 0, wallet);
			if (res == PERM_ERR) {
				printf("Impossibile concatenare il file: %s, permessi insufficienti\n", argv[i]);
			} else {
				if (res == FAILED) printf("Impossibile concatenare il file: %s\n", argv[i]);
				else {
					// Se tutto e' andato bene allora leggo l'intero contenuto del file
					int letti = 0;
					int dim_let = 512;
					char *buffer = (char *) malloc(dim_let * sizeof(char));
					int daLeggere = fh->fcb->fcb.size_in_bytes;
					printf("da leggere: %d\n", daLeggere);
					while (letti < daLeggere) {
						memset(buffer, 0, dim_let * sizeof(char));
						if (daLeggere - letti >= dim_let) letti += shell_readFile(fh, buffer, dim_let, wallet);
						else letti += shell_readFile(fh, buffer, daLeggere - letti, wallet);
						printf("%s", buffer);
					}
					printf("\nletti: %d\n", letti);
					// Libero la memoria occupata
					free(buffer);
				}
			}
			// Chiudo il file
			shell_closeFile(fh);
		}
	}
	return i;
}

int do_copy_file(DirectoryHandle *dh, char *argv[MAX_NUM_COMMAND], int i_init, Wallet *wallet) {
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
		return i_init;
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
		FileHandle *fh = shell_openFile(dh, argv[i], wallet);
		if (fh == NULL) {
			fh = shell_createFile(dh, argv[i], wallet);
			if (fh == NULL) {
				printf("Impossibile aprire dal Simplefs filesystem il file: %s\n", argv[i]);
				return i;
			}
		}
		i++;
		
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
				resS = shell_writeFile(fh, buffer, res, wallet);
				if (resS == FAILED || PERM_ERR) {
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
				shell_closeFile(fh);
			}
			close(fd);
			printf("Errore durante la lettura del file dall'host\n");
			return i;
		}
		
		// Se la write è andata in errore comunico l'accaduto
		if (resS == FAILED) {
			shell_closeFile(fh);
			printf("Errore durante la scrittura del file su Simplefs\n");
			return i;
		}
		
		// Libero le risorse
		close(fd);
		shell_closeFile(fh);
		free(buffer);
	}
	
	// Copia di file dall'interno verso l'esterno
	if (toDisk == 2) {
		// Cerco di aprire entrambi i file
		FileHandle *fh = shell_openFile(dh, argv[i], wallet);
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
		i++;
		
		// Se tutto è andato a buon fine leggo il file passato come
		// sorgente dall'inzio e lo scrivo su file destinazione
		int seek_res = shell_seekFile(fh, 0, wallet);
		if (seek_res == FAILED) {
			printf("Impossibile leggere il file sorgente\n");
			return i;
		}
		if (seek_res == PERM_ERR) {
			printf("Impossibile leggere il file sorgente, permessi insufficienti\n");
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
			//todo there could be problems in here because read can return failed or perm_err
			if (daLeggere - resS > BLOCK_SIZE) resS += shell_readFile(fh, buffer, BLOCK_SIZE, wallet);
			else resS += shell_readFile(fh, buffer, daLeggere - resS, wallet);
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
				shell_closeFile(fh);
			}
			close(fd);
			printf("Errore durante la scrittura del file sull'host\n");
			return i;
		}
		
		// Libero le risorse
		close(fd);
		shell_closeFile(fh);
		free(buffer);
	}
	
	return i;
}

// penultimo spazio di argv vedere se contiene >    ====> se contiene > l'ultimo spazio contiene file destinazione
int echo(char *argv[MAX_NUM_TOK + 1], DirectoryHandle *d, int i_init, Wallet *wallet, int append) {
	//variabili utili
	int i = i_init, res;
	int control = 0;//variabile che se è settata a 1 mi indica che bisogna scrivere su file
	char *result = malloc(sizeof(char) * MAX_COMMAND_SIZE);
	memset(result, 0, MAX_COMMAND_SIZE);
	FileHandle *fh;
	int b_scritti;
	
	for (; argv[i] != NULL && !control && strcmp(argv[i], "\0") != 0; i++) {
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
			fh = shell_createFile(d, argv[i], wallet);
			if (fh == NULL) {
				fh = shell_openFile(d, argv[i], wallet);
				if (fh == NULL) {
					printf("errore sulla open del file\n");
					return i;
				}
			}
			//we seek at the end of the file to append new content
			if (append) {
				res = shell_seekFile(fh, fh->fcb->fcb.size_in_bytes, wallet);
				if (res == FAILED || res == PERM_ERR) {
					return res;
				}
			}
			b_scritti = shell_writeFile(fh, (void *) result, (int) strlen(result), wallet);
			if (b_scritti == FAILED || b_scritti == PERM_ERR) return b_scritti;
			while (b_scritti != strlen(result)) {
				b_scritti = shell_writeFile(fh, (void *) result + b_scritti, (int) (strlen(result) - b_scritti), wallet);
				if (b_scritti == FAILED || b_scritti == PERM_ERR) return b_scritti;
			}
			shell_closeFile(fh);
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

int do_su(char *username, Wallet *wallet) {
	//we check if we have valid data
	if (username == NULL || strlen(username) > NAME_LENGTH || wallet == NULL) {
		return FAILED;
	}
	//we search for the given user
	ListElement *lst = usrsrc(wallet, username, MISSING);
	if (lst == NULL || lst->item == NULL) {
		return FAILED;
	}
	wallet->current_user = lst->item;
	return SUCCESS;
}

int
do_cmd(SimpleFS *fs, DirectoryHandle *dh, char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE], int tok_num, Wallet *wallet) {
	int i = 0, j, status = FAILED, res;
	char *argv[MAX_NUM_COMMAND];
	memset(argv, 0, MAX_NUM_TOK + 1);
	// Ottengo i comandi con i loro argomenti
	make_argv(argv, tok_buf, tok_num);
	
	//se argv[0]='sudo' eseguiamo il comando come superuser
	//In questa versione assiumiamo che tutti gli utenti siano sudoers
	User *user = NULL;
	if (argv[i] != NULL && strcmp(argv[i], "sudo") == 0) {
		//salviamo l'utente attuale
		user = wallet->current_user;
		//sostituiamo all'utente attuale l'utente root
		wallet->current_user = wallet->user_list->first->item;
		i = 1;
	}
	// Iterazioni che scorrono tutti i possibili comandi dati
	for (; argv[i] != NULL; i++) {
		//we use && to concatenate commands
		if (argv[i] != NULL && strcmp(argv[i], "&&") == 0) {
			i++;
		}
		if (argv[i] != NULL && strcmp(argv[i], "help") == 0) {
			printf("\n[progettoSO_shell] HELP\n"
			       "I comandi che puoi dare in questa semplice shell sono:\n"
			       "\t- help per mostrare questo messaggio\n"
			       "\t- puoi usare && per concatenare più comandi"
			       "\t- ls per mostrare l'elenco dei file contenuti nella working directory\n"
			       "\t- info per avere informazioni sul disco\n"
			       "\t- info -bmap per avere informazioni sulla bitmap del disco\n"
			       "\t- touch [nome file] per creare un file vuoto\n"
			       "\t- echo [-a] [contenuto] > [file] per scrivere un messaggio su un file (sovrascive il contenuto se"
			       "non viene usata l'opzione -a)\n"
			       "\t- cat [files] per concatenare i file e scriverli sullo schermo\n"
			       "\t- mkdir [directory] per creare una nuova directory\n"
			       "\t- cd [directory] per cambiare la directory di lavoro\n"
			       "\t- rm [file/directory] per rimuovere un file o una directory,"
			       " rimuove una directory in maniera ricorsiva se questa non e' vuota\n"
			       "\t- cp per copiare un file da o verso SimpleFs\n"
			       "\t usando sudo puoi eseguire i comandi come utente root\n"
			       "\t- chmod [nome elemento] [utente] [gruppo] [altri] per modificare i permessi di un elemento"
			       "usando la notazione ottale\n"
			       "\t- chown [nome elemento] [nome nuovo possessore] per cambiare il possessore di un elemento\n"
			       "\t- useradd [username] per aggiungere un utente\n"
			       "\t- userdel [usename] per eliminare un utente\n"
			       "\t- groupadd [groupname] per aggiungere un gruppo\n"
			       "\t- groupdel [groupname] per eliminare un gruppo\n"
			       "\t- su [username] per cambiare utente\n"
			       "\t- gpasswd -[ad] [groupname] [username] per aggiungere (-a) o rimuovere (-d) un utente da un gruppo\n\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "quit") == 0) {
			shell_shutdown(fs, dh, wallet);
			free(argv[i]);
			return QUIT;
		}
		if (argv[i] != NULL && strcmp(argv[i], "ls") == 0) {
			status = SUCCESS;
			char *names;
			if (argv[i + 1] != NULL && strcmp(argv[i + 1], "-l") == 0) {
				res = shell_readDir_perms(&names, dh, wallet);
				i++;
			} else {
				res = shell_readDir(&names, dh, wallet);
			}
			if (res != FAILED) {
				printf("\n%s\n", names);
				free(names);
			} else printf("Errore nell'esecuzione di ls\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "cd") == 0) {
			status = SUCCESS;
			for (j = i + 1; argv[j] != NULL && strcmp(argv[j], "&&") != 0 && strcmp(argv[j], "\0") != 0; j++) {
				res = shell_changeDir(dh, argv[j], wallet);
				if (res == FAILED) printf("Errore nell'esecuzione di cd\n");
				if (res == PERM_ERR) printf("permessi insufficienti\n");
			}
			i = j;
		}
		if (argv[i] != NULL && strcmp(argv[i], "mkdir") == 0) {
			status = SUCCESS;
			for (j = i + 1; argv[j] != NULL && strcmp(argv[j], "&&") != 0 && strcmp(argv[j], "\0") != 0; j++) {
				res = shell_mkDir(dh, argv[j], wallet);
				if (res == FAILED) printf("Errore nell'esecuzione di mkdir\n");
				if (res == PERM_ERR) printf("permessi insufficienti\n");
			}
			i = j;
		}
		if (argv[i] != NULL && strcmp(argv[i], "rm") == 0) {
			status = SUCCESS;
			for (j = i + 1; argv[j] != NULL && strcmp(argv[j], "&&") != 0 && strcmp(argv[j], "\0") != 0; j++) {
				res = shell_removeFile(dh, argv[j], wallet);
				if (res == FAILED) printf("Errore nell'esecuzione di rm su file/directory: %s\n", argv[j]);
				if (res == PERM_ERR) printf("permessi insufficienti\n");
			}
			i = j;
		}
		if (argv[i] != NULL && strcmp(argv[i], "touch") == 0) {
			status = SUCCESS;
			for (j = i + 1; argv[j] != NULL && strcmp(argv[j], "&&") != 0 && strcmp(argv[j], "\0") != 0; j++) {
				FileHandle *fh = shell_createFile(dh, argv[j], wallet);
				if (fh == NULL) printf("Errore nell'esecuzione di touch su file: %s\n", argv[j]);
				else shell_closeFile(fh);
			}
			i = j;
		}
		if (argv[i] != NULL && strcmp(argv[i], "cat") == 0) {
			status = SUCCESS;
			i += do_cat(dh, argv, i + 1, wallet);
		}
		if (argv[i] != NULL && strcmp(argv[i], "cp") == 0) {
			status = SUCCESS;
			i += do_copy_file(dh, argv, i + 1, wallet);
		}
		if (argv[i] != NULL && strcmp(argv[i], "info") == 0) {
			status = SUCCESS;
			if (argv[i + 1] != NULL && strcmp(argv[i + 1], "-bmap") == 0) {
				bitmap_info(fs->disk);
				i++;
			} else {
				shell_info(fs->disk);
			}
		}
		if (argv[i] != NULL && strcmp(argv[i], "echo") == 0) {
			status = SUCCESS;
			if (argv[i + 1] != NULL && strcmp(argv[i + 1], "-a") == 0) {
				res = echo(argv, dh, i + 2, wallet, APPEND);
			} else {
				res = echo(argv, dh, i + 1, wallet, OVERWRITE);
			}
			i = res;
			if (res == FAILED) printf("Errore nell'esecuzione echo\n");
			if (res == PERM_ERR) printf("permessi insufficienti\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "chmod") == 0) {
			status = SUCCESS;
			res = shell_chmod(dh, argv[i + 1], atoi(argv[i + 2]), atoi(argv[i + 3]), atoi(argv[i + 4]), wallet);
			i += 4;
			if (res == FAILED) printf("Errore nell'esecuzione di chmod\n");
			if (res == PERM_ERR) printf("permessi insufficienti\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "chown") == 0) {
			status = SUCCESS;
			res = shell_chown(dh, argv[i + 1], argv[i + 2], wallet);
			i += 2;
			if (res == FAILED) printf("Errore nell'esecuzione di chown\n");
			if (res == PERM_ERR) printf("permessi insufficienti\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "useradd") == 0) {
			status = SUCCESS;
			res = useradd(argv[i + 1], dh, wallet);
			i++;
			if (res == FAILED) printf("Errore nell'esecuzione di useradd\n");
			if (res == PERM_ERR) printf("permessi insufficienti\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "userdel") == 0) {
			status = SUCCESS;
			res = userdel(argv[i + 1], dh, wallet);
			i++;
			if (res == FAILED) printf("Errore nell'esecuzione di userdel\n");
			if (res == PERM_ERR) printf("permessi insufficienti\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "groupadd") == 0) {
			status = SUCCESS;
			res = groupadd(argv[i + 1], wallet);
			i++;
			if (res == FAILED) printf("Errore nell'esecuzione di groupadd\n");
			if (res == PERM_ERR) printf("permessi insufficienti\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "groupdel") == 0) {
			status = SUCCESS;
			res = groupdel(argv[i + 1], wallet);
			i++;
			if (res == FAILED) printf("Errore nell'esecuzione di groupdel\n");
			if (res == PERM_ERR) printf("permessi insufficienti\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "su") == 0) {
			status = SUCCESS;
			res = do_su(argv[i + 1], wallet);
			i++;
			if (res == FAILED) printf("Errore nell'esecuzione di su\n");
			if (res == PERM_ERR) printf("permessi insufficienti\n");
		}
		if (argv[i] != NULL && strcmp(argv[i], "gpasswd") == 0) {
			status = SUCCESS;
			if (strcmp(argv[i + 1], "-a") == 0) {
				res = gpasswd(argv[i + 2], argv[i + 3], wallet, ADD);
			} else {
				if (strcmp(argv[i + 1], "-d") == 0) {
					res = gpasswd(argv[i + 2], argv[i + 3], wallet, REMOVE);
				} else {
					res = FAILED;
				}
			}
			i += 3;
			if (res == PERM_ERR) printf("permessi insufficienti\n");
			if (res == FAILED) printf("Errore nell'esecuzione di gpasswd\n");
		}
	}
	//non è stato eseguito nessun comando
	if (status == FAILED) {
		printf("comando non trovato\n");
	}
	// Libero la memoria dai vari comandi
	for (i = 0; argv[i] != NULL; i++) if (strcmp(argv[i], "\0") != 0) free(argv[i]);
	
	//se è stato usato sudo ripristiniamo l'utente attuale
	if (user != NULL) {
		wallet->current_user = user;
	}
	return AGAIN;
}

int shell_login(Wallet *wallet, DirectoryHandle *dh) {
	
	int res;
	//we check if we have valid data
	if (wallet == NULL) {
		return FAILED;
	}
	
	char *user_buf = NULL;
	//we check if we must create a new user
	if (wallet->user_list->first->next == NULL) {
		//the first user is always root, so in this case we need to create the a user
		printf("Non è presente nessun utente, crea un utente\n");
		res = FAILED;
		while (res == FAILED || res > NAME_LENGTH) {
			printf("username:");
			res = scanf("%ms", &user_buf);
			//to get the /n left
			getchar();
			if (res <= 0 && errno != 0) {
				return FAILED;
			}
			if (res > NAME_LENGTH) {
				printf("il nome utente e' troppo lungo,riprova\n");
			}
		}
		res = useradd(user_buf, dh, wallet);
		if (res == FAILED || res == PERM_ERR) {
			return res;
		}
		//we log with the added user
		res = do_su(user_buf, wallet);
		if (res == FAILED || res == PERM_ERR) {
			return res;
		}
	} else {
		printf("\t\tLogin\n");
		res = FAILED;
		while (res == FAILED || res > NAME_LENGTH) {
			printf("username:");
			res = scanf("%ms", &user_buf);
			//to get the /n left
			getchar();
			if (res <= 0 && errno != 0) {
				return FAILED;
			}
			if (res > NAME_LENGTH) {
				printf("il nome utente e' troppo lungo,riprova\n");
			} else {
				//we log with the chosen user
				res = do_su(user_buf, wallet);
				if (res == PERM_ERR) {
					return res;
				}
				if (res == FAILED) {
					printf("l'utente %s non esiste,riprova\n", user_buf);
				}
			}
			
		}
	}
	res = SUCCESS;
	//we move into the user home directory
	//we go back to the root directory
	while (dh->directory != NULL && res != FAILED && res != PERM_ERR) {
		res = shell_changeDir(dh, "..", wallet);
	}
	//we go to the user home directory
	if (wallet->current_user->uid != ROOT) {
		res = shell_changeDir(dh, "home", wallet);
		if (res == FAILED || res == PERM_ERR) {
			return res;
		}
		res = shell_changeDir(dh, user_buf, wallet);
	} else {
		res = shell_changeDir(dh, "root", wallet);
	}
	free(user_buf);
	return res;
}

int main(int arg, char **args) {
	//dichiarazione varibili utili
	int res;
	//questa variabile indica la funzione attualmente selezionata dall'utente
	int control = 1;
	SimpleFS *fs = (SimpleFS *) malloc(sizeof(SimpleFS));
	DirectoryHandle *dh = NULL;
	
	//caricamento/inizializzazione del disco as the root user
	res = shell_init(fs);
	if (res == FAILED) shell_shutdown(fs, dh, NULL);
	CHECK_ERR(res == FAILED, "can't initialize the disk(1)");
	dh = fs_init(fs, fs->disk, ROOT, ROOT);
	if (dh == NULL) shell_shutdown(fs, dh, NULL);
	CHECK_ERR(dh == NULL, "can't initialize the disk(2)");
	
	//we initialize the wallet which will contain all the info un the user subsystem
	Wallet *wallet = initialize_wallet(dh);
	
	/*after having initialized the wallet we check if we let the user authenticate
	or we create the first user in case only the root user is present*/
	res = shell_login(wallet, dh);
	CHECK_ERR(res == FAILED || res == PERM_ERR, "can't login/create user!")
	
	//per ottenere i comandi inseriti dall'utente
	char command[1024];
	char tok_buf[MAX_NUM_TOK][MAX_COMMAND_SIZE];
	int tok_num = 0;
	char *ret;
	
	while (control) {
		printf("[%s@SimpleFS_shell:%s]>", wallet->current_user->account, dh->dcb->fcb.name);
		
		//lettura dei comandi da tastiera
		ret = fgets(command, MAX_COMMAND_SIZE, stdin);
		if (ret == NULL) shell_shutdown(fs, dh, wallet);
		CHECK_ERR(ret == NULL, "can't read command from input");
		
		if (strchr(command, ';') != NULL) parse_command(command, ";", tok_buf, &tok_num);
		else parse_command(command, "\n", tok_buf, &tok_num);
		
		// tok_num nullo allora ricomincio
		if (tok_num <= 0) continue;
		
		// Chiamo do_cmd che esegue i comandi
		control = do_cmd(fs, dh, tok_buf, tok_num, wallet);
		
		memset(command, 0, MAX_COMMAND_SIZE);
		tok_num = 0;
	}
	
	return 0;
}
