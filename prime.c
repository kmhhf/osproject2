#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(int argc, char* argv[])
{
    //struct for the simulated clock
    struct clock
    {
        int second;
        int nanosecond;
    };

    struct clock* sharedClock = NULL;
    int totalChildren = atoi(argv[3]);
    int timer1;
    int timer2;

    key_t sharedClockKey = ftok("oss", 1);                                  //get the key for the clock shared mem
    if(sharedClockKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    int clockShmid = shmget(sharedClockKey, sizeof(struct clock), 0666);
    if(clockShmid == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    sharedClock =  shmat(clockShmid, NULL, SHM_RDONLY);                     //attach to shared mem in read only mode
    if(sharedClock == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    key_t sharedPrimeKey = ftok("oss", 2);                                          //get the key for the results shared mem
    if(sharedPrimeKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    int primeShmid = shmget(sharedPrimeKey, sizeof(int) * totalChildren, 0666);
    if(primeShmid == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    int* sharedPrime = (int*) shmat(primeShmid, NULL, 0);                           //connect to the results shared mem
    if(sharedPrime == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        shmctl(primeShmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    int index = atoi(argv[1]);
    int isPrime = atoi(argv[2]);
    timer1 = sharedClock->nanosecond;

    int check = 0;
    int i;

    //check if the number is prime and put the result in shared memory
    for (i = 2; i <= (isPrime / 2); i++)
    {
        timer2 = sharedClock->nanosecond;
        if(timer2 > (timer1 + 1000000))                 //check for 1 milisecond and exit if that time has pasted
        {
            sharedPrime[index] = -1;
            shmdt(sharedClock);
            shmdt(sharedPrime);
            exit(EXIT_FAILURE);
        }
        if ((isPrime % i) == 0)
        {
            check = 1;
            break;
        }
    }

    if (check == 0)
    {
        sharedPrime[index] = isPrime;
    }
    else
    {
        sharedPrime[index] = isPrime - (isPrime * 2);
    }

    shmdt(sharedClock);
    shmdt(sharedPrime);

    return 0;
}
