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

void utworzPamiecDzielona();
void dolaczPamiecDzielona();
void usunPamiecDzielona();
void dolaczSemafor();
void ustawSemafor();
void zamknijSemafory();
void obsluga(int);

int mId;
key_t mKey;
int sId;
key_t sKey;
long *pamiecDzielona;
int *PIDyPodprocesow;
long liczbaPisarzy, liczbaCzytelnikow;

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        perror("zla wartosc argumentow wywolania\n");
        exit(1);
    }

    char *strtolPtr;
    liczbaPisarzy = strtol(argv[1], &strtolPtr, 10);
    if (errno == ERANGE || *strtolPtr != '\0')
    {
        perror("strtol error");
        exit(-1);
    }

    liczbaCzytelnikow = strtol(argv[2], &strtolPtr, 10);
    if (errno == ERANGE || *strtolPtr != '\0')
    {
        perror("strtol error");
        exit(-1);
    }

    long ilosc_miejsc = strtol(argv[3], &strtolPtr, 10);
    if (errno == ERANGE || *strtolPtr != '\0')
    {
        perror("strtol error");
        exit(-1);
    }

    if (liczbaPisarzy < 1 || liczbaCzytelnikow < 1 || ilosc_miejsc < 1)
    {
        perror("wrong value of the arguments\n");
        exit(-1);
    }

    FILE *pd;
    int procesyUzytkownika;
    pd = popen("ps ux | wc -l", "r");

    if (pd == NULL)
    {
        perror("popen error");
        exit(-1);
    }

    fscanf(pd, "%d", &procesyUzytkownika);

    if (pclose(pd) == -1)
    {
        perror("pclose error");
        exit(-1);
    }

    int limitProcesow;
    pd = popen("ulimit -p", "r");

    if (pd == NULL)
    {
        perror("popen error");
        exit(-1);
    }

    fscanf(pd, "%d", &limitProcesow);

    if (pclose(pd) == -1)
    {
        perror("pclose error");
        exit(-1);
    }

    if (liczbaPisarzy + liczbaCzytelnikow + procesyUzytkownika > limitProcesow + 4)
    {
        perror("exceeded processes limit\n");
        exit(-1);
    }

    signal(SIGINT, obsluga);
    utworzPamiecDzielona();
    dolaczPamiecDzielona();

    pamiecDzielona[0] = ilosc_miejsc; // podana w argv[3]
    pamiecDzielona[1] = 0; // licznik czytelnikow
                         //pamiecDzielona[2] = bufor

    dolaczSemafor();
    ustawSemafor();

    PIDyPodprocesow = malloc((liczbaPisarzy + liczbaCzytelnikow) * sizeof(int));
    for (long i = 0; i < liczbaPisarzy; i++)
    {
        switch (PIDyPodprocesow[i] = fork())
        {
        case -1:
        {
            perror("blad fork pisarza\n");
            exit(-1);
        }
        case 0:
        {
            if (execl("./pisarz", "pisarz", NULL) == -1)
            {
                perror("writers execl error\n");
                exit(-1);
            }
        }
        }
    }

    for (long i = 0; i < liczbaCzytelnikow; i++)
    {
        switch (PIDyPodprocesow[liczbaPisarzy + i] = fork())
        {
        case -1:
        {
            perror("blad fork czytelnika\n");
            exit(-2);
        }
        case 0:
        {
            if (execl("./czytelnik", "czytelnik", NULL) == -1)
            {
                perror("blad execl czytelnika\n");
                exit(-2);
            }
        }
        }
    }

    int stat, x;
    while (x = wait(&stat) > 0)
    {
        if (x == -1)
        {
            perror("wait error");
            obsluga(-1);
            exit(-1);
        }
    }

    exit(0);
}

void utworzPamiecDzielona()
{
    mKey = ftok(".", 0);
    if (mKey == -1)
    {
        perror("\nsharedMemoryory ftok error\n");
        exit(-1);
    }
    mId = shmget(mKey, 2 * sizeof(long int) + 1, 0600 | IPC_CREAT);
    if (mId == -1)
    {
        perror("shmget error\n");
        exit(-1);
    }
}

void dolaczPamiecDzielona()
{
    pamiecDzielona = shmat(mId, 0, 0);
    if (*pamiecDzielona == -1 && errno != 0)
    {
        perror("shmat error\n");
        exit(-3);
    }
}

void usunPamiecDzielona()
{
    if (shmctl(mId, IPC_RMID, 0) == -1 || shmdt(pamiecDzielona) == -1)
    {
        perror("close sharedMemoryory error\n");
        exit(-1);
    }
}

void dolaczSemafor()
{
    sKey = ftok(".", 0);
    if (sKey == -1)
    {
        perror("semaphors ftok error\n");
        exit(-1);
    }
    sId = semget(sKey, 2, IPC_CREAT | 0600);
    if (sKey == -1)
    {
        perror("semget error\n");
        exit(-2);
    }
}

void ustawSemafor()
{
    int set = 0;
    for (int i = 0; i < 2; i++)
    {
        set = semctl(sId, i, SETVAL, 1);
        if (set == -1)
        {
            perror("setsem semctl error\n");
            exit(-1);
        }
    }
}

void zamknijSemafory(void)
{
    if (semctl(sId, 0, IPC_RMID) == -1)
    {
        perror("semctl error\n");
        exit(-1);
    }
}

void obsluga(int sigNum)
{
    int x, pid;
    for (int i = 0; i < liczbaPisarzy + liczbaCzytelnikow; i++)
    {
        if(PIDyPodprocesow[i] != -1)
        {
            kill(PIDyPodprocesow[i], SIGINT);
        }
    }

    zamknijSemafory();
    usunPamiecDzielona();

    while (wait(&x) > 0)
        ;

    exit(sigNum);
}