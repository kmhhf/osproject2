#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

struct clock
{
    int second;
    int nanosecond;
};

int* sharedPrime;
int clockShmid;
int primeShmid;

struct clock* sharedClock = NULL;


void timer_handler(int signum)
{
    fprintf(stderr, "Error: Program timed out.");
    shmdt(sharedClock);
    shmdt(sharedPrime);
    shmctl(clockShmid, IPC_RMID, NULL);
    shmctl(primeShmid, IPC_RMID, NULL);
    kill(0, SIGKILL);
}

void ctrlc_handler(int signum)
{
    shmdt(sharedClock);
    shmdt(sharedPrime);
    shmctl(clockShmid, IPC_RMID, NULL);
    shmctl(primeShmid, IPC_RMID, NULL);
    kill(0, SIGKILL);
}

int main(int argc, char* argv[])
{
    int opt;
    int totalChildren = 4;
    int activeChildren = 2;
    int startNumber = 1;
    int incrementBy = 1;
    int totalProcesses = 0;
    int totalActive = 0;
    char outputFile[255] = "primeoutput.txt";
    int pidList[activeChildren];

    signal(SIGALRM, timer_handler);
    signal(SIGINT, ctrlc_handler);

    while ((opt = getopt(argc, argv, "hn:s:b:i:o:")) != -1)
    {
        switch(opt)
        {
            case 'h':
                printf("Usage: %s [-h] [-n max children] [-s simultaneous children] [-b start number]"
                       " [-i increment] [-o output]\n", argv[0]);
                printf("        -h   Display usage message and options\n");
                printf("        -n   Set maximum child processes to be created\n");
                printf("        -s   Set mazimum simultaneous child processes\n");
                printf("        -b   Start number for checking primality\n");
                printf("        -i   Increment between numbers texted\n");
                printf("        -o   Output file name\n");
                exit(EXIT_FAILURE);
            case 'n':
                totalChildren = atoi(optarg);
                if (totalChildren < 1)
                {
                    fprintf(stderr, "%s: Error: You must have at least 1 child process\n", argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 's':
                activeChildren = atoi(optarg);
                if (activeChildren < 1)
                {
                    fprintf(stderr, "%s: Error: You must have at least 1 child process\n", argv[0]);
                    exit(EXIT_FAILURE);
                }
                else if(activeChildren > 19)
                {
                    fprintf(stderr, "%s: Error: You can not have more than 19 child processes at the same time\n", argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'b':
                startNumber = atoi(optarg);
                break;
            case 'i':
                incrementBy = atoi(optarg);
                break;
            case 'o':
                strncpy(outputFile, optarg, 255);
                break;
            default:

                fprintf(stderr, "Usage: %s [-h] [-n max children] [-s simultaneous children] [-b start number]"
                                " [-i increment] [-o output]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    alarm(120);

    key_t sharedClockKey = ftok("oss", 1);
    if(sharedClockKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    clockShmid = shmget(sharedClockKey, sizeof(struct clock), 0666|IPC_CREAT);
    if(clockShmid == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    sharedClock =  shmat(clockShmid, NULL, 0);
    if(sharedClock == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        shmctl(clockShmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    key_t sharedPrimeKey = ftok("oss", 2);
    if(sharedPrimeKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    primeShmid = shmget(sharedPrimeKey, sizeof(int) * totalChildren, 0666|IPC_CREAT);
    if(primeShmid == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    sharedPrime = (int*) shmat(primeShmid, NULL, 0);
    if(sharedPrime == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        shmctl(primeShmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    printf("%i\n%i\n", sharedClock->second, sharedClock->nanosecond);

    while(totalProcesses < totalChildren)
    {
        sharedClock->nanosecond += 10000;

        if(totalActive < activeChildren)
        {
            pid_t processPid = fork();

            if(processPid == -1)
            {
                fprintf(stderr, "%s: Error: ", argv[0]);
                perror("");
                shmdt(sharedClock);
                shmdt(sharedPrime);
                shmctl(clockShmid, IPC_RMID, NULL);
                shmctl(primeShmid, IPC_RMID, NULL);
                exit(EXIT_FAILURE);
            }

            if(processPid == 0)
            {
                execl("./prime", NULL);
                fprintf(stderr, "%s: Error: execl failed.", argv[0]);
            }
            pidList[totalActive] = processPid;
            totalProcesses++;
            totalActive++;
        }

        int i = 0;
        for(i = 0; i < activeChildren; i++)
        {
            if (waitpid(-1, NULL, WNOHANG) > 0)
            {
                totalActive--;
            }
        }
    }
    int i = 0;
    for(i = 0; i < activeChildren; i++)
    {
        wait();
    }

    shmdt(sharedClock);
    shmdt(sharedPrime);

    shmctl(clockShmid, IPC_RMID, NULL);
    shmctl(primeShmid, IPC_RMID, NULL);

    return 0;
}