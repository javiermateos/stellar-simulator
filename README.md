# Stellar simulator

This repository contains the final project of the subject Operating Systems of 
the [Escuela Politécnica](https://www.uam.es/ss/Satellite/EscuelaPolitecnica/es/home.htm) from the [Universidad
Autónoma de Madrid](https://uam.es/ss/Satellite/es/home.htm).

## Description

The simulator process is the main process which is responsible to manage
shared resources and coordinate the components in the simulation. Indeed,
he establishes the arrival of a new turn. It will generate a leader process
for each team. They will be responsible to coordinate and manage their spaceships.
Each spaceship will be a child process of the leader process and they execute 
commands sent by the leader process within each shift.

The monitor process show the development of the simulation and has to be executed
in another terminal.

The interprocess communication used are:

- simulator -> leaders : pipes
- leaders -> spaceships: pipes
- spaceships -> simulator: message queue
- state and map -> shared memory
- turn -> alarm

## Requirements

- Make
- Ncurses
    ```sh
    sudo apt install libncurses-dev
    ```

## Play It!
```sh
make
cd build
# First terminal
./simulator
# Second terminal
./monitor
```
**Note**: If you don't launch first the simulator you will get an error saying
that one semaphore was not able to be opened. So, you always have to launch
the simulator program first.
