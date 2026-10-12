IPC Number Guessing Game - Project 2
CSCI 356

Files
-----
pipes_game.c       - guessing game using fork() and pipes (processes)
sockets_game.c     - guessing game using pthreads and sockets (threads)
Makefile.pipes     - compiles pipes_game.c
Makefile.sockets   - compiles sockets_game.c
run.sh             - bash script that compiles and runs both programs
README.md          - this file

What the program does
---------------------
The parent picks a random number from 1 to 10 each round. Two children
(processes in the pipes version, threads in the sockets version) wait for
the parent to tell them to guess. Each child picks a random guess from 1 to
10 and sends it back to the parent. The child closest to the parent's number
wins the round. If both guesses are the same distance away, the parent tells
both children to guess again until there is a winner. After all rounds the
parent prints how many rounds each child won and who won the tournament.
The overall tournament can end in a tie.

How to compile
--------------
Pipes version:
    make -f Makefile.pipes

Sockets version:
    make -f Makefile.sockets

To remove the compiled programs:
    make -f Makefile.pipes clean
    make -f Makefile.sockets clean

How to run
----------
Both programs need one argument, the number of rounds (a number greater
than 0).

    ./pipes_game 5
    ./sockets_game 5

If the argument is missing or not a positive number the program prints a
message and exits.

Using the bash script
---------------------
The script compiles both programs with the two Makefiles and then runs them
one after the other, with a label showing which one is running.

    chmod +x run.sh
    ./run.sh

By default it runs 5 rounds. To use a different number of rounds, give it
as an argument:

    ./run.sh 8

Notes
-----
- Tested on macOS and compiles with gcc.
- Neither program uses global variables.
- sockets_game.c does not use any for loops, only while loops.
- Communication in the pipes version is done with pipe(). Each child has
  one pipe going from the parent to the child and one going back.
- Communication in the sockets version is done with socketpair(). Each
  thread has one socket that works in both directions.
- The parent sends the number 1 to tell a child to guess and the number 0
  to tell it to quit.
