#include <stdlib.h>
#include <string.h>
#include <math.h>

int *sieveOfEratosthenes(int limit, int *size)
{
  int *primes = malloc((limit + 1) * sizeof(int));
  if (!primes)
    return NULL;

  memset(primes, 1, (limit + 1) * sizeof(int));
  primes[0] = primes[1] = 0;

  for (int p = 2; p * p <= limit; p++)
  {
    if (primes[p])
    {
      for (int i = p * p; i <= limit; i += p)
      {
        primes[i] = 0;
      }
    }
  }

  int count = 0;
  for (int i = 2; i <= limit; i++)
  {
    if (primes[i])
      count++;
  }

  int *result = malloc(count * sizeof(int));
  if (!result)
  {
    free(primes);
    return NULL;
  }

  int index = 0;
  for (int i = 2; i <= limit; i++)
  {
    if (primes[i])
      result[index++] = i;
  }

  free(primes);
  *size = count;
  return result;
}

int *sieveOfAtkin(int limit, int *size)
{
  int *primes = malloc((limit + 1) * sizeof(int));
  if (!primes)
    return NULL;

  memset(primes, 0, (limit + 1) * sizeof(int));

  for (int x = 1; x * x <= limit; x++)
  {
    for (int y = 1; y * y <= limit; y++)
    {
      int n = (4 * x * x) + (y * y);
      if (n <= limit && (n % 12 == 1 || n % 12 == 5))
      {
        primes[n] ^= 1;
      }
      n = (3 * x * x) + (y * y);
      if (n <= limit && n % 12 == 7)
      {
        primes[n] ^= 1;
      }
      n = (3 * x * x) - (y * y);
      if (x > y && n <= limit && n % 12 == 11)
      {
        primes[n] ^= 1;
      }
    }
  }

  for (int r = 5; r * r <= limit; r++)
  {
    if (primes[r])
    {
      for (int i = r * r; i <= limit; i += r * r)
      {
        primes[i] = 0;
      }
    }
  }

  primes[2] = primes[3] = 1;

  int count = 0;
  for (int a = 2; a <= limit; a++)
  {
    if (primes[a])
      count++;
  }

  int *result = malloc(count * sizeof(int));
  if (!result)
  {
    free(primes);
    return NULL;
  }

  int index = 0;
  for (int a = 2; a <= limit; a++)
  {
    if (primes[a])
      result[index++] = a;
  }

  free(primes);
  *size = count;
  return result;
}