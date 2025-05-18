<script lang="ts">
  import { onMount } from 'svelte';

  let files = $state();
  let selectedFile: string = $state('');

  function getFileList() {
    fetch('/listFiles', {
      method: 'GET',
      headers: { 'Content-Type': 'application/json' },
    })
      .then((response) => {
        if (!response.ok) {
          console.error('Failed to get file list:', response.statusText);
        } else {
          return response.json();
        }
      })
      .then((data) => {
        if (data) {
          console.log('File list:', data);
          files = data;
        } else {
          files = ["/files", "/not", "/found", "/data/date.csv"];
        }
      })
      .catch((error) => {
        console.error('Error getting file list:', error);
      });
  }

  function deleteFile(fileName: string) {
    fetch('/deleteFile', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ file: fileName }),
    })
      .then((response) => {
        if (!response.ok) {
          console.error('Failed to delete file:', response.statusText);
        } else {
          console.log('File deleted successfully');
        }
      })
      .catch((error) => {
        console.error('Error deleting file:', error);
      });
  }

  onMount(() => {
    getFileList();
  });
</script>

<svelte:head>
  <title>File Management</title>
  <meta name="description" content="Manage your files" />
</svelte:head>

<div class="flex justify-between">
  <h1>Files</h1>
  <div class="flex gap-2">
    <a href={'/data' + selectedFile} role="button">Open</a>
    <button onclick={() => deleteFile(selectedFile)}>Delete</button>
  </div>
</div>

<fieldset>
  {#each files as file}
  <label >
    <input type="radio" name="file" bind:group={selectedFile} value={file}/>
    {file}
  </label>
  {/each}
</fieldset>