#!/bin/bash
# usage: ./run.sh [rounds]   (default is 5 rounds)

ROUNDS=${1:-5}

echo "=============================================="
echo " Compiling both programs"
echo "=============================================="
make -f Makefile.pipes
if [ $? -ne 0 ]; then
    echo "Pipes program failed to compile"
    exit 1
fi
make -f Makefile.sockets
if [ $? -ne 0 ]; then
    echo "Sockets program failed to compile"
    exit 1
fi

echo ""
echo "=============================================="
echo " RUNNING: Pipes with Processes ($ROUNDS rounds)"
echo "=============================================="
./pipes_game $ROUNDS
if [ $? -ne 0 ]; then
    echo "Pipes program had an error"
    exit 1
fi

echo ""
echo "=============================================="
echo " RUNNING: Sockets with Threads ($ROUNDS rounds)"
echo "=============================================="
./sockets_game $ROUNDS
if [ $? -ne 0 ]; then
    echo "Sockets program had an error"
    exit 1
fi

echo ""
echo "=============================================="
echo " Both programs finished"
echo "=============================================="
