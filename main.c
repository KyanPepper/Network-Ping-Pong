
/* Cpt S 411, Introduction to Parallel Computing
* School of Electrical Engineering and Computer Science
*
* Example code
* Send receive test:
* Rank 1 sends to rank 0 (all other ranks, if any, sit idle)
* Payload is just one integer in this example, but the code can be adapted to
send other types of messages.
* For timing, this code uses C gettimeofday(). Alternatively you can also use
MPI_Wtime().
* */
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <assert.h>
#include <sys/time.h>
int main(int argc, char *argv[])
{
    int rank, p;
    struct timeval t1, t2;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    printf("my rank=%d\n", rank);
    printf("Rank=%d: number of processes =%d\n", rank, p);
    assert(p >= 2);
    if (rank == 1)
    { // rank 1 should send m bytes to rank 0
        int x = 10;
        int dest = 0;
        // start timer
        gettimeofday(&t1, NULL);
        // send message i.e., x from rank 1 to destination (rank 0)
        MPI_Send(&x, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);
        // please note that the send buffer (x) is passed as reference (i.e.,
&x) --- why?
// end message
gettimeofday(&t2,NULL);
// calculate time for send (in microseconds)
int tSend = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);
printf("Rank=%d: Send message size %ld bytes: time %d microsec\n",
       rank, sizeof(int), tSend);
    }
    else if (rank == 0)
    { // rank 0 receives
        int y = 0;
        printf("Rank=%d: y value BEFORE receiving = %d\n", rank, y);
        MPI_Status status;
        // start timer
        gettimeofday(&t1, NULL);
        // receive message into y from any sending source rank
        MPI_Recv(&y, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
                 &status);
        // the above call is written such that this receiver (rank 0) can
        receive message from any sender rank
            // please note that the receive buffer (y) is passed as reference
            (i.e., &y)-- -
            why
            ? printf("Rank=%d: y value AFTER receiving = %d\n", rank, y);
        // end timer
        gettimeofday(&t2, NULL);
        // calculate time for recv (in microseconds)
        int tRecv = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);
        printf("Rank=%d: Recv message size %ld bytes: time %d microsec\n",
               rank, sizeof(int), tRecv);
    }
    MPI_Finalize();
}
