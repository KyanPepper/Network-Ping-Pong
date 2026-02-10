/*
 * Network Ping-Pong: Measuring Network Latency, Bandwidth, and Buffer Size
 *
 * This program performs ping-pong communication between two MPI processes
 * to empirically estimate network parameters:
 *   - Latency (a): Fixed overhead per message
 *   - Bandwidth (b): Data transfer rate
 *   - Buffer size: Point where MPI_Send becomes blocking
 *
 * Usage: mpirun -N 2 ./pingpong
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <assert.h>
#include <sys/time.h>

#define MIN_MSG_SIZE 1            // Starting message size (1 byte)
#define MAX_MSG_SIZE (1 << 20)    // Maximum message size (1 MB = 2^20 bytes)
#define NUM_ITERATIONS 100        // Number of iterations for averaging
#define WARMUP_ITERATIONS 10      // Warmup iterations (not timed)
#define OUTPUT_FILE "results.csv" // Output file for results

// Get time in microseconds
double get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
}

int main(int argc, char *argv[])
{
    int rank, num_procs;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    // Ensure exactly 2 processes
    if (num_procs != 2)
    {
        if (rank == 0)
        {
            fprintf(stderr, "Error: This program requires exactly 2 processes.\n");
            fprintf(stderr, "Usage: mpirun -np 2 -N 2 ./pingpong\n");
        }
        MPI_Finalize();
        return 1;
    }

    // Open output file and print headers (rank 0 only)
    FILE *outfile = NULL;
    if (rank == 0)
    {
        outfile = fopen(OUTPUT_FILE, "w");
        if (!outfile)
        {
            fprintf(stderr, "Error: Could not open output file %s\n", OUTPUT_FILE);
            MPI_Finalize();
            return 1;
        }

        // Write CSV header
        fprintf(outfile, "msg_size_bytes,avg_send_us,avg_recv_us,rtt_us,bandwidth_mbps\n");

        // Print to console
        printf("=== Network Ping-Pong Experiment ===\n");
        printf("Results will be saved to: %s\n", OUTPUT_FILE);
        printf("Iterations per message size: %d\n", NUM_ITERATIONS);
        printf("Warmup iterations: %d\n\n", WARMUP_ITERATIONS);
        printf("%12s %15s %15s %15s %15s\n",
               "Msg Size", "Avg Send (us)", "Avg Recv (us)",
               "RTT (us)", "Bandwidth (MB/s)");
        printf("%-12s %-15s %-15s %-15s %-15s\n",
               "------------", "---------------", "---------------",
               "---------------", "---------------");
    }

    // Allocate buffers for the largest message size
    char *send_buffer = (char *)malloc(MAX_MSG_SIZE);
    char *recv_buffer = (char *)malloc(MAX_MSG_SIZE);

    if (!send_buffer || !recv_buffer)
    {
        fprintf(stderr, "Rank %d: Failed to allocate memory\n", rank);
        MPI_Finalize();
        return 1;
    }

    // Initialize send buffer with some data
    memset(send_buffer, 'A', MAX_MSG_SIZE);
    memset(recv_buffer, 0, MAX_MSG_SIZE);

    MPI_Status status;

    // Iterate over message sizes (1, 2, 4, 8, ..., 1MB)
    for (int msg_size = MIN_MSG_SIZE; msg_size <= MAX_MSG_SIZE; msg_size *= 2)
    {

        double total_send_time = 0.0;
        double total_recv_time = 0.0;
        double total_rtt = 0.0;

        // Warmup rounds (not timed) - helps stabilize measurements
        for (int i = 0; i < WARMUP_ITERATIONS; i++)
        {
            if (rank == 0)
            {
                // Rank 0: Send then Receive (ping)
                MPI_Send(send_buffer, msg_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(recv_buffer, msg_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD, &status);
            }
            else
            {
                // Rank 1: Receive then Send (pong)
                MPI_Recv(recv_buffer, msg_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);
                MPI_Send(send_buffer, msg_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
            }
        }

        // Synchronize before timing
        MPI_Barrier(MPI_COMM_WORLD);

        // Timed iterations
        for (int i = 0; i < NUM_ITERATIONS; i++)
        {
            double t_start, t_after_send, t_after_recv;

            if (rank == 0)
            {
                // Rank 0: PING (send) then receive PONG
                t_start = get_time_us();
                MPI_Send(send_buffer, msg_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                t_after_send = get_time_us();
                MPI_Recv(recv_buffer, msg_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD, &status);
                t_after_recv = get_time_us();

                total_send_time += (t_after_send - t_start);
                total_recv_time += (t_after_recv - t_after_send);
                total_rtt += (t_after_recv - t_start);
            }
            else
            {
                // Rank 1: Receive PING then send PONG
                t_start = get_time_us();
                MPI_Recv(recv_buffer, msg_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);
                t_after_recv = get_time_us();
                MPI_Send(send_buffer, msg_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
                t_after_send = get_time_us();

                total_recv_time += (t_after_recv - t_start);
                total_send_time += (t_after_send - t_after_recv);
            }
        }

        // Calculate averages
        double avg_send = total_send_time / NUM_ITERATIONS;
        double avg_recv = total_recv_time / NUM_ITERATIONS;
        double avg_rtt = total_rtt / NUM_ITERATIONS;

        // Calculate bandwidth (MB/s) using RTT (round-trip sends 2x the data)
        // Bandwidth = (2 * msg_size) / RTT
        double bandwidth_mbps = 0.0;
        if (avg_rtt > 0)
        {
            bandwidth_mbps = (2.0 * msg_size) / avg_rtt; // bytes/microsecond = MB/s
        }

        // Only rank 0 prints and saves results
        if (rank == 0)
        {
            // Print to console
            printf("%12d %15.2f %15.2f %15.2f %15.2f\n",
                   msg_size, avg_send, avg_recv, avg_rtt, bandwidth_mbps);

            // Write to CSV file
            fprintf(outfile, "%d,%.2f,%.2f,%.2f,%.2f\n",
                    msg_size, avg_send, avg_recv, avg_rtt, bandwidth_mbps);
        }

        // Synchronize before next message size
        MPI_Barrier(MPI_COMM_WORLD);
    }

    // Print footer with analysis hints and close file
    if (rank == 0)
    {
        fclose(outfile);
        printf("\nResults saved to: %s\n", OUTPUT_FILE);
        printf("\n=== Analysis Notes ===\n");
        printf("- Latency (a): Look at RTT/2 for smallest message sizes\n");
        printf("- Bandwidth (b): Look at bandwidth for largest message sizes\n");
        printf("- Buffer size: Look for sudden increase in send time\n");
        printf("  (when message exceeds buffer, MPI_Send blocks until receiver posts MPI_Recv)\n");
    }

    // Cleanup
    free(send_buffer);
    free(recv_buffer);

    MPI_Finalize();
    return 0;
}
