#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

// Define constants for the socket path, buffer size, and thread pool size
#define SOCKET_PATH "/tmp/unix_socket"
#define BUFFER_SIZE 256
#define THREAD_POOL_SIZE 4

// Structure to hold client connection information
typedef struct
{
  int client_socket;
  struct sockaddr_un client_addr;
} connection_t;

// Mutex and condition variable for synchronizing access to the connection queue
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Connection queue to hold pending client connections
connection_t *connection_queue[THREAD_POOL_SIZE];
int queue_size = 0;

// Worker thread function to handle client connections
void *worker_thread(void *arg)
{
  while (1)
  {
    connection_t *conn;

    // Lock the mutex to access the connection queue
    pthread_mutex_lock(&mutex);
    // Wait for a connection to be added to the queue
    while (queue_size == 0)
    {
      pthread_cond_wait(&cond, &mutex);
    }
    // Get the next connection from the queue
    conn = connection_queue[--queue_size];
    // Unlock the mutex
    pthread_mutex_unlock(&mutex);

    if (conn)
    {
      char buffer[BUFFER_SIZE];
      int bytes_read;

      // Read data from the client socket
      while ((bytes_read = read(conn->client_socket, buffer, BUFFER_SIZE)) > 0)
      {
        buffer[bytes_read] = '\0';
        printf("Received: %s\n", buffer);
      }

      // Close the client socket and free the connection structure
      close(conn->client_socket);
      free(conn);
    }
  }
  return NULL;
}

// Function to set up the Unix socket
int setup_unix_socket()
{
  int server_socket;
  struct sockaddr_un server_addr;

  // Create a Unix socket
  if ((server_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
  {
    perror("socket error");
    exit(EXIT_FAILURE);
  }

  // Initialize the server address structure
  memset(&server_addr, 0, sizeof(struct sockaddr_un));
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

  // Unlink the socket path to remove any previous socket file
  unlink(SOCKET_PATH);

  // Bind the socket to the server address
  if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) == -1)
  {
    perror("bind error");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(server_socket, 5) == -1)
  {
    perror("listen error");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  return server_socket;
}

// Main function to initialize and run the server
int main()
{
  int server_socket;
  struct sockaddr_un client_addr;
  socklen_t client_addr_len = sizeof(struct sockaddr_un);
  pthread_t thread_pool[THREAD_POOL_SIZE];

  // Set up the Unix socket
  server_socket = setup_unix_socket();

  printf("Server listening on %s\n", SOCKET_PATH);

  // Create the worker threads
  for (int i = 0; i < THREAD_POOL_SIZE; i++)
  {
    pthread_create(&thread_pool[i], NULL, worker_thread, NULL);
  }

  // Main loop to accept incoming connections
  while (1)
  {
    connection_t *conn = malloc(sizeof(connection_t));
    // Accept a new client connection
    if ((conn->client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) == -1)
    {
      perror("accept error");
      free(conn);
      continue;
    }

    // Lock the mutex to access the connection queue
    pthread_mutex_lock(&mutex);
    // Wait if the connection queue is full
    while (queue_size == THREAD_POOL_SIZE)
    {
      pthread_cond_wait(&cond, &mutex);
    }
    // Add the new connection to the queue
    connection_queue[queue_size++] = conn;
    // Signal a worker thread that a new connection is available
    pthread_cond_signal(&cond);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex);
  }

  // Close the server socket and unlink the socket path
  close(server_socket);
  unlink(SOCKET_PATH);

  return 0;
}