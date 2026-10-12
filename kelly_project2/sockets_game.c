#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#define MAX_NUMBER 10
#define GUESS_COMMAND 1
#define QUIT_COMMAND 0

/* info that each thread needs, made in main and passed to the thread */
struct thread_info
{
    int id;
    int fd;
    unsigned int seed;
};

/* sends a command (guess or quit) to a thread through a socket */
int send_command(int fd, int command)
{
    if (send(fd, &command, sizeof(command), 0) != sizeof(command))
    {
        perror("send command");
        return -1;
    }
    return 0;
}

/* reads one guess from a thread through a socket */
int read_guess(int fd, int *guess)
{
    if (recv(fd, guess, sizeof(*guess), 0) != sizeof(*guess))
    {
        perror("recv guess");
        return -1;
    }
    return 0;
}

/* this is all the thread does. it waits for the parent, guesses, and
   sends the guess back. it stops when the parent says quit */
void *child_thread(void *arg)
{
    struct thread_info *info = (struct thread_info *)arg;
    int command;
    int guess;

    while (recv(info->fd, &command, sizeof(command), 0) > 0 && command == GUESS_COMMAND)
    {
        /* rand_r so the two threads dont mess with each other */
        guess = rand_r(&info->seed) % MAX_NUMBER + 1;
        if (send(info->fd, &guess, sizeof(guess), 0) != sizeof(guess))
        {
            perror("thread send guess");
            close(info->fd);
            return NULL;
        }
    }

    close(info->fd);
    printf("Thread %d is done and exiting\n", info->id);
    return NULL;
}

/* plays one round. returns 1 if thread 1 wins, 2 if thread 2 wins,
   and -1 if something went wrong */
int play_round(int round, int fd1, int fd2)
{
    int target = rand() % MAX_NUMBER + 1;
    int guess1;
    int guess2;
    int dist1;
    int dist2;
    int winner = 0;

    printf("========== Round %d ==========\n", round);
    printf("Parent picked the number %d\n", target);

    /* keep going until there is a winner, ties get redone */
    while (winner == 0)
    {
        if (send_command(fd1, GUESS_COMMAND) == -1)
        {
            return -1;
        }
        if (send_command(fd2, GUESS_COMMAND) == -1)
        {
            return -1;
        }
        if (read_guess(fd1, &guess1) == -1)
        {
            return -1;
        }
        if (read_guess(fd2, &guess2) == -1)
        {
            return -1;
        }

        printf("Thread 1 guessed %d\n", guess1);
        printf("Thread 2 guessed %d\n", guess2);

        dist1 = abs(target - guess1);
        dist2 = abs(target - guess2);

        if (dist1 < dist2)
        {
            winner = 1;
        }
        else if (dist2 < dist1)
        {
            winner = 2;
        }
        else
        {
            printf("Tie! Both are %d away. Guessing again...\n", dist1);
        }
    }

    printf("Thread %d wins round %d\n", winner, round);
    printf("------------------------------\n");
    return winner;
}

int main(int argc, char *argv[])
{
    int sock1[2];
    int sock2[2];
    pthread_t thread1;
    pthread_t thread2;
    struct thread_info info1;
    struct thread_info info2;
    int rounds;
    int round = 1;
    int winner;
    int wins1 = 0;
    int wins2 = 0;
    int error = 0;
    int thread1_started = 0;
    int thread2_started = 0;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <number of rounds>\n", argv[0]);
        return 1;
    }
    rounds = atoi(argv[1]);
    if (rounds <= 0)
    {
        fprintf(stderr, "Number of rounds must be greater than 0\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    /* one socket pair for each thread. the parent uses index 0 and
       the thread uses index 1 */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock1) == -1)
    {
        perror("socketpair 1");
        return 1;
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock2) == -1)
    {
        perror("socketpair 2");
        return 1;
    }

    info1.id = 1;
    info1.fd = sock1[1];
    info1.seed = time(NULL) ^ 1111;
    info2.id = 2;
    info2.fd = sock2[1];
    info2.seed = time(NULL) ^ 2222;

    if (pthread_create(&thread1, NULL, child_thread, &info1) != 0)
    {
        perror("pthread_create 1");
        error = 1;
    }
    else
    {
        thread1_started = 1;
    }

    if (!error)
    {
        if (pthread_create(&thread2, NULL, child_thread, &info2) != 0)
        {
            perror("pthread_create 2");
            error = 1;
        }
        else
        {
            thread2_started = 1;
        }
    }

    if (!error)
    {
        srand(time(NULL));
        printf("*** Sockets and Threads Guessing Game ***\n");
        printf("Threads: Thread 1 and Thread 2\n");
        printf("Rounds: %d\n\n", rounds);
    }

    while (!error && round <= rounds)
    {
        winner = play_round(round, sock1[0], sock2[0]);
        if (winner == -1)
        {
            error = 1;
        }
        else
        {
            if (winner == 1)
            {
                wins1++;
            }
            else
            {
                wins2++;
            }
            round++;
        }
    }

    /* tell the threads the tournament is over */
    if (thread1_started)
    {
        send_command(sock1[0], QUIT_COMMAND);
    }
    if (thread2_started)
    {
        send_command(sock2[0], QUIT_COMMAND);
    }

    /* wait for the threads to finish */
    if (thread1_started)
    {
        pthread_join(thread1, NULL);
    }
    if (thread2_started)
    {
        pthread_join(thread2, NULL);
    }

    close(sock1[0]);
    close(sock2[0]);
    if (!thread1_started)
    {
        close(sock1[1]);
    }
    if (!thread2_started)
    {
        close(sock2[1]);
    }

    if (error)
    {
        fprintf(stderr, "Tournament stopped because of an error\n");
        return 1;
    }

    printf("\n*** Final Results ***\n");
    printf("Thread 1 won %d rounds\n", wins1);
    printf("Thread 2 won %d rounds\n", wins2);
    if (wins1 > wins2)
    {
        printf("Thread 1 wins the tournament!\n");
    }
    else if (wins2 > wins1)
    {
        printf("Thread 2 wins the tournament!\n");
    }
    else
    {
        printf("The tournament is a tie!\n");
    }

    return 0;
}
