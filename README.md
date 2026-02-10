# Network Ping-Pong

MPI ping-pong benchmark to measure network latency, bandwidth, and buffer size between two processes.

## Build

```bash
mpicc -o pingpong main.c
```

## Run

```bash
mpirun -np 2 -N 2 ./pingpong
or 
mpiexec -np 2 ./pingpong
or
srun -N 2 -n 2 ./pingpong

```

This outputs results to `results.csv`.

## Generate Report

Requires Python with matplotlib and numpy:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install matplotlib numpy
python generate_report.py
```

Creates `Network_PingPong_Report.pdf` with charts and analysis.
