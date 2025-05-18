<script lang="ts">
  import Highcharts from "highcharts/highcharts.js";
  import "highcharts/modules/data";
  import "highcharts/modules/exporting";
  import "highcharts/modules/offline-exporting";
  import { onMount } from "svelte";
  import RangeDatePicker from '$lib/components/RangeDatePicker.svelte';

  let chartDetails: HTMLDetailsElement = $state();
  let myChart: Highcharts.Chart;
  let myChartContainer: HTMLDivElement;
  let title = $state({ text: "Chart" });
  let dataCsv: string = $state("");

  let { count, countTarget } = $state({ count: 0, countTarget: 0 });
  let countPerMinute: number = $state(0);
  let countPerHour: number = $state(0);
  let todayDate: string;
  let dateInput = $state();
  let endDateInput = $state();
  let dateRangeText = $state();
  let dateRangeArray = $state();
  let countEvtSource: EventSource;
  let runningAverageEvents: EventSource;
  let countProgress = $derived(
    countTarget > 0 ? (count / countTarget) * 100 : 0
  );

  const today = new Date();
  const year = today.getFullYear();
  const month = (today.getMonth() + 1).toString().padStart(2, '0');
  const day = today.getDate().toString().padStart(2, '0');
  todayDate = `${year}-${month}-${day}`;

  onMount(() => {
    // Initialize 'count' from the server
    getCurrentCount();

    countEvtSource = new EventSource("/counterStream");
    countEvtSource.onmessage = function (event) {
      const data = event.data;
      count = Number(data);
    };
    countEvtSource.onerror = function (error) {
      console.error("EventSource failed:", error);
    };

    runningAverageEvents = new EventSource("/runningAverageStream");
    runningAverageEvents.onmessage = function (event) {
      const data = event.data;
      const counts = data.split(",");
      countPerMinute = Number(counts[0]);
      countPerHour = Number(counts[1]);
    };
    runningAverageEvents.onerror = function (error) {
      console.error("EventSource failed:", error);
    };

    dataCsv = `/data/${todayDate}.csv`;
    title.text = todayDate;

    myChart = new Highcharts.Chart({
      chart: {
        renderTo: myChartContainer,
        borderRadius: 6,
        type: "line",
        zooming: { type: "x" },
      },
      title: title,
      // subtitle: subtitle,
      xAxis: { type: "datetime" },
      yAxis: { title: { text: "Drum" } },
      // plotOptions: plotOptions,
      data: {
        csvURL: dataCsv,
        enablePolling: true,
        dataRefreshRate: 60
      },
      credits: { enabled: false },
      accessibility: { enabled: false },
      legend: { enabled: false },
    });


  });

  function updateChart() {
    if (dateInput && !endDateInput) {
      if (myChart.series[0]) myChart.series[0].remove(true);
      if (String(dateInput) === todayDate) {
        dataCsv = `/data/${dateInput}.csv`;
        myChart.update({
          title: { text: String(dateInput) },
          data: {
            csvURL: dataCsv,
            enablePolling: true,
          },
        });
      } else {
        dataCsv = `/data/${dateInput}.csv`;
        myChart.update({
          title: { text: String(dateInput) },
          data: {
            csvURL: dataCsv,
            enablePolling: false,
          },
        });
      }
    } else if (endDateInput) {
      const startDate = new Date(String(dateInput));
      const endDate = new Date(String(endDateInput));
      if (startDate >= endDate) {
        alert("End Date should be bigger than start date");
        return;
      }

      myChart.update({
        title: { text: dateRangeText },
        data: {
          csvURL: '',
          enablePolling: false
        },
      })
    } else {
      alert("Please select a date");
      return;
    }
    chartDetails.open = false;
    myChart.redraw();
  }

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
          const counts = data.split(",");
          count = Number(counts[0]);
          countPerMinute = Number(counts[1]);
          countPerHour = Number(counts[2]);
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
  <header class="text-center">
    <!-- <button><svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32"><path fill="currentColor" d="M20.834 8.037L9.64 14.5c-1.43.824-1.43 2.175 0 3l11.194 6.463c1.43.826 2.598.15 2.598-1.5V9.537c0-1.65-1.17-2.326-2.598-1.5"/></svg></button> -->
    <h1 class="mb-0!">Today</h1>
    <!-- <button><svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32"><path fill="currentColor" d="M11.166 23.963L22.36 17.5c1.43-.824 1.43-2.175 0-3L11.165 8.037c-1.43-.826-2.598-.15-2.598 1.5v12.926c0 1.65 1.17 2.326 2.598 1.5z"/></svg></button> -->
  </header>

  <div class="md:flex justify-around gap-5 my-5 text-center">
    <h1>{count} 🛢</h1>
    <h1>{countPerMinute} 🛢 / m</h1>
    <h1>{countPerHour} 🛢 / h</h1>
  </div>
</article>

<article>
  <header>
    <details bind:this={chartDetails}>
      <summary role="button">Graph</summary>
      <form>
        <RangeDatePicker
          bind:startdate={dateInput}
          bind:enddate={endDateInput}
          bind:rangeText={dateRangeText}
          bind:rangeArray={dateRangeArray}
        />
        <input type="submit" onclick={updateChart} />
      </form>
    </details>
  </header>

  <div class="w-full aspect-square md:aspect-3/1 mt-5">
    <div bind:this={myChartContainer}></div>
  </div>
  <!-- <button onclick={addRandomPoint}>Add Random</button> -->
  <!-- <button onclick={()=>{myChart.destroy()}} >Reset</button> -->
</article>
