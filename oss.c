#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

// Simple struct for simulated clock
struct clock
{
    int second;
    int nanosecond;
};

int* sharedPrime;
int clockShmid;
int primeShmid;

struct clock* sharedClock = NULL;

// Signal handler for the alarm
void timer_handler(int signum)
{
    fprintf(stderr, "Error: Program timed out.");
    shmdt(sharedClock);
    shmdt(sharedPrime);
    shmctl(clockShmid, IPC_RMID, NULL);
    shmctl(primeShmid, IPC_RMID, NULL);
    kill(0, SIGKILL);
}

// CTRL C handler
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
    int childPid;
    int totalChildren = 4;
    int activeChildren = 2;
    int startNumber = 1;
    int incrementBy = 1;
    int totalProcesses = 0;
    int totalActive = 0;
    char outputFile[255] = "primeoutput.txt";
    FILE *output = NULL;
    char text[255];

    // Calls to signal to catch the signals
    signal(SIGALRM, timer_handler);
    signal(SIGINT, ctrlc_handler);

    //Normal getopt option handling
    while ((opt = getopt(argc, argv, "hn:s:b:i:o:")) != -1)
    {
        switch(opt)
        {
            case 'h':
                printf("Usage: %s [-h] [-n max children] [-s simultaneous children] [-b start number]"
                       " [-i increment] [-o output]\n", argv[0]);
                printf("        -h   Display usage message and options\n");
                printf("        -n   Set maximum child processes to be created\n");
                printf("        -s   Set maximum simultaneous child processes\n");
                printf("        -b   Start number for checking primality\n");
                printf("        -i   Increment between numbers tested\n");
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

    // Set the alarm or two seconds so that a signal is sent.
    alarm(2);

    // Open file to write and error checking
    output = fopen(outputFile, "w");
    if(output == NULL)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    key_t sharedClockKey = ftok("oss", 1);                                          //get key for shared mem for clock
    if(sharedClockKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    clockShmid = shmget(sharedClockKey, sizeof(struct clock), 0666|IPC_CREAT);      //create the shared mem for clock
    if(clockShmid == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    sharedClock =  shmat(clockShmid, NULL, 0);                                      //attach to shared mem for clock
    if(sharedClock == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        shmctl(clockShmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    key_t sharedPrimeKey = ftok("oss", 2);                                          //get key for shared mem for results
    if(sharedPrimeKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    primeShmid = shmget(sharedPrimeKey, sizeof(int) * totalChildren, 0666|IPC_CREAT);  //create the shared mem for results
    if(primeShmid == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    sharedPrime = (int*) shmat(primeShmid, NULL, 0);                                //attach to shared mem for results
    if(sharedPrime == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        shmctl(primeShmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    while(totalProcesses < totalChildren)                                           //loop runs until the total number of children created = the number of children requested
    {
        sharedClock->nanosecond += 10000;               //increment simulated clock
        if(sharedClock->nanosecond > 1000000000)
        {
            sharedClock->second = sharedClock->second + 1;
            sharedClock->nanosecond = sharedClock->nanosecond - 1000000000;
        }

        if(totalActive < activeChildren)                //limits number of active children
        {
            pid_t processPid = fork();              //fork off a new process

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

            //code for child
            if(processPid == 0)
            {
                char processNumber[12];
                char findPrime[12];
                char numOfChildren[12];
                sprintf(numOfChildren, "%d", totalChildren);
                sprintf(findPrime, "%d", startNumber);
                sprintf(processNumber, "%d", totalProcesses);
                execl("./prime", "prime", processNumber, findPrime, numOfChildren, NULL);       //pass arguements and exec
                fprintf(stderr, "%s: Error: execl failed.", argv[0]);
            }

            //write start time to file and increment number of processes
            fprintf(output, "Process %d started at %d seconds and %d nanoseconds\n", processPid, sharedClock->second, sharedClock->nanosecond);
            totalProcesses++;
            startNumber = (startNumber + incrementBy);     //increment the next number to test for prime
            totalActive++;
        }

        int i = 0;
        for(i = 0; i < activeChildren; i++)
        {
            childPid = waitpid(-1, NULL, WNOHANG);     //check if child has finished
            if(childPid > 0)
            {
                fprintf(output, "Process %d finished at %d seconds %d nanoseconds\n", childPid, sharedClock->second, sharedClock->nanosecond);  //write finish time of child
                totalActive--;
            }
        }
    }
    int i = 0;
    for(i = 0; i < activeChildren; i++)                     //wait for rest of the children and write finish time
    {
        childPid = wait();
        if(childPid > 0)
        {
            fprintf(output, "Process %d finished at %d seconds %d nanoseconds\n", childPid, sharedClock->second, sharedClock->nanosecond);
        }
    }

    for(i = 0; i < totalChildren; i++)              //format results and write them to file
    {
        if(sharedPrime[i] < -1)
        {
            fprintf(output, "%d is not prime\n", (sharedPrime[i] - (2 * sharedPrime[i])));
        }
        else if(sharedPrime[i] > 0)
        {
            fprintf(output, "%d is prime\n", sharedPrime[i]);
        }
        else
        {
            fprintf(output, "Child timed out\n");
        }
    }

    fflush(output);
    fclose(output);         //close file
    output = NULL;

    shmdt(sharedClock);
    shmdt(sharedPrime);

    shmctl(clockShmid, IPC_RMID, NULL);     //release the shared mem
    shmctl(primeShmid, IPC_RMID, NULL);

    return 0;
}
