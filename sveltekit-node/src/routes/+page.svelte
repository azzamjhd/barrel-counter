<script lang="ts">
  import { onMount } from 'svelte';

  let count: number = 0;
  let evtSource: EventSource;

  onMount(() => {
    evtSource = new EventSource('/counterStream');

    evtSource.onopen = function (event) {
      // console.log(event);
    };

    evtSource.onmessage = function (event) {
      // console.log(event);
      const data = event.data;
      count = Number(data);
    };

    evtSource.onerror = function (error) {
      console.error("EventSource failed:", error);
    };
  });
</script>

<svelte:head>
	<title>Home</title>
	<meta name="description" content="Svelte demo app" />
</svelte:head>

<article>
  <header>Drum Counter</header>
  {count}
</article>