<script lang="ts">
	import { onMount, onDestroy } from 'svelte';

	/**
	 * The URL from which to fetch the CSV data.
	 * @type {string}
	 */
	export let csvUrl: string;

	/**
	 * Enable or disable periodic data polling.
	 * @type {boolean}
	 */
	export let enablePolling: boolean = false;

	/**
	 * Interval for data polling in milliseconds.
	 * @type {number}
	 */
	export let pollingInterval: number = 5000; // Default to 5 seconds


	// --- State Management ---
	let tableData: any[] = [];
	let columns: string[] = [];
	let totalEntries = 0;
	let isLoading = true;
	let isLoadingMessage = 'Initializing...';
	let errorMessage = '';

	let pageSize = 10;
	let currentPage = 1;
	let totalPages = 1;

	let sortKey = '';
	let sortDirection = 'asc';

	let worker: Worker | undefined = undefined;
	let componentMounted = false;
	let pollingTimerId: any = null; // NodeJS.Timeout or number

	async function loadDataForUrl(url: string, isPollingUpdate = false) {
		if (!url) {
			errorMessage = "CSV URL is invalid or not provided.";
			isLoading = false;
			isLoadingMessage = '';
			console.error(errorMessage);
			clearTableState();
			return;
		}

		console.log(`[HistoryTable] loadDataForUrl called for: ${url}. Is polling: ${isPollingUpdate}`);
		isLoading = true;
		if (!isPollingUpdate) { // Show full loading message only for initial/manual loads
			isLoadingMessage = 'Initializing Web Worker...';
		} else {
			isLoadingMessage = 'Refreshing data...'; // More subtle message for polling
		}
		errorMessage = '';

		if (worker && !isPollingUpdate) { // Terminate worker only on manual URL change, not for polling the same URL
			console.log('[HistoryTable] Terminating existing Web Worker before loading new URL (manual change).');
			worker.terminate();
			worker = undefined;
		}
		
		if (!isPollingUpdate) { // Reset table state fully for manual loads/initial load
			clearTableState();
		}


		try {
			if (!worker) { // Create worker if it doesn't exist (initial load or after manual URL change)
				worker = new Worker(new URL('$lib/data.worker.js', import.meta.url), { type: 'module' });
				console.log('[HistoryTable] New Web Worker created for URL:', url);
				worker.onmessage = handleWorkerMessage;
				worker.onerror = (err) => {
					console.error('[HistoryTable] Uncaught error from Web Worker:', err);
					errorMessage = `Worker error: ${err.message}. Check console for details.`;
					isLoading = false;
				};
			}
			
			if (!isPollingUpdate) isLoadingMessage = `Fetching CSV data from ${url}...`;
			const response = await fetch(url);
			if (!response.ok) {
				throw new Error(`Failed to fetch CSV from ${url}: ${response.status} ${response.statusText}`);
			}
			const csvText = await response.text();
			if (!isPollingUpdate) console.log('[HistoryTable] CSV data fetched. Sending to worker.');
			if (!isPollingUpdate) isLoadingMessage = 'Processing CSV data...';
			
			// Pass a flag to the worker if it's a polling update, in case it wants to optimize (not used yet by worker)
			worker.postMessage({ action: 'loadCSV', payload: csvText, isPolling: isPollingUpdate });

		} catch (error: any) {
			console.error('[HistoryTable] Error during loadDataForUrl:', error);
			errorMessage = error.message || 'An unknown error occurred during data loading.';
			isLoading = false;
			isLoadingMessage = '';
		}
	}
	
	function clearTableState() {
		tableData = [];
		columns = [];
		totalEntries = 0;
		currentPage = 1;
		totalPages = 1;
		// sortKey = ''; // Keep sortKey if user had one, or reset based on new columns later
	}


	function setupPolling() {
		if (pollingTimerId) {
			clearInterval(pollingTimerId);
			pollingTimerId = null;
			console.log('[HistoryTable] Cleared existing polling timer.');
		}
		if (enablePolling && csvUrl && pollingInterval > 0) {
			console.log(`[HistoryTable] Setting up polling for ${csvUrl} every ${pollingInterval}ms.`);
			pollingTimerId = setInterval(() => {
				console.log('[HistoryTable] Polling for new data...');
				loadDataForUrl(csvUrl, true); // Pass true for isPollingUpdate
			}, pollingInterval);
		}
	}

	// --- Component Lifecycle ---
	onMount(() => {
		console.log('[HistoryTable] Component mounting.');
		loadDataForUrl(csvUrl);
		setupPolling();
		componentMounted = true;
	});

	// Reactive statements for prop changes
	$: if (componentMounted && csvUrl) { // Handles manual URL changes from parent
		console.log('[HistoryTable] csvUrl prop changed to:', csvUrl, 'Reloading data and resetting poll.');
		loadDataForUrl(csvUrl);
		setupPolling(); // Reset polling with new URL if applicable
	}
	
	$: if (componentMounted) { // Handles changes to polling settings
		console.log(`[HistoryTable] Polling settings changed. enablePolling: ${enablePolling}, pollingInterval: ${pollingInterval}. Resetting poll.`);
		setupPolling();
	}


	onDestroy(() => {
		console.log('[HistoryTable] Component destroying.');
		if (worker) {
			console.log('[HistoryTable] Terminating Web Worker.');
			worker.terminate();
		}
		if (pollingTimerId) {
			clearInterval(pollingTimerId);
			console.log('[HistoryTable] Cleared polling timer on destroy.');
		}
	});

	// --- Worker Communication ---
	function handleWorkerMessage(event: MessageEvent) {
		const { action, payload } = event.data;
		// console.log('[HistoryTable] Received message from worker:', action, payload);

		if (action === 'loadComplete') {
			columns = payload.headers || [];
			totalEntries = payload.totalEntries || 0;
			tableData = payload.initialData || []; // This is page 1 data
			totalPages = Math.ceil(totalEntries / pageSize) || 1;
			
			// Preserve sort key if it's still valid, otherwise set to first column or clear
			if (columns.length > 0) {
				if (!sortKey || !columns.includes(sortKey)) {
					sortKey = columns[0];
					// sortDirection = 'asc'; // Optionally reset sort direction too
				}
			} else {
				sortKey = '';
			}
			currentPage = 1; // Always reset to page 1 on full load/refresh for simplicity with live data

			isLoading = false;
			isLoadingMessage = '';
			console.log(`[HistoryTable] Load complete. Columns: ${columns.length}, Entries: ${totalEntries}, Current Page: ${currentPage}, Sort: ${sortKey} ${sortDirection}`);

			// If a sortKey exists, we need to fetch the sorted data for page 1
			if (sortKey && columns.includes(sortKey)) {
				console.log('[HistoryTable] Re-fetching data for page 1 with current sort after loadComplete.');
				requestDataFromWorker(); // This will use current sortKey, sortDirection, and currentPage (which is 1)
			}


		} else if (action === 'dataFetched') {
			tableData = payload || [];
			isLoading = false;
			isLoadingMessage = '';
			console.log('[HistoryTable] Data fetched for current page.');
		} else if (action === 'error') {
			errorMessage = payload || 'Unknown worker error.';
			isLoading = false;
			isLoadingMessage = '';
			console.error('[HistoryTable] Error message from worker:', payload);
		}
	}

	function requestDataFromWorker() {
		if (!worker) {
			console.warn('[HistoryTable] Worker not available for requestDataFromWorker.');
			errorMessage = 'Worker not available. Please reload.';
			isLoading = false;
			return;
		}
		isLoading = true;
		// isLoadingMessage = 'Fetching data...'; // Keep this subtle for polling
		console.log('[HistoryTable] Requesting data from worker. Page:', currentPage, 'SortKey:', sortKey, 'Dir:', sortDirection);
		worker.postMessage({
			action: 'fetch',
			payload: {
				page: currentPage,
				pageSize: pageSize,
				sortKey: sortKey,
				sortDir: sortDirection
			}
		});
	}

	// --- UI Event Handlers ---
	function handleSort(key: string) {
		console.log('[HistoryTable] handleSort called for key:', key);
		if (sortKey === key) {
			sortDirection = sortDirection === 'asc' ? 'desc' : 'asc';
		} else {
			sortKey = key;
			sortDirection = 'asc';
		}
		currentPage = 1; // Reset to page 1 when sort changes
		requestDataFromWorker();
	}

	function handlePageSizeChange() {
		currentPage = 1; // Reset to page 1 when page size changes
		totalPages = Math.ceil(totalEntries / pageSize);
		console.log('[HistoryTable] Page size changed to:', pageSize);
		requestDataFromWorker();
	}

	function nextPage() {
		if (currentPage < totalPages) {
			currentPage++;
			console.log('[HistoryTable] Next page:', currentPage);
			requestDataFromWorker();
		}
	}

	function prevPage() {
		if (currentPage > 1) {
			currentPage--;
			console.log('[HistoryTable] Previous page:', currentPage);
			requestDataFromWorker();
		}
	}
