#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(int argc, char* argv[])
{
    struct clock
    {
        int second;
        int nanosecond;
    };

    struct clock* sharedClock = NULL;

    key_t sharedClockKey = ftok("oss", 1);
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

    sharedClock =  shmat(clockShmid, NULL, SHM_RDONLY);
    if(sharedClock == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    printf("%i\n%i\n", sharedClock->second, sharedClock->nanosecond);

    shmdt(sharedClock);

    return 0;
}
