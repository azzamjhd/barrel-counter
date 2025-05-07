<script lang="ts">
    import type { PageData } from './$types';

    let { data }: { data: PageData } = $props();
</script>

<input type="file">
<progress value="49" max="100"></progress>
<input type="button" value="Upload">