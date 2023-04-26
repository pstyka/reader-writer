#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/shm.h>

#define W1 0
#define W3 2
#define SEMAFOR_PISARZA 3
#define SEMAFOR_CZYTELNIKA 4

void utworzPamiecDzielona();
void dolaczPamiecDzielona();
void usunPamiecDzielona();
void dolaczSemafor();
void zablokujSemafor(int);
void podniesSemafor(int);

int mId;
key_t mKey;
int sId;
key_t sKey;

long int *pamiecDzielona;

int main(int argc, char *argv[])
{
    dolaczSemafor();
    utworzPamiecDzielona();
    dolaczPamiecDzielona();

    while (1)
    {
        zablokujSemafor(W3); //  przedpokój dla czytelnika, zapewnia priorytet pisarza
        zablokujSemafor(SEMAFOR_CZYTELNIKA); // ochrona przed wejsciem do sekcji krytycznej czytajacych
        zablokujSemafor(W1);// max 1 inkrementuje zmienna i blokuje

        if (pamiecDzielona[1] < pamiecDzielona[0])
        {
            pamiecDzielona[1]++;
            if (pamiecDzielona[1] == 1)
            {
                zablokujSemafor(SEMAFOR_PISARZA);
            }

            podniesSemafor(W1);
            podniesSemafor(SEMAFOR_CZYTELNIKA);
            podniesSemafor(W3);

            printf("Czytelnik %d przeczytal: %c\t w bibliotece: %ld\n", getpid(), (char)pamiecDzielona[3], pamiecDzielona[1]);
           
            zablokujSemafor(W1);
            
            pamiecDzielona[1]--;
            if (pamiecDzielona[1] == 0)
            {
                podniesSemafor(SEMAFOR_PISARZA);
            }
        }
        podniesSemafor(W1);
    }
    usunPamiecDzielona();
    exit(0);
}

void utworzPamiecDzielona()
{
    mKey = ftok(".", 0);
    if (mKey == -1)
    {
        perror("reader ftok error\n");
        exit(-1);
    }
    mId = shmget(mKey, 2 * sizeof(long int) + 1, 0600 | IPC_CREAT);
    if (mId == -1)
    {
        perror("reader shmget error\n");
        exit(-3);
    }
}

void dolaczPamiecDzielona()
{
    pamiecDzielona = shmat(mId, 0, 0);
    if (*pamiecDzielona == -1 && errno != 0)
    {
        perror("reader shmat error\n");
        exit(-1);
    }
}

void usunPamiecDzielona()
{

    if (shmctl(mId, IPC_RMID, 0) || shmdt(pamiecDzielona))
    {
        perror("reader shmctl or smhdt error\n");
        exit(-1);
    }
}

void dolaczSemafor(void)
{
    sKey = ftok(".", 0);
    if (sKey == -1)
    {
        perror("reader ftok error\n");
        exit(-1);
    }

    sId = semget(sKey, 2, IPC_CREAT | 0200);
    if (sId == -1)
    {
        perror("reader semget error\n");
        exit(-2);
    }
}

void zablokujSemafor(int nr)
{
    int x;
    struct sembuf buforSem;
    buforSem.sem_num = nr;
    buforSem.sem_op = -1;
    buforSem.sem_flg = 0;

    if (semop(sId, &buforSem, 1))
    {
        if (errno == 4)
        {
            zablokujSemafor(nr);
        }
        else
        {
            perror("reader zablokujSemafor semop error\n");
            exit(-1);
        }
    }
}

void podniesSemafor(int nr)
{
    int x;
    struct sembuf buforSem;
    buforSem.sem_num = nr;
    buforSem.sem_op = 1;
    buforSem.sem_flg = 0;

    x = semop(sId, &buforSem, 1);
    if (x == -1)
    {
        if (errno == 4)
        {
            podniesSemafor(nr);
        }
        else
        {
            perror("reader podniesSemafor semop error\n");
            exit(-1);
        }
    }
}
