#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/ipc.h>

/* zmienne globalne dotyczace pamieci wspoldzielonej*/
key_t klucz_serwera;
int	id_serwera;
int maks_wiad;

/* zmienne dla semafor*/

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} arg;

key_t klucz_semafor;
int id_semafor;

/*nasza pamiec wspoldzielona*/
struct twit{
	char nazwa_uzytkownika[32];
	char posty[512];
	int polubienia;
	int ilosc;
	int zalogowani;
}* tw;


void utworzenie_pamieci(char** av)
{
	int pamiec;
	/* w specyfikacji nie bylo podane ktory argument ma byc liczba a ktory iloscia wiadomosci
	 * wiec zakladamy ze zgodnie z podana kolejnoscia w poleceniu*/
	if((maks_wiad = atoi(av[2])) <= 0)
	{
		fprintf(stderr, "nieprawidlowy rozmiar serwera\n");
		exit(EXIT_FAILURE);
	}
	
	/*tworzenie klucza serwera*/
	if((klucz_serwera = ftok(av[1], 'A')) < 0)
	{
		perror("blad ftok");
		exit(EXIT_FAILURE);
	}
	printf("utworzono klucz\n");
	
	
	/*usuwanie pamieci wspoldzielonej jesli takowa istnieje*/
	if((pamiec = shmget(klucz_serwera, 0, 0)) >= 0)
	{
		if (shmctl(pamiec, IPC_RMID, NULL) != 0) 
		{
        perror("blad shmctl");
        exit(EXIT_FAILURE);
		}
	}
	
	/*tworzenie pamieci wspoldzielonej*/
	if((id_serwera = shmget(klucz_serwera, maks_wiad, 0700 | IPC_CREAT | IPC_EXCL)) < 0)
	{
		perror("blad shmget");
		exit(EXIT_FAILURE);
	}
	
	/*deklaracja pamieci wspoldzielonej */
	if((tw = (struct twit*)shmat(id_serwera, (struct twit*)0, 0)) == (struct twit *) -1)
	{
		perror("blad shmat twit");
		exit(EXIT_FAILURE);
	}
	tw->ilosc = 0;
	tw->zalogowani = 0;
	printf("utworzono pamiec wspoldzielona\n");
	
}

void utworzenie_semafor(char** av)
{
	int i;
	/*utworzenie klucza semafor*/
	if((klucz_semafor = ftok(av[1], 'S')) < 0)
	{
		perror("blad ftok sem");
		exit(EXIT_FAILURE);
	}
	/*utworzenie semafora*/
	if((id_semafor = semget(klucz_semafor, maks_wiad, IPC_CREAT | 0777)) < 0)
	{
		perror("blad semget");
		exit(EXIT_FAILURE);
	}
	
	/*nadanie wartosci semafor 1 oznacza ze jest "gotowy do uzycia"*/
	arg.val = 1;
	for(i = 0; i < maks_wiad; i++)
	{
		if((semctl(id_semafor, i, SETVAL, arg)) < 0)
		{
			perror("blad semctl SETVAL");
			exit(EXIT_FAILURE);
		}  
	}
	
}



void akcja_reakcja(int sygnal)
{	

	if(sygnal == SIGTSTP)
	{
		int i;
		int a;
		
		/*a = 0 jest tutaj dla sprawdzenia czy jest jakikolwiek post*/
		a = 0;
		printf("\n");
		for(i = 0; i < maks_wiad; i++)
		{
			if(tw[i].nazwa_uzytkownika != NULL || tw[i].posty != NULL)
			{	
				if(strcmp(tw[i].nazwa_uzytkownika, "") != 0)
				{
					printf("%d. uzytkownik: %s, post: %s, polubienia %d \n", i + 1, tw[i].nazwa_uzytkownika, tw[i].posty, tw[i].polubienia);
					a = 1;
				}
			}
		}
		if(a == 0)
		{
			printf("deadchat xddddd\n");
		}
	}
	
	if(sygnal == SIGINT)
	{
		/*odlaczanie pamieci*/
		
		if(shmdt(tw) < 0)
		{
			perror("\nblad shmdt");
			exit(EXIT_FAILURE);
		}
		printf("\nusunieto posty\n");
		/*usuwanie pamieci wspoldzielonej*/
		if(shmctl(id_serwera, IPC_RMID, 0) < 0)
		{
			perror("\nblad shmctl, IPC_RMID");
			exit(EXIT_FAILURE);
		}
		printf("usunieto serwer\n");
		
		/*usuwanie semafor*/
		if(semctl(id_semafor, 0, IPC_RMID, arg) < 0)
		{
			perror("\nblad usuniecia semafor");
			exit(EXIT_FAILURE);
		}
		printf("usunieto semafora\n");
		printf("bedziemy za toba tesknic :c\n");
		exit(EXIT_SUCCESS);
	}
}

int main(int argc, char **argv)
{	
	int maks_wiad;

	if(argc != 3)
	{
		fprintf(stderr, "nieodpowiednia ilosc argumentow\n");
		exit(EXIT_FAILURE);
	}
	if((maks_wiad = atoi(argv[2])) < 0)
	{
		fprintf(stderr, "nieprawidlowy rozmiar serwera\n");
		exit(EXIT_FAILURE);
	}
	
	utworzenie_pamieci(argv);
	utworzenie_semafor(argv); 
	
	printf("witamy na twiter!!!\n");
	printf("wybierz co chcesz zrobic:\n");
	printf("SIGTSTP - wyswietlenie wszystkich postow\n");
	printf("SIGINT - zakonczenie dzialania serwera\n");
	/* obsluga sygnalow*/
	if((signal(SIGTSTP, akcja_reakcja)) == SIG_ERR || signal(SIGINT, akcja_reakcja) == SIG_ERR)
	{
		perror("blad signal");
		exit(EXIT_FAILURE);
	}
	
	/*oczekiwanie na wpisy*/
	while(1)
	{
		pause();
	}
	exit(EXIT_SUCCESS);

	return 0;
}