</script>

<!-- <div class="bg-white dark:bg-gray-800 p-4 sm:p-6 rounded-xl shadow-lg font-inter"> -->
 <article>
	<div class="flex flex-col sm:flex-row items-start sm:items-center justify-between space-y-2 sm:space-y-0 sm:space-x-2 mb-4">
		<div class="flex items-center space-x-2">
			<select
				bind:value={pageSize}
				on:change={handlePageSizeChange}
				class="border border-gray-300 dark:border-gray-600 rounded-md px-3 py-2 bg-white dark:bg-gray-700 text-gray-700 dark:text-gray-200 focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				aria-label="Entries per page"
				disabled={isLoading || !!errorMessage || totalEntries === 0}
			>
				<option value={10}>10</option>
				<option value={20}>20</option>
				<option value={50}>50</option>
				<option value={100}>100</option>
			</select>
			<span class="text-gray-600 dark:text-gray-300">entries per page</span>
		</div>
		{#if totalEntries > 0 && !isLoading && !errorMessage}
		<div class="text-sm text-gray-600 dark:text-gray-400">
			Total {totalEntries} records {#if isLoading && isLoadingMessage === 'Refreshing data...'} (Refreshing...){/if}
		</div>
		{/if}
	</div>

	{#if isLoading && tableData.length === 0 && !errorMessage && isLoadingMessage !== 'Refreshing data...'}
		<div class="text-center p-8 text-gray-500 dark:text-gray-400">
			<div class="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500 mx-auto mb-2"></div>
			{isLoadingMessage}
		</div>
	{:else if errorMessage}
		<div class="text-center p-8 text-red-500 dark:text-red-400 bg-red-50 dark:bg-red-900/20 rounded-lg">
			<strong class="block">Error:</strong> {errorMessage}
			<p class="text-sm mt-1">Please check the browser console for more details. You might need to provide a valid CSV URL or check the file.</p>
		</div>
	{:else if columns.length === 0 && !isLoading}
		<div class="text-center p-8 text-gray-500 dark:text-gray-400">
			No data to display. This could be due to an empty CSV file or an issue with parsing the headers.
		</div>
	{:else}
		<div class="overflow-x-auto relative">
			{#if isLoading && isLoadingMessage === 'Refreshing data...'}
				<div class="absolute top-2 right-2 text-xs text-blue-500 dark:text-blue-400 flex items-center">
					<div class="animate-spin rounded-full h-3 w-3 border-b-2 border-blue-500 mr-1"></div>
					Refreshing...
				</div>
			{:else if isLoading && tableData.length > 0}
				<div class="absolute inset-0 bg-white/70 dark:bg-black/70 flex items-center justify-center z-10 rounded-md">
					<div class="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500"></div>
				</div>
			{/if}
			<table class="w-full text-left text-sm text-gray-700 dark:text-gray-300">
				<thead class="bg-gray-100 dark:bg-gray-700 text-xs text-gray-700 dark:text-gray-200 uppercase sticky top-0 z-5">
					<tr>
						{#each columns as column (column)}
							<th scope="col" class="px-6 py-3 font-semibold text-center">
								<button
									class="flex justify-center items-center space-x-1 hover:text-blue-600 dark:hover:text-blue-400 transition-colors duration-150 w-full text-left disabled:opacity-50 disabled:cursor-not-allowed"
									on:click={() => handleSort(column)}
									aria-label="Sort by {column}"
									disabled={isLoading || !!errorMessage}
								>
									<span>{column}</span>
									{#if sortKey === column}
										<span class="text-lg leading-none">{sortDirection === 'asc' ? '🔼' : '🔽'}</span>
									{/if}
								</button>
							</th>
						{/each}
					</tr>
				</thead>
				<tbody class="divide-y divide-gray-200 dark:divide-gray-700">
					{#if tableData.length === 0 && !isLoading}
						<tr>
							<td colspan={columns.length || 1} class="px-6 py-12 text-center text-gray-500 dark:text-gray-400">
								No records match your current filter or page.
							</td>
						</tr>
					{/if}
					{#each tableData as row, i (row.id || JSON.stringify(row) + i)}
						<tr class="hover:bg-gray-50 dark:hover:bg-gray-600/50 transition-colors duration-150">
							{#each columns as column (column)}
								<td class="px-6 py-4 whitespace-nowrap text-center">{row[column]}</td>
							{/each}
						</tr>
					{/each}
				</tbody>
			</table>
		</div>

		{#if totalEntries > 0}
		<div class="flex flex-col sm:flex-row justify-between items-center mt-6 pt-4 border-t border-gray-200 dark:border-gray-700 text-sm text-gray-600 dark:text-gray-400">
			<div>
				Showing
				<strong>{(currentPage - 1) * pageSize + 1}</strong>
				to
				<strong>{Math.min(currentPage * pageSize, totalEntries)}</strong>
				of
				<strong>{totalEntries}</strong> entries
			</div>
			<div class="flex items-center space-x-1 mt-4 sm:mt-0">
				<button
					class="px-3 py-2 border border-gray-300 dark:border-gray-600 rounded-md hover:bg-gray-100 dark:hover:bg-gray-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
					on:click={prevPage}
					disabled={currentPage === 1 || isLoading || !!errorMessage}
					aria-label="Previous page"
				>
					Previous
				</button>
				<span class="px-2 py-1">Page {currentPage} of {totalPages}</span>
				<button
					class="px-3 py-2 border border-gray-300 dark:border-gray-600 rounded-md hover:bg-gray-100 dark:hover:bg-gray-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
					on:click={nextPage}
					disabled={currentPage === totalPages || isLoading || !!errorMessage}
					aria-label="Next page"
				>
					Next
				</button>
			</div>
		</div>
		{/if}
	{/if}
</article>
<!-- </div> -->
