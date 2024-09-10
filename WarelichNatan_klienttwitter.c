#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/ipc.h>

/*zmienne globalne z: kluczem serwera, id serwera oraz z structura shmid_id ktora
 * bedzie zawierac informacje dotyczace naszej pamieci wspoldzielonej*/
struct shmid_ds shm_status;
key_t klucz_serwera;
int id_serwera;

/*to jest nasza pamiec dzielona*/
struct twit
{
	char nazwa_uzytkownika[32];
	char posty[512];
	int polubienia;
	int ilosc;
	int zalogowani;
}*tw;

/* zmienne dla semafor*/

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} arg;

struct sembuf sb;
key_t klucz_semafor;
int id_semafor;


void utworzenie_pamieci(char**av)
{	

	/*tutaj analogicznie jak w serwerze najpierw nazwa pliku pamieci, potem nazwa uzytkownika*/
	
	/*utworzenie klucza*/
	if((klucz_serwera = ftok(av[1], 'A')) < 0)
	{
		perror("blad ftok");
		exit(EXIT_FAILURE);
	} 
	/*dolaczanie do serwera*/
	if((id_serwera = shmget(klucz_serwera, 0, 0)) < 0)
	{
		perror("blad shmget");
		exit(EXIT_FAILURE);
	}
	
	/*deklaracja pamieci wspoldzielonej*/
	
	if((tw = (struct twit*)shmat(id_serwera, (struct twit*)0, 0)) == (struct twit *) -1)
	{
		perror("blad shmat twit");
		exit(EXIT_FAILURE);
	}
	
	tw->zalogowani++;
	
	/*wydobywanie danych o pamieci wspoldzielonej ktore potrzebne bedzie potem np przy semaforach*/
	if(shmctl(id_serwera, IPC_STAT, &shm_status) < 0)
	{
	
		perror("blad shmctl, IPC_STAT");
		exit(EXIT_FAILURE);
	}	
}	
	
void utworzenie_semafor(char** av)
{
		
	if((klucz_semafor = ftok(av[1], 'S')) < 0)
	{
		perror("blad ftok sem");
		exit(EXIT_FAILURE);
	}
	if((id_semafor = semget(klucz_semafor, shm_status.shm_segsz, IPC_CREAT | 0700)) < 0)
	{
		perror("blad semget");
		exit(EXIT_FAILURE);
	}
	
	
	sb.sem_op = -1; 
	/*flaga semundo w razie gdyby proces klienta zostal przerwany przy uzyciu np sygnalow
	 * cofnie status semafora*/
	sb.sem_flg = SEM_UNDO;
	sb.sem_num = tw->zalogowani % shm_status.shm_segsz;
	
	if (semop(id_semafor, &sb, 1) == -1) {
        perror("blad semop");
        exit(EXIT_FAILURE);
    }
}

void info(){
	
	long unsigned int i;
	printf("\nstatus twitera\n");
	printf("ilosc postow: %d\n", tw->ilosc);
	printf("ilosc wolnych slotow na serwerze to: %lu\n", shm_status.shm_segsz - tw->ilosc);
	if(tw->ilosc != 0)
	{
		printf("\n");
			for(i = 0; i < shm_status.shm_segsz; i++)
			{
				if(tw[i].nazwa_uzytkownika != NULL || tw[i].posty != NULL)
				{
					if(strcmp(tw[i].nazwa_uzytkownika, "") != 0)
					{
						printf("%lu. post: %s, uzytkownik: %s, polubienia: %d\n", i + 1, tw[i].posty, tw[i].nazwa_uzytkownika, tw[i].polubienia);
					}
				}
			}
	}
	else printf("deadchat xddddddd\n");
}

int main(int argc, char **argv)
{
	char wpis[512];
	int akcja;
	long unsigned int lajk;
	
	if(argc != 3)
	{
		fprintf(stderr, "nieodpowiednia ilosc argumentow\n");
		exit(EXIT_FAILURE);
	}
	if(strlen(argv[2]) > 32)
	{
		fprintf(stderr, "podano zbyt dluga nazwe uzytkownika\n");
		exit(EXIT_FAILURE);
	}
	
	utworzenie_pamieci(argv);
	utworzenie_semafor(argv);
	
	
	while(1){
		
		info();
		printf("\nco chcesz zrobic?\n(1) wstawic nowego twita\n(2) polubic wpis\n");
		scanf("%d", &akcja);
		
		if(akcja == 1)
		{
			/*tutaj najpierw pobieramy przez shmctl ilosc wolnych "slotow" a nastepnie
			 * sprawdzamy czy mamy jeszcze miejsce na wpis*/
			if(shm_status.shm_segsz - tw->ilosc <= 0){
				printf("serwer jest przepelniony, koncze prace\n");
				break;
			}
			else
			{
				printf("co chcesz napisac?\n");
				getchar(); /*usuwanie znaku konca lini z scanf ktory bylby pobierany przez fgets*/
				fgets(wpis, sizeof(wpis), stdin);
				
				while(strcmp(wpis, "\n") == 0 || strlen(wpis) > 512)
				{
					printf("twoj wpis jest nieprawidlowy, powinien on: nie byc pusty oraz zawierac mniej znakow niz 512\n");
					if(scanf("%s", wpis) < 0)
					{
						perror("blad scanf");
						exit(EXIT_FAILURE);
					}	
				}
				
				wpis[strcspn(wpis, "\n")] = '\0';  	/*usuwanie znaku konca lini*/
				strcpy(tw[tw->ilosc].posty, wpis);
				strcpy(tw[tw->ilosc].nazwa_uzytkownika, argv[2]);
				tw->ilosc+= 1;
				printf("\n");
			}
		}
		else if(akcja == 2)
		{
			/*WAZNE!, musimy okreslic czy uzytkownik podal odpowiedni numer postu do polubienia.
			 * Najlepiej bedzie to zrealizowac przez sprawdzenie czy podana przez niego liczba
			 * jest wieksza od shm_status.shm_segsz oraz czy nie jest wieksza od zmiennej ilosc_postow
			 * oraz czy zmienna lajk nie jest rowna zero*/
			 
			printf("podaj numer wpisu ktory chcesz polubic\n");
			if(scanf("%lu", &lajk) < 0)
			{
				perror("blad scanf");
				exit(EXIT_FAILURE);
			}
			if(lajk > shm_status.shm_segsz || lajk > (long unsigned int)tw->ilosc || lajk == 0)
			{
				printf("nieprawidlowy numer posta, koncze prace\n");
				break;
			}
			else
			{
				tw[lajk - 1].polubienia = tw[lajk - 1].polubienia + 1;
			}
			
		}
		else printf("nieladnie tak\n");
	}
	
	/*odlaczanie semafora*/
	sb.sem_op = 1;
	
	if (semop(id_semafor, &sb, 1) == -1) {
        perror("blad semop");
        exit(EXIT_FAILURE);
    }

	/*odlaczenie od pamieci wspoldzielonej*/
	if(shmdt(tw) < 0)
	{
		perror("blad shmdt");
		exit(EXIT_FAILURE);
	}
	return 0;
}

