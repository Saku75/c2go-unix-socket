<script lang="ts">
  import { writable } from 'svelte/store';

  let number: number = 1000;
  let primes = writable<number[]>([]);
  let error = writable<string | null>(null);
  let totalTime = writable<string | null>(null);
  let apiTime = writable<string | null>(null);
  let networkTime = writable<string | null>(null);

  let hidden = true

  async function fetchPrimes() {
    const startTime = performance.now();
    try {
      const response = await fetch(`http://localhost:8080/prime_list?limit=${number}`);
      if (!response.ok) {
        throw new Error('Failed to fetch primes');
      }
      const data = await response.json() as { primes: number[], time: string };
      primes.set(data.primes);
      error.set(null);

      const endTime = performance.now();
      const totalDuration = endTime - startTime;
      const apiDuration = parseFloat(data.time.replace('ms', ''));
      const networkDuration = totalDuration - apiDuration;

      totalTime.set(`${totalDuration.toFixed(2)} ms`);
      apiTime.set(data.time);
      networkTime.set(`${networkDuration.toFixed(2)} ms`);
    } catch (err: any) {
      error.set(err.message);
    }
  }

  function handleSubmit(event: Event) {
    event.preventDefault();
    totalTime.set(null);
    apiTime.set(null);
    networkTime.set(null);
    fetchPrimes();
  }
</script>

<h1>Prime Calculator</h1>

<p>Enter a number to find all prime numbers up to that number.</p>
<form on:submit={handleSubmit}>
  <label for="number">Number:</label>
  <input type="number" id="number" bind:value={number} min="0" required />
  <button type="submit">Calculate</button>
</form>

{#if $error}
  <p style="color: red;">{$error}</p>
{/if}

{#if $primes.length > 0}
  <h2>Primes:</h2>
  <p>Total number of primes: {$primes.length}</p>
{/if}

{#if $totalTime}
  <h2>Performance Metrics:</h2>
  <p>Total Time: {$totalTime}</p>
  <p>Compute Time: {$apiTime}</p>
  <p>Network Time: {$networkTime}</p>
{/if}