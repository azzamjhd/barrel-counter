<script>
  import { fade } from "svelte/transition";
  let {
    startdate = $bindable(),
    enddate = $bindable(),
    rangeText = $bindable(),
    rangeArray = $bindable(),
  } = $props();

  let selectedStartDate = $state();
  let selectedEndDate = $state();
  let isRange = $state(false);

  $effect(() => {
    startdate = selectedStartDate;
    enddate = selectedEndDate;

    if (selectedStartDate && selectedEndDate) {
      rangeText = `${selectedStartDate} - ${selectedEndDate}`;
      rangeArray = (() => {
        const dates = [];
        let currentDate = new Date(selectedStartDate);
        const endDateObj = new Date(selectedEndDate);

        // Adjust for timezone offset to avoid off-by-one day issues
        currentDate.setMinutes(
          currentDate.getMinutes() + currentDate.getTimezoneOffset()
        );
        endDateObj.setMinutes(
          endDateObj.getMinutes() + endDateObj.getTimezoneOffset()
        );

        while (currentDate <= endDateObj) {
          dates.push(currentDate.toISOString().split("T")[0]);
          currentDate.setDate(currentDate.getDate() + 1);
        }
        return dates;
      })();
    } else {
      rangeText = ""; // Clear range if one or both dates are not selected
    }
  });

  // Reactive effect for date validation: Ensure end date is not before start date.
  $effect(() => {
    if (selectedStartDate && selectedEndDate) {
      const start = new Date(selectedStartDate);
      const end = new Date(selectedEndDate);
      if (end < start) {
        // If end date is before start date, auto-correct end date to be the same as start date.
        const temp = selectedEndDate;
        selectedEndDate = selectedStartDate;
        selectedStartDate = temp;
      }
      if (start.getTime() === end.getTime()) {
        selectedEndDate = undefined;
        alert("End Date should be bigger than start date");
      }
    }
  });
</script>

<div class="flex justify-between text-start my-2 items-center">
  <label for="">Range</label>
  <input type="checkbox" bind:checked={isRange} />
</div>
<div class="md:flex md:justify-around text-start gap-2">
  <div class="w-full">
    <label for="startDate">{isRange ? "Start Date" : "Select Date"}</label>
    <input type="date" id="startDate" bind:value={selectedStartDate} required />
  </div>
  {#if isRange}
    <div transition:fade class="w-full">
      <label for="endDate">End Date</label>
      <input type="date" id="endDate" bind:value={selectedEndDate} />
    </div>
  {/if}
</div>
