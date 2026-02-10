/*
 * Network Ping-Pong: Measuring Network Latency, Bandwidth, and Buffer Size
 *
 * This program performs ping-pong communication between two MPI processes
 * to empirically estimate network parameters:
 *   - Latency (a): Fixed overhead per message
 *   - Bandwidth (b): Data transfer rate
 *   - Buffer size: Point where MPI_Send becomes blocking
 *
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
        printf("Ping-Pong Test (%d iterations, %d warmup)\n\n", NUM_ITERATIONS, WARMUP_ITERATIONS);
        printf("%10s %12s %12s %12s %12s\n",
               "Size (B)", "Send (us)", "Recv (us)", "RTT (us)", "BW (MB/s)");
        printf("---------- ------------ ------------ ------------ ------------\n");
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

    // Variables to track estimates
    double min_rtt = 1e9;         // For latency estimation (smallest RTT)
    double max_bandwidth = 0.0;   // For bandwidth estimation (largest bandwidth)
    int buffer_size_estimate = 0; // For buffer size detection
    double prev_send_time = 0.0;  // To detect buffer size threshold
    int buffer_detected = 0;      // Flag for buffer detection

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
            printf("%10d %12.2f %12.2f %12.2f %12.2f\n",
                   msg_size, avg_send, avg_recv, avg_rtt, bandwidth_mbps);

            // Write to CSV file
            fprintf(outfile, "%d,%.2f,%.2f,%.2f,%.2f\n",
                    msg_size, avg_send, avg_recv, avg_rtt, bandwidth_mbps);

            // Track minimum RTT (for latency - use small message sizes)
            if (msg_size <= 64 && avg_rtt < min_rtt)
            {
                min_rtt = avg_rtt;
            }

            // Track maximum bandwidth (for bandwidth estimation)
            if (bandwidth_mbps > max_bandwidth)
            {
                max_bandwidth = bandwidth_mbps;
            }

            // Detect buffer size: look for significant jump in send time
            // When message exceeds buffer, MPI_Send blocks, causing longer send times
            if (!buffer_detected && prev_send_time > 0)
            {
                // If send time increases by more than 50%, buffer threshold likely exceeded
                if (avg_send > prev_send_time * 1.5 && msg_size >= 1024)
                {
                    buffer_size_estimate = msg_size / 2; // Previous size was within buffer
                    buffer_detected = 1;
                }
            }
            prev_send_time = avg_send;
        }

        // Synchronize before next message size
        MPI_Barrier(MPI_COMM_WORLD);
    }

    // Print footer with analysis hints and close file
    if (rank == 0)
    {
        // Calculate derived estimates
        double latency_estimate = min_rtt / 2.0;   // One-way latency from RTT
        double bandwidth_estimate = max_bandwidth; // MB/s

        // Print summary
        printf("\n--- Results ---\n");
        printf("Latency: %.2f us (RTT/2 for small msgs)\n", latency_estimate);
        printf("Bandwidth: %.2f MB/s (max observed)\n", bandwidth_estimate);
        if (buffer_detected)
        {
            printf("Buffer size: ~%d bytes\n", buffer_size_estimate);
        }
        else
        {
            printf("Buffer size: >1MB (no blocking seen)\n");
        }
        printf("\n");

        // Write summary to CSV file
        fprintf(outfile, "\n");
        fprintf(outfile, "# Latency: %.2f us\n", latency_estimate);
        fprintf(outfile, "# Bandwidth: %.2f MB/s\n", bandwidth_estimate);
        if (buffer_detected)
        {
            fprintf(outfile, "# Buffer size: %d bytes\n", buffer_size_estimate);
        }
        else
        {
            fprintf(outfile, "# Buffer size: >1MB\n");
        }

        fclose(outfile);
        printf("Saved to %s\n", OUTPUT_FILE);
    }

    // Cleanup
    free(send_buffer);
    free(recv_buffer);

    MPI_Finalize();
    return 0;
}
