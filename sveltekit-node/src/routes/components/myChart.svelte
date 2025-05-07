<script lang="ts">
  import * as Highcharts from 'highcharts';
  import 'highcharts/modules/exporting';
  import 'highcharts/modules/offline-exporting';
  import { Chart } from '@highcharts/svelte';

  let { name = 'Chart', subtitle = '', type = 'line', yLabel = '', series = $bindable([])} = $props();

  let options = $state({
      chart: { type: type },
      title: { text: name },
      subtitle: { text: subtitle },
      yAxis: { title: { text: yLabel } },
      series: series,
  });
</script>

<Chart options={options} highcharts={Highcharts} />