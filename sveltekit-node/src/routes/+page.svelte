<script lang="ts">
  import Highcharts, { Point, Series, type SeriesOptions } from "highcharts/highcharts.js";
  import "highcharts/modules/exporting";
  import "highcharts/modules/offline-exporting";
  import { onMount } from "svelte";
  import RangeDatePicker from "$lib/components/RangeDatePicker.svelte";
  import HistoryTable from "$lib/components/HistoryTable.svelte";

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
  let dateRangeArray: Array<string> = $state();
  let countEvtSource: EventSource;
  let runningAverageEvents: EventSource;
  let countProgress = $derived(
    countTarget > 0 ? (count / countTarget) * 100 : 0
  );

  let historyURL: string = $state("");
  let enablePolling: boolean = $state(false);

  const today = new Date();
  const year = today.getFullYear();
  const month = (today.getMonth() + 1).toString().padStart(2, "0");
  const day = today.getDate().toString().padStart(2, "0");
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
      credits: { enabled: false },
      accessibility: { enabled: false },
      legend: { enabled: true },
    });

    dateInput = todayDate;
    updateChart();
  });

  function updateChart() {
    // if just one date is selected, it will show the data for that date
    // if two dates are selected, it will show the data for that range
    if (dateInput && !endDateInput) {
      while (myChart.series.length > 0) {
        myChart.series[0].remove(true);
      }
      if (dateInput === todayDate) {
        enablePolling = true;
      } else {
        enablePolling = false;
      }
      const newCsvUrl = `/data/${dateInput}.csv`;
      historyURL = newCsvUrl;
      fetch(newCsvUrl)
        .then((response) => {
          if (!response.ok) {
            console.error("Failed to fetch CSV data:", response.statusText);
            return;
          }
          return response.text();
        })
        .then((data) => {
          if (!data) {
            console.error("No data received for the CSV file");
            return;
          }
          const parsedData = parseCsvData(data);
          myChart.update({
            title: { text: String(dateInput)},
          });
          for (let i = 0; i < parsedData.length; i++) {
            myChart.addSeries(parsedData[i]);
          }
        })
        .catch((error) => {
          console.error("Error fetching CSV data:", error);
        });
        
    } else if (endDateInput) {
      while (myChart.series.length > 0) {
        myChart.series[0].remove(true);
      }
      const newCsvUrls = [];
      for (let i = 0; i < dateRangeArray.length; i++) {
        newCsvUrls.push(`/data/${dateRangeArray[i]}.csv`);
      }
      fetchMultipleCsvFiles(newCsvUrls)
        .then((results) => {
          results.forEach((data) => {
            if (!data) {
              console.error("No data received for one of the CSV files");
              return;
            }
            const parsedData = parseCsvData(data);
            for (let i = 0; i < parsedData.length; i++) {
              parsedData[i].data = parsedData[i].data.map((point) => {
                const newDates = point[0].split("T");
                return [`${dateRangeArray[0]}T${newDates[1]}`, point[1]];
              });

              myChart.addSeries(parsedData[i]);
            }
          });
          myChart.update({
            title: { text: String(dateRangeText) },
          });
        })
        .catch((error) => {
          console.error("Error fetching CSV data:", error);
        });
    } else {
      alert("Please select a date");
      return;
    }
    chartDetails.open = false;
    myChart.redraw();
  }

  function parseCsvData(csvData: string) {
    // Example CSV data format:
    // time,count,cpm,cph
    // 2025-05-16T08:00,6,3.5,210
    // 2025-05-16T08:01,7,4.0,240

    const lines = csvData.split("\n");
    const parsedData = [];
    const headers = lines[0].split(",");
    let date = lines[1].split(",")[0];
    date = date.split("T")[0];
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].trim();
      if (i === 0) {
        for (let j = 1; j < headers.length; j++) {
          const name = headers[j];
          parsedData.push({
            name: name + " " + date,
            data: [],
          });
        }
      } else if (line) {
        const values = line.split(",");
        for (let j = 1; j < headers.length; j++) {
          if (parsedData[j - 1].data) {
            if (values[j] !== "") {
              parsedData[j - 1].data.push([values[0], Number(values[j])]);
            }
          }
        }
      }
    }
    return parsedData;
  }

  async function fetchMultipleCsvFiles(csvUrls: string[]) {
    const promises = csvUrls.map((url) =>
      fetch(url)
        .then((response) => {
          if (!response.ok) {
            throw new Error(`Failed to fetch ${url}: ${response.statusText}`);
          }
          return response.text();
        })
        .catch((error) => {
          console.error("Error fetching CSV data:", error);
        })
    );

    const results = await Promise.all(promises);
    return results;
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

<HistoryTable csvUrl={historyURL} {enablePolling} />
