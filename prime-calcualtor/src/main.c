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
#define BUFFER_SIZE 1048576 // 1 MB buffer size

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

      // Format the response as JSON
      char *response = malloc(BUFFER_SIZE);
      if (!response)
      {
        perror("malloc");
        free(primes);
        close(request.client_socket);
        continue;
      }
      snprintf(response, BUFFER_SIZE, "{\"primes\":[");
      size_t response_len = strlen(response);
      for (int i = 0; i < size; i++)
      {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", primes[i]);
        size_t buffer_len = strlen(buffer);
        if (response_len + buffer_len + 2 >= BUFFER_SIZE)
        {
          response = realloc(response, response_len + buffer_len + 2);
          if (!response)
          {
            perror("realloc");
            free(primes);
            close(request.client_socket);
            continue;
          }
        }
        strcat(response, buffer);
        response_len += buffer_len;
        if (i < size - 1)
        {
          strcat(response, ",");
          response_len++;
        }
      }
      char time_buffer[64];
      snprintf(time_buffer, sizeof(time_buffer), "], \"time\": \"%.3f ms\"}", elapsed);
      size_t time_buffer_len = strlen(time_buffer);
      if (response_len + time_buffer_len + 1 >= BUFFER_SIZE)
      {
        response = realloc(response, response_len + time_buffer_len + 1);
        if (!response)
        {
          perror("realloc");
          free(primes);
          close(request.client_socket);
          continue;
        }
      }
      strcat(response, time_buffer);
      response_len += time_buffer_len;

      // Send the JSON response to the client
      size_t total_written = 0;
      while (total_written < response_len)
      {
        ssize_t written = write(request.client_socket, response + total_written, response_len - total_written);
        if (written < 0)
        {
          perror("write");
          break;
        }
        total_written += written;
      }
      free(response);
      free(primes);
      printf("Response size: %ld bytes\n", total_written);
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