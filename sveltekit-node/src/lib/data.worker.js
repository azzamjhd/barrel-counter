// src/lib/data.worker.js

let db; // Holds the database connection
const DB_NAME = 'ProduksiDB';
const STORE_NAME = 'HistoriProduksi';
const VERSION = 11; // <-- Incremented version for parsing logic change

// Store header mapping globally in the worker
let workerGlobalOriginalHeaders = [];
let workerGlobalSanitizedHeaders = [];

/**
 * Sanitizes a string to be a valid IndexedDB keyPath (and JS property name).
 * Replaces spaces with underscores and removes other problematic characters.
 * @param {string} header
 * @returns {string}
 */
function sanitizeKey(header) {
	if (typeof header !== 'string') return '';
	return header.replace(/\s+/g, '_').replace(/[^a-zA-Z0-9_]/g, '');
}

/**
 * Initializes the IndexedDB database and object store.
 * @param {string[]} sanitizedHeadersForDB - An array of SANITIZED headers from the CSV to create indexes.
 */
function initDB(sanitizedHeadersForDB = []) {
	console.log(`[Worker V${VERSION}] initDB called. Sanitized headers for DB upgrade:`, sanitizedHeadersForDB);
	return new Promise((resolve, reject) => {
		const request = indexedDB.open(DB_NAME, VERSION);

		request.onerror = (event) => {
			const error = event.target.error;
			let errorMessage = 'Unknown IndexedDB error during open';
			if (error) {
				errorMessage = `IndexedDB Open Error: ${error.name} - ${error.message}`;
			} else if (event.target.errorCode) {
				errorMessage = `IndexedDB Open Error Code: ${event.target.errorCode}`;
			}
			console.error(`[Worker V${VERSION}] initDB onerror:`, errorMessage, event);
			reject(errorMessage);
		};

		request.onsuccess = (event) => {
			console.log(`[Worker V${VERSION}] initDB onsuccess: Database opened successfully.`);
			db = event.target.result; 
			db.onversionchange = () => { 
				console.warn(`[Worker V${VERSION}] Database version change detected elsewhere. Closing connection.`);
				if(db) db.close(); // Check if db is not null before closing
			};
			db.onerror = (event) => {
				console.error(`[Worker V${VERSION}] Database error (on db object): `, event.target.error);
			};
			resolve(db);
		};

		request.onupgradeneeded = (event) => {
			console.log(`[Worker V${VERSION}] initDB onupgradeneeded: Upgrading database schema.`);
			const dbInstance = event.target.result;
			const transaction = event.target.transaction;

			transaction.onerror = (transEvent) => {
				console.error(`[Worker V${VERSION}] onupgradeneeded transaction error:`, transEvent.target.error);
			};
			transaction.onabort = (transEvent) => {
				console.error(`[Worker V${VERSION}] onupgradeneeded transaction aborted:`, transEvent.target.error);
			}

			try {
				console.log(`[Worker V${VERSION}] Current object store names:`, Array.from(dbInstance.objectStoreNames));
				if (dbInstance.objectStoreNames.contains(STORE_NAME)) {
					console.log(`[Worker V${VERSION}] Deleting existing object store: ${STORE_NAME}`);
					dbInstance.deleteObjectStore(STORE_NAME);
				}
				console.log(`[Worker V${VERSION}] Creating new object store: ${STORE_NAME}`);
				const objectStore = dbInstance.createObjectStore(STORE_NAME, { autoIncrement: true });
				console.log(`[Worker V${VERSION}] Object store ${STORE_NAME} created.`);

				if (sanitizedHeadersForDB && sanitizedHeadersForDB.length > 0) {
					console.log(`[Worker V${VERSION}] Creating indexes for sanitized headers:`, sanitizedHeadersForDB);
					sanitizedHeadersForDB.forEach(sHeader => {
						if (sHeader && sHeader.trim() !== '') {
							if (!objectStore.indexNames.contains(sHeader)) {
								console.log(`[Worker V${VERSION}] Creating index: name='${sHeader}', keyPath='${sHeader}'`);
								objectStore.createIndex(sHeader, sHeader, { unique: false });
								console.log(`[Worker V${VERSION}] Index '${sHeader}' created.`);
							} else {
								console.log(`[Worker V${VERSION}] Index '${sHeader}' already exists.`);
							}
						} else {
							console.warn(`[Worker V${VERSION}] Skipped creating index for empty or invalid sanitized header:`, sHeader);
						}
					});
				} else {
					console.warn(`[Worker V${VERSION}] No sanitized headers provided for DB. No specific indexes will be created.`);
				}
				console.log(`[Worker V${VERSION}] onupgradeneeded: Schema upgrade operations defined.`);
			} catch (e) {
				console.error(`[Worker V${VERSION}] Error within onupgradeneeded try-catch block:`, e);
				if (transaction && typeof transaction.abort === 'function') {
					transaction.abort();
				}
				reject(`Error during schema upgrade: ${e.message}`);
				return;
			}
		};

		request.onblocked = (event) => {
			console.warn(`[Worker V${VERSION}] initDB open request is blocked. Old version: ${event.oldVersion}, New version: ${event.newVersion}`, event);
			reject('Database open request is blocked. Please close other tabs/connections to this page and try reloading.');
		};
	});
}

