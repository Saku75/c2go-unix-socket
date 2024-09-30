#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include "sieve_algorithms.h"

#define SOCKET_PATH "/tmp/prime_socket"
#define QUEUE_SIZE 10
#define THREAD_POOL_SIZE 4
#define BUFFER_SIZE 1024 // Reduced buffer size

typedef struct
{
  int client_socket;
  int limit;
} request_t;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
request_t request_queue[QUEUE_SIZE];
int queue_front = 0;
int queue_rear = 0;
int queue_count = 0;

void enqueue(request_t request)
{
  pthread_mutex_lock(&queue_mutex);
  while (queue_count == QUEUE_SIZE)
  {
    pthread_cond_wait(&queue_cond, &queue_mutex);
  }
  request_queue[queue_rear] = request;
  queue_rear = (queue_rear + 1) % QUEUE_SIZE;
  queue_count++;
  pthread_cond_signal(&queue_cond);
  pthread_mutex_unlock(&queue_mutex);
  printf("Enqueued request: limit=%d\n", request.limit);
}

request_t dequeue()
{
  pthread_mutex_lock(&queue_mutex);
  while (queue_count == 0)
  {
    pthread_cond_wait(&queue_cond, &queue_mutex);
  }
  request_t request = request_queue[queue_front];
  queue_front = (queue_front + 1) % QUEUE_SIZE;
  queue_count--;
  pthread_cond_signal(&queue_cond);
  pthread_mutex_unlock(&queue_mutex);
  printf("Dequeued request: limit=%d\n", request.limit);
  return request;
}

void *worker_thread(void *arg)
{
  while (1)
  {
    request_t request = dequeue();
    int size;
    int *primes = NULL;
    struct timeval start, end;

    gettimeofday(&start, NULL);

    primes = sieveOfAtkin(request.limit, &size);

    gettimeofday(&end, NULL);
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    double elapsed = (seconds * 1000.0) + (microseconds / 1000.0); // Convert to milliseconds

    if (primes)
    {
      printf("Calculated primes: size=%d, time=%.3f ms\n", size, elapsed);

      // Generate a unique temporary file path
      char temp_file_path[256];
      snprintf(temp_file_path, sizeof(temp_file_path), "/tmp/prime_response_%d.json", request.client_socket);

      // Open the file for writing
      FILE *file = fopen(temp_file_path, "w");
      if (!file)
      {
        perror("fopen");
        free(primes);
        close(request.client_socket);
        continue;
      }

      // Write the JSON response to the file
      fprintf(file, "{\"primes\":[");
      for (int i = 0; i < size; i++)
      {
        fprintf(file, "%d", primes[i]);
        if (i < size - 1)
        {
          fprintf(file, ",");
        }
      }
      fprintf(file, "], \"time\": \"%.3f ms\"}", elapsed);
      fclose(file);

      // Send the file path to the client
      write(request.client_socket, temp_file_path, strlen(temp_file_path));

      free(primes);
      printf("Response written to file: %s\n", temp_file_path);
    }
    else
    {
      printf("Failed to calculate primes for limit=%d\n", request.limit);
    }
    close(request.client_socket);
  }
  return NULL;
}

int main()
{
  int server_socket, client_socket;
  struct sockaddr_un server_addr, client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  pthread_t threads[THREAD_POOL_SIZE];

  // Create and bind the socket
  server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_socket < 0)
  {
    perror("socket");
    exit(EXIT_FAILURE);
  }
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
  unlink(SOCKET_PATH);
  if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("bind");
    close(server_socket);
    exit(EXIT_FAILURE);
  }
  if (listen(server_socket, 5) < 0)
  {
    perror("listen");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  printf("Server started, waiting for connections...\n");

  // Create worker threads
  for (int i = 0; i < THREAD_POOL_SIZE; i++)
  {
    pthread_create(&threads[i], NULL, worker_thread, NULL);
  }

  // Accept and handle incoming connections
  while (1)
  {
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket < 0)
    {
      perror("accept");
      continue;
    }

    char buffer[256];
    int n = read(client_socket, buffer, sizeof(buffer) - 1);
    if (n < 0)
    {
      perror("read");
      close(client_socket);
      continue;
    }
    buffer[n] = '\0';

    request_t request;
    request.client_socket = client_socket;
    sscanf(buffer, "%d", &request.limit); // Ensure correct parsing
    printf("Received request: limit=%d\n", request.limit);
    enqueue(request);
  }

  close(server_socket);
  unlink(SOCKET_PATH);
  return 0;
}