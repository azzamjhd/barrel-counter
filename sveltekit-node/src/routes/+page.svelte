<script lang="ts">
  import * as Highcharts from "highcharts";
  import "highcharts/modules/exporting";
  import "highcharts/modules/offline-exporting";
  import { Chart } from "@highcharts/svelte";
  import { onMount, onDestroy } from "svelte";
  import { fade } from "svelte/transition";
  import { faCrop } from "@fortawesome/free-solid-svg-icons";

  let title = $state({ text: "Chart" });
  let type = $state({ type: "column", borderRadius: 6 });
  let subtitle = $state({ text: "25/02/2025 - 29/02/2025" });
  let yLabel = $state({ title: { text: "Drum/Hour" }, visible: false });
  let plotOptions = { series: { dataLabels: { enabled: true }, cropThreshold:10} };
  let chartSeries = $state([
    {
      name: "My Series",
      data: [],
    },
    {
      name: "Second Series",
      data: [],
    },
  ]);

  function addRandomPoint() {
    const x = Date.now();
    const value1 = Math.floor(Math.random() * 100) / 10;
    const value2 = Math.floor(Math.random() * 100) / 10;
    chartSeries[0].data?.push([x,value1]);
    chartSeries[1].data?.push([x,value2]);
  }

  let options = $state({
    chart: type,
    title: title,
    subtitle: subtitle,
    yAxis: yLabel,
    xAxis: { type: 'datetime',dateTimeLabelFormats: {second: '%H:%M:%S'}},
    plotOptions: plotOptions,
    series: chartSeries,
    credits: { enabled: false}
  });

  let { count, countTarget } = $state({ count: 0, countTarget: 0 });
  let { isRange, useTarget } = $state({ isRange: false, useTarget: false });
  let evtSource: EventSource;
  let countProgress = $derived(
    countTarget > 0 ? (count / countTarget) * 100 : 0
  );

  onMount(() => {
    // Initialize 'count' from the server
    getCurrentCount();

    evtSource = new EventSource("/counterStream");

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

  function getCurrentCount() {
    fetch("/getCount", {
      method: "GET",
      headers: { "Content-Type": "text/plain" },
    })
      .then((response) => {
        if (!response.ok) {
          console.error("Failed to get count:", response.statusText);
        } else {
          return response.text();
        }
      })
      .then((data) => {
        if (data) {
          count = Number(data);
          console.log("Current count:", count);
        }
      })
      .catch((error) => {
        console.error("Error getting count:", error);
      });
  }
</script>

<svelte:head>
  <title>Home</title>
  <meta name="description" content="Svelte demo app" />
</svelte:head>

<article>
  <header>
    <details>
      <summary role="button">Today</summary>
      <form action="">
        <div class="flex justify-between text-start my-2 items-center">
          <label for="">Target</label>
          <input type="checkbox" bind:checked={useTarget} />
        </div>
        <div>
          <input type="number" bind:value={countTarget} disabled={!useTarget} />
        </div>
        <input type="submit" />
      </form>
    </details>
  </header>

  <div class="flex justify-around">
    <h1>{count} 🛢</h1>
    {#if useTarget || countTarget > 0}
      <h1 transition:fade>/</h1>
      <h1 transition:fade>{countTarget} 🛢</h1>
    {/if}
  </div>
  {#if useTarget || countTarget > 0}
    <progress transition:fade value={countProgress} max="100"></progress>
  {/if}
</article>

<article>
  <header>
    <details>
      <summary role="button">Graph</summary>
      <form>
        <div class="flex justify-between text-start my-2 items-center">
          <label for="">Range</label>
          <input type="checkbox" bind:checked={isRange} />
        </div>
        <hr />
        <div class="md:flex md:justify-around text-start gap-2">
          <div class="w-full">
            <label for="startDate"
              >{isRange ? "Start Date" : "Select Date"}</label
            >
            <input type="date" id="startDate" required />
          </div>
          {#if isRange}
            <div transition:fade class="w-full">
              <label for="endDate">End Date</label>
              <input type="date" id="endDate" />
            </div>
          {/if}
        </div>
        <input type="submit" />
      </form>
    </details>
  </header>
  <div class="w-full aspect-square md:aspect-3/1">
    <Chart options={options} highcharts={Highcharts} />
  </div>
  <button onclick={addRandomPoint}>Add Random</button>
</article>