/**
 * Parses CSV text. Data object keys will be sanitized for DB storage.
 * Stores original and sanitized headers globally in the worker.
 * @param {string} csvText The raw CSV string.
 * @returns {{dataForDB: any[]}} - Data with sanitized keys
 */
function parseCSV(csvText) {
	console.log(`[Worker V${VERSION}] parseCSV called.`);
	const lines = csvText.trim().split('\n');
	if (lines.length === 0) throw new Error('CSV is empty.');
    if (lines.length === 1 && lines[0].trim() === '') throw new Error('CSV is empty or contains only whitespace.');

	workerGlobalOriginalHeaders = lines[0].split(',').map((h) => String(h || '').trim());
	if (workerGlobalOriginalHeaders.length === 0 || workerGlobalOriginalHeaders.every(h => h === '')) {
		throw new Error('CSV headers are missing, empty, or invalid.');
	}
	console.log(`[Worker V${VERSION}] Stored Original headers:`, workerGlobalOriginalHeaders);

	workerGlobalSanitizedHeaders = workerGlobalOriginalHeaders.map(h => sanitizeKey(h));
	console.log(`[Worker V${VERSION}] Stored Sanitized headers for object keys:`, workerGlobalSanitizedHeaders);

	const dataForDB = lines.slice(1).map((line) => {
		const values = line.split(',');
		const entry = {};
		workerGlobalSanitizedHeaders.forEach((sHeader, index) => {
			const originalHeader = workerGlobalOriginalHeaders[index]; // Get original header for type checking
			const value = values[index];
            const trimmedValue = (value !== undefined && value !== null) ? String(value).trim() : '';
			
            let finalValue;
            // Check if the original header suggests it's a date/time column
            if (originalHeader && (originalHeader.toLowerCase().includes('time') || originalHeader.toLowerCase().includes('date'))) {
                finalValue = trimmedValue; // Keep as string for date/time
				console.log(`[Worker V${VERSION}] Keeping value for header '${originalHeader}' as string: '${finalValue}'`);
            } else if (trimmedValue === '') {
                finalValue = ''; // Keep empty strings as is
            } else {
                const num = parseFloat(trimmedValue);
                if (!isNaN(num)) { // Check if it's a valid number (incl. 0, floats)
                    finalValue = num;
                } else {
                    finalValue = trimmedValue; // Keep as string if not a valid number
                }
            }
			entry[sHeader] = finalValue;
		});
		return entry;
	});
	console.log(`[Worker V${VERSION}] Parsed ${dataForDB.length} data rows. First row example (sanitized keys, parsed values):`, dataForDB.length > 0 ? dataForDB[0] : 'N/A');
	return { dataForDB };
}


/**
 * Stores data in IndexedDB. Assumes data objects have sanitized keys.
 * @param {any[]} dataToStore - The array of objects (with sanitized keys) to store.
 */
function storeData(dataToStore) {
    console.log(`[Worker V${VERSION}] storeData called.`);
    if (!db) {
        console.error(`[Worker V${VERSION}] Database not initialized. Cannot store data.`);
        return Promise.reject('Database not initialized. Cannot store data.');
    }
    const transaction = db.transaction([STORE_NAME], 'readwrite');
	const objectStore = transaction.objectStore(STORE_NAME);
    
    console.log(`[Worker V${VERSION}] Clearing old data from ${STORE_NAME}.`);
    objectStore.clear(); 

	console.log(`[Worker V${VERSION}] Adding ${dataToStore.length} new items to ${STORE_NAME}.`);
	dataToStore.forEach(item => objectStore.add(item));

	return new Promise((resolve, reject) => {
		transaction.oncomplete = () => {
            console.log(`[Worker V${VERSION}] storeData transaction completed.`);
            resolve(dataToStore.length);
        }
		transaction.onerror = (event) => {
            const error = event.target.error;
            let errorMessage = 'Unknown error during storeData transaction';
            if(error) {
                errorMessage = `StoreData Transaction Error: ${error.name} - ${error.message}`;
            }
			console.error(`[Worker V${VERSION}] storeData transaction error:`, errorMessage, event);
			reject(errorMessage);
        }
        transaction.onabort = (event) => {
            const error = event.target.error;
            let errorMessage = 'storeData transaction aborted';
             if(error) {
                errorMessage = `StoreData Transaction Abort: ${error.name} - ${error.message}`;
            }
            console.error(`[Worker V${VERSION}] storeData transaction aborted:`, errorMessage, event);
            reject(errorMessage);
        }
	});
}

