//inserisco nel processo figlio il comando da eseguire 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LEN_BUFFER 32 // dimensione dell'array 'comando'

int main(int argc, char *argv[])
{
	char comando[LEN_BUFFER];
	int pid;

	while (1)
	{
		printf("myShell(scrivi 'termina' per uscire): ");   
		scanf("%s", comando);                                   //viene catturato ciò che viene scritto 

		if (strcmp(comando, "termina") == 0)	break;

		pid = fork();

		// errore
		if (pid == -1)
		{
			printf("Errore nella fork()");
			exit(1);
		}

		//figlio->esegue il comando
		if (pid == 0){
			execlp(comando, comando, NULL);

			// se il comando appena eseguito termina con successo non arriveremo mai a questo punto, in caso contrario segnaliamo errore
			printf("Errore nell'esecuzione di %s", comando);
			exit(2);
		}
		else{
            //padre->aspetta figlio
            wait(NULL);
        }
	}
	exit(0);
}