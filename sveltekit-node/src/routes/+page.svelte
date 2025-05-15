<script lang="ts">
  import Highcharts from "highcharts/highcharts.js";
  import "highcharts/modules/exporting";
  import "highcharts/modules/offline-exporting";
  import { onMount, onDestroy } from "svelte";
  import { fade } from "svelte/transition";

  let myChart: Highcharts.Chart;
  let myChartContainer: HTMLDivElement;
  let title = $state({ text: "Chart" });
  let type = $state({ type: "column", borderRadius: 6, });
  let subtitle = $state({ text: "25/02/2025 - 29/02/2025" });
  let yLabel = $state({ title: { text: "Drum/Hour" }, visible: false });
  let plotOptions = {
    series: { dataLabels: { enabled: true }, cropThreshold: 10 },
  };
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
    chartSeries[0].data?.push([x, value1]);
    chartSeries[1].data?.push([x, value2]);
    if (chartSeries[0].data.length > 40) {
      chartSeries[0].data.shift();
    }
    if (chartSeries[1].data.length > 40) {
      chartSeries[1].data.shift();
    }
  }

  let options = $state({
    chart: type,
    title: title,
    subtitle: subtitle,
    yAxis: yLabel,
    // xAxis: { type: "datetime", dateTimeLabelFormats: { second: "%H:%M:%S" } },
    // plotOptions: plotOptions,
    // series: chartSeries,
    series: [{
      lineWidth: 1,
    }],
    data: {
      // csvURL: window.location.origin + "/data/data.csv",
      csv: document.getElementById('mycsv')?.innerHTML,
    },
    credits: { enabled: false },
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

    console.log(document.getElementById('mycsv')?.innerHTML);

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

    myChart = new Highcharts.Chart({
      chart: {
        renderTo: myChartContainer,
        borderRadius: 6,
        type: "column",
        zooming: { type: "x", }
      },
      title: title,
      subtitle: subtitle,
      xAxis: { type: "datetime", dateTimeLabelFormats: { second: "%H:%M:%S" } },
      data: { csv: document.getElementById('mycsv')?.innerHTML },
      plotOptions: plotOptions,
      credits: { enabled: false },
      accessibility: { enabled: false },
      legend: { enabled: false },
    });

    const csvData = document.getElementById('mycsv')?.innerHTML;
    const parsedData = csvData
      .split("\n")
      .slice(1)
      .map((line) => line.split(",").map((item) => item.trim()));
    const seriesData = parsedData.map((item) => ({
      x: new Date(item[0]).getTime(),
      y: Number(item[1]),
    }));
    myChart.addSeries({
      name: "My Series",
      data: seriesData,
    });

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

  function getCSV() {
    fetch("/data/data.csv", {
      method: "GET",
      headers: { "Content-Type": "text/csv" },
    })
      .then((response) => {
        if (!response.ok) {
          console.error("Failed to get CSV:", response.statusText);
        } else {
          return response.text();
        }
      })
      .then((data) => {
        if (data) {
          console.log("CSV data:", data);
        }
      })
      .catch((error) => {
        console.error("Error getting CSV:", error);
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

  <div class="flex justify-around gap-5 my-5">
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
  <div class="w-full aspect-square md:aspect-3/1 mt-5">
    <div bind:this={myChartContainer}></div>
  </div>
  <button onclick={addRandomPoint}>Add Random</button>
  <button onclick={getCSV}>Get CSV</button>
</article>

<pre id="mycsv" style="display:none">time,count
2025-05-13T03:40,1
2025-05-13T03:41,5
2025-05-13T03:42,9
2025-05-13T03:43,17
2025-05-13T03:44,21
2025-05-13T03:45,25
2025-05-13T03:46,29
2025-05-13T03:47,13
2025-05-13T03:48,33
2025-05-13T03:49,37</pre>