/**
 * Transforms data from DB (with sanitized keys) to data for page (with original keys).
 * @param {any[]} dataFromDB - Array of objects with sanitized keys.
 * @returns {any[]} - Array of objects with original keys.
 */
function transformDataForPage(dataFromDB) {
    if (!workerGlobalOriginalHeaders.length || !workerGlobalSanitizedHeaders.length || workerGlobalOriginalHeaders.length !== workerGlobalSanitizedHeaders.length) {
        console.warn(`[Worker V${VERSION}] Header mapping not available or mismatched for transforming data for page. Orig: ${workerGlobalOriginalHeaders.length}, Sanitized: ${workerGlobalSanitizedHeaders.length}`);
        return dataFromDB;
    }
    return dataFromDB.map(itemFromDB => {
        const itemForPage = {};
        workerGlobalOriginalHeaders.forEach((origHeader, index) => {
            const sanitizedHeader = workerGlobalSanitizedHeaders[index];
            if (itemFromDB.hasOwnProperty(sanitizedHeader)) {
                itemForPage[origHeader] = itemFromDB[sanitizedHeader];
            } else {
                itemForPage[origHeader] = undefined; 
                console.warn(`[Worker V${VERSION}] Missing sanitized key '${sanitizedHeader}' in itemFromDB during transform. Item:`, itemFromDB);
            }
        });
        return itemForPage;
    });
}


/**
 * Fetches a sorted and paginated set of data from IndexedDB.
 * Returns data with original keys for the page component.
 * @param {{page: number, pageSize: number, sortKey: string (original header), sortDir: 'asc' | 'desc'}} query
 */
function fetchData(query) {
    console.log(`[Worker V${VERSION}] fetchData called with query (original sortKey):`, query);
    if (!db) {
        console.error(`[Worker V${VERSION}] Database not initialized for fetchData. This should not happen if loadCSV ran successfully.`);
        return Promise.reject('Database not initialized for fetchData.');
    }
    const { page, pageSize, sortKey: originalSortKey, sortDir } = query;
    const sanitizedSortKeyForDB = sanitizeKey(originalSortKey);

    return new Promise((resolve, reject) => {
        if (!db.objectStoreNames.contains(STORE_NAME)) {
            console.error(`[Worker V${VERSION}] Object store '${STORE_NAME}' does not exist in fetchData. DB might be in an inconsistent state.`);
            return reject(`Object store '${STORE_NAME}' not found.`);
        }
        const transaction = db.transaction([STORE_NAME], 'readonly');
        const store = transaction.objectStore(STORE_NAME);
        
        let indexToUse;
        if (sanitizedSortKeyForDB && store.indexNames.contains(sanitizedSortKeyForDB)) {
            indexToUse = store.index(sanitizedSortKeyForDB);
            console.log(`[Worker V${VERSION}] Using index: '${indexToUse.name}' (keyPath: '${indexToUse.keyPath}', sanitized from '${originalSortKey}') for fetching.`);
        } else {
            indexToUse = store; 
            console.warn(`[Worker V${VERSION}] Sanitized sort key '${sanitizedSortKeyForDB}' (from '${originalSortKey}') is not a valid index or not provided. Fetching by primary key order (store order). Index name: ${indexToUse.name}`);
        }
        
        const resultsFromDB = [];
        const cursorDir = sortDir === 'asc' ? 'next' : 'prev'; 
        
        const offset = (page - 1) * pageSize;
        let advancedOnce = false; 
        let itemsAdded = 0;

        console.log(`[Worker V${VERSION}] Opening cursor. Index/Store: ${indexToUse.name}, Direction: ${cursorDir}, Offset: ${offset}, PageSize: ${pageSize}`);
        const cursorRequest = indexToUse.openCursor(null, cursorDir);

        cursorRequest.onsuccess = event => {
            const cursor = event.target.result;
            if (cursor) {
                console.log(`[Worker V${VERSION}] Cursor iteration on index '${indexToUse.name}': Key='${cursor.key}' (Type: ${typeof cursor.key}), PrimaryKey='${cursor.primaryKey}'`);

                if (!advancedOnce && offset > 0) {
                    advancedOnce = true;
                    console.log(`[Worker V${VERSION}] Advancing cursor by ${offset}`);
                    cursor.advance(offset);
                    return; 
                }
                if (itemsAdded < pageSize) {
                    resultsFromDB.push(cursor.value); 
                    itemsAdded++;
                    cursor.continue();
                } else {
                    const dataForPage = transformDataForPage(resultsFromDB);
                    console.log(`[Worker V${VERSION}] Fetched page of ${dataForPage.length} items (transformed for page).`);
                    resolve(dataForPage); 
                }
            } else {
                const dataForPage = transformDataForPage(resultsFromDB);
                console.log(`[Worker V${VERSION}] Cursor finished. Fetched ${dataForPage.length} items in total for this page (transformed for page).`);
                resolve(dataForPage); 
            }
        };
        cursorRequest.onerror = event => {
            const error = event.target.error;
            let errorMessage = 'Unknown error during fetchData cursor';
            if(error) {
                errorMessage = `FetchData Cursor Error: ${error.name} - ${error.message}`;
            }
            console.error(`[Worker V${VERSION}] fetchData cursor error:`, errorMessage, event);
            reject(errorMessage);
        }
    });
}


