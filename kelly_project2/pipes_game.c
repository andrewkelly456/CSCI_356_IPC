#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_NUMBER 10
#define GUESS_COMMAND 1
#define QUIT_COMMAND 0

/* sends a command (guess or quit) to a child through a pipe */
int send_command(int fd, int command)
{
    if (write(fd, &command, sizeof(command)) != sizeof(command))
    {
        perror("write command");
        return -1;
    }
    return 0;
}

/* reads one guess from a child through a pipe */
int read_guess(int fd, int *guess)
{
    if (read(fd, guess, sizeof(*guess)) != sizeof(*guess))
    {
        perror("read guess");
        return -1;
    }
    return 0;
}

/* this is all the child does. it waits for the parent, guesses, and
   writes the guess back. it stops when the parent says quit */
void child_loop(int id, int read_fd, int write_fd)
{
    int command;
    int guess;

    /* mix in the pid so the two children dont guess the same numbers */
    srand(time(NULL) ^ getpid());

    while (read(read_fd, &command, sizeof(command)) > 0 && command == GUESS_COMMAND)
    {
        guess = rand() % MAX_NUMBER + 1;
        if (write(write_fd, &guess, sizeof(guess)) != sizeof(guess))
        {
            perror("child write guess");
            exit(1);
        }
    }

    close(read_fd);
    close(write_fd);
    printf("Child %d (PID %d) is done and exiting\n", id, getpid());
    exit(0);
}

/* plays one round. returns 1 if child 1 wins, 2 if child 2 wins,
   and -1 if something went wrong */
int play_round(int round, pid_t pid1, pid_t pid2,
               int to1, int from1, int to2, int from2)
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
        if (send_command(to1, GUESS_COMMAND) == -1)
        {
            return -1;
        }
        if (send_command(to2, GUESS_COMMAND) == -1)
        {
            return -1;
        }
        if (read_guess(from1, &guess1) == -1)
        {
            return -1;
        }
        if (read_guess(from2, &guess2) == -1)
        {
            return -1;
        }

        printf("Child 1 (PID %d) guessed %d\n", pid1, guess1);
        printf("Child 2 (PID %d) guessed %d\n", pid2, guess2);

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

    printf("Child %d wins round %d\n", winner, round);
    printf("------------------------------\n");
    return winner;
}

int main(int argc, char *argv[])
{
    int parent_to_child1[2];
    int child1_to_parent[2];
    int parent_to_child2[2];
    int child2_to_parent[2];
    pid_t child1_pid;
    pid_t child2_pid;
    int rounds;
    int round = 1;
    int winner;
    int wins1 = 0;
    int wins2 = 0;
    int error = 0;

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

    /* line buffer so the output from parent and children stays in order */
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGPIPE, SIG_IGN);

    /* two pipes for each child, one for each direction */
    if (pipe(parent_to_child1) == -1)
    {
        perror("pipe parent_to_child1");
        return 1;
    }
    if (pipe(child1_to_parent) == -1)
    {
        perror("pipe child1_to_parent");
        return 1;
    }
    if (pipe(parent_to_child2) == -1)
    {
        perror("pipe parent_to_child2");
        return 1;
    }
    if (pipe(child2_to_parent) == -1)
    {
        perror("pipe child2_to_parent");
        return 1;
    }

    child1_pid = fork();
    if (child1_pid == -1)
    {
        perror("fork child1");
        return 1;
    }

    if (child1_pid == 0)
    {
        /* child 1 only keeps the ends it needs */
        close(parent_to_child1[1]);
        close(child1_to_parent[0]);
        close(parent_to_child2[0]);
        close(parent_to_child2[1]);
        close(child2_to_parent[0]);
        close(child2_to_parent[1]);
        child_loop(1, parent_to_child1[0], child1_to_parent[1]);
    }

    child2_pid = fork();
    if (child2_pid == -1)
    {
        perror("fork child2");
        return 1;
    }

    if (child2_pid == 0)
    {
        /* child 2 only keeps the ends it needs */
        close(parent_to_child2[1]);
        close(child2_to_parent[0]);
        close(parent_to_child1[0]);
        close(parent_to_child1[1]);
        close(child1_to_parent[0]);
        close(child1_to_parent[1]);
        child_loop(2, parent_to_child2[0], child2_to_parent[1]);
    }

    /* parent closes the ends that belong to the children */
    close(parent_to_child1[0]);
    close(child1_to_parent[1]);
    close(parent_to_child2[0]);
    close(child2_to_parent[1]);

    srand(time(NULL));
    printf("*** Pipes and Processes Guessing Game ***\n");
    printf("Child 1 PID: %d\n", child1_pid);
    printf("Child 2 PID: %d\n", child2_pid);
    printf("Rounds: %d\n\n", rounds);

    while (round <= rounds)
    {
        winner = play_round(round, child1_pid, child2_pid,
                            parent_to_child1[1], child1_to_parent[0],
                            parent_to_child2[1], child2_to_parent[0]);
        if (winner == -1)
        {
            error = 1;
            break;
        }
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

    /* tell the children the tournament is over */
    send_command(parent_to_child1[1], QUIT_COMMAND);
    send_command(parent_to_child2[1], QUIT_COMMAND);
    close(parent_to_child1[1]);
    close(child1_to_parent[0]);
    close(parent_to_child2[1]);
    close(child2_to_parent[0]);

    /* wait for both children so they dont turn into zombies */
    waitpid(child1_pid, NULL, 0);
    waitpid(child2_pid, NULL, 0);

    if (error)
    {
        fprintf(stderr, "Tournament stopped because of an error\n");
        return 1;
    }

    printf("\n*** Final Results ***\n");
    printf("Child 1 (PID %d) won %d rounds\n", child1_pid, wins1);
    printf("Child 2 (PID %d) won %d rounds\n", child2_pid, wins2);
    if (wins1 > wins2)
    {
        printf("Child 1 wins the tournament!\n");
    }
    else if (wins2 > wins1)
    {
        printf("Child 2 wins the tournament!\n");
    }
    else
    {
        printf("The tournament is a tie!\n");
    }

    return 0;
}
