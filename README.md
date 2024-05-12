# You Spin Me Round Robin

Round Robin processes each process for time = quantum length to use the CPU, and it does this for each scheduled process to ensure every process gets some CPU time to increase interactivity.

## Building

```shell
Run `make` to build an executable file of rr.c
```

## Running

cmd for running
```shell
Run by running `./rr [process.txt] [quantum length]`
For example `./rr process.txt 4` runs each process in process.txt with a quantum length of 4
```

results
```shell
Some running results include:
./rr processes.txt 3
Average waiting time: 7.00
Average response time: 2.75

./rr processes.txt 4
Average waiting time: 4.50
Average response time: 3.25
```

## Cleaning up

```shell
Run `make clean` to cleanup and remove built executables
```