// --- Main Worker Message Handler ---
self.onmessage = async (event) => {
	const { action, payload } = event.data;
	let payloadLog = "No payload";
	if (payload) {
		if (typeof payload === 'string') {
			payloadLog = payload.substring(0,100) + (payload.length > 100 ? "..." : "");
		} else if (typeof payload === 'object') {
			try {
				payloadLog = JSON.stringify(payload);
			} catch (e) {
				payloadLog = "Unserializable object";
			}
		} else {
			payloadLog = String(payload);
		}
	}
	console.log(`[Worker V${VERSION}] Received message: Action - ${action}`, payloadLog);


	try {
		if (action === 'loadCSV') {
			if (db) {
				console.log(`[Worker V${VERSION}] Closing existing database connection before deletion.`);
				db.close(); 
				db = null; 
				console.log(`[Worker V${VERSION}] Existing database connection closed.`);
			}
			
			console.log(`[Worker V${VERSION}] Deleting database '${DB_NAME}' to ensure a clean schema for the new data.`);
			const deleteRequest = indexedDB.deleteDatabase(DB_NAME);
			
			await new Promise((resolveDelete, rejectDelete) => {
				deleteRequest.onsuccess = () => {
					console.log(`[Worker V${VERSION}] Database '${DB_NAME}' deleted successfully.`);
					resolveDelete();
				};
				deleteRequest.onerror = (e) => {
					console.error(`[Worker V${VERSION}] Error deleting database.`, e.target.error);
					rejectDelete(`Could not delete old database: ${e.target.error?.message}`);
				};
				deleteRequest.onblocked = (e) => {
					console.warn(`[Worker V${VERSION}] Deleting database is blocked. Old version: ${e.oldVersion}, New version: ${e.newVersion}. This usually means other tabs/connections are open.`);
					rejectDelete('Database deletion blocked. Please close other tabs using this page and try reloading.');
				};
			});

			const { dataForDB } = parseCSV(payload); 
			await initDB(workerGlobalSanitizedHeaders); 
            
			const totalEntries = await storeData(dataForDB);
			
			const defaultSortKey = workerGlobalOriginalHeaders.length > 0 ? workerGlobalOriginalHeaders[0] : ''; 
			
            const initialDataForPage = await fetchData({ page: 1, pageSize: 10, sortKey: defaultSortKey, sortDir: 'asc' });
			
			console.log(`[Worker V${VERSION}] loadCSV complete. Posting message back to main thread.`);
			self.postMessage({ action: 'loadComplete', payload: { headers: workerGlobalOriginalHeaders, totalEntries, initialData: initialDataForPage } });

		} else if (action === 'fetch') {
			if (!db) { 
                console.warn(`[Worker V${VERSION}] DB not initialized at fetch. This should not happen if loadCSV ran successfully.`);
                await initDB(workerGlobalSanitizedHeaders); 
            }
			const dataForPage = await fetchData(payload); 
			console.log(`[Worker V${VERSION}] fetch complete. Posting message back to main thread.`);
			self.postMessage({ action: 'dataFetched', payload: dataForPage });
		}
	} catch (error) {
        const errorMessage = (error instanceof Error) ? `${error.name}: ${error.message}` : String(error);
		console.error(`[Worker V${VERSION}] Error in onmessage handler:`, errorMessage, error);
		self.postMessage({ action: 'error', payload: errorMessage });
	}
};

console.log(`[Worker V${VERSION}] Script loaded and ready.`);
