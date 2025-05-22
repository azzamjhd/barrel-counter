<script lang="ts">
  import { onMount } from "svelte";
  import ConfirmButton from "$lib/ConfirmButton.svelte";
  let darkTheme = $state(false);
  let timeEvtSource: EventSource;
  let countEvtSource: EventSource;
  let espTime: string = $state("");
  let count: number = $state(0);

  onMount(() => {
    getCurrentCount();

    const prefersDark = window.matchMedia("(prefers-color-scheme: dark)");
    darkTheme = prefersDark.matches;
    document.documentElement.setAttribute(
      "data-theme",
      darkTheme ? "dark" : "light"
    );
    prefersDark.addEventListener("change", (e) => {
      darkTheme = e.matches;
      document.documentElement.setAttribute(
        "data-theme",
        darkTheme ? "dark" : "light"
      );
    });

    countEvtSource = new EventSource("/counterStream");
    countEvtSource.onmessage = function (event) {
      const data = event.data;
      count = Number(data);
    };
    timeEvtSource = new EventSource("/timeStream");
    timeEvtSource.onmessage = function (event) {
      const data = event.data;
      espTime = data;
    };
  });

  function toggleTheme(event: Event) {
    const target = event.target as HTMLInputElement;
    darkTheme = target.checked;
    document.documentElement.setAttribute(
      "data-theme",
      darkTheme ? "dark" : "light"
    );
  }

  function setTime() {
    const now = Date.now();
    const localNow =
      Math.floor(now / 1000) - new Date().getTimezoneOffset() * 60;
    // console.log('Setting time:', localNow);
    fetch("/updateTime", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ time: localNow }),
    })
      .then((response) => {
        if (!response.ok) {
          console.error("Failed to set time:", response.statusText);
        } else {
          console.log("Time set successfully");
        }
      })
      .catch((error) => {
        console.error("Error setting time:", error);
      });
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

  function resetCounter() {
    fetch("/resetCount", {
      method: "GET",
      headers: { "Content-Type": "text/plain" },
    })
      .then((response) => {
        if (!response.ok) {
          console.error("Failed to reset counter:", response.statusText);
        } else {
          console.log("Counter reset successfully");
        }
      })
      .catch((error) => {
        console.error("Error resetting counter:", error);
      });
  }

  function handleFormSubmit(event: Event) {
    event.preventDefault();
    const form = event.target as HTMLFormElement;
    const formData = new FormData(form);
    const data = Object.fromEntries(formData.entries());

    fetch("/wifiSetting", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(data),
    })
      .then((response) => {
        if (!response.ok) {
          console.error("Failed to set WiFi settings:", response.statusText);
        } else {
          console.log("WiFi settings saved successfully");
        }
      })
      .catch((error) => {
        console.error("Error setting WiFi settings:", error);
      });
  }

  function restartEsp() {
    fetch("/restart", {
      method: "GET",
      headers: { "Content-Type": "text/plain" },
    })
      .then((response) => {
        if (!response.ok) {
          console.error("Failed to restart ESP:", response.statusText);
        } else {
          console.log("ESP restarted successfully");
        }
      })
      .catch((error) => {
        console.error("Error restarting ESP:", error);
      });
  }
</script>

<article>
  <header class="flex justify-between items-center">
    <h2>Settings</h2>
    <div class="flex gap-2">
      <div class="w-4 h-4 bg-red-500 rounded-full"></div>
      <div class="w-4 h-4 bg-lime-400 rounded-full"></div>
    </div>
  </header>
  <div class="flex justify-between text-start my-2 mx-3 items-center">
    <label>Theme </label>
    <input
      name="theme"
      type="checkbox"
      role="switch"
      onchange={toggleTheme}
      checked={darkTheme}
    />
  </div>
  <hr />
  <label for="reset-counter">Reset Counter</label>
  <div class="flex justify-between gap-2 items-center px-3">
    <div class="w-auto text-center self-center">{count}</div>
    <ConfirmButton
      title="Reset Counter"
      message="Are you sure you want to reset the counter?"
      confirmLabel="OK"
      cancelLabel="Cancel"
      btnLabel="Reset"
      onConfirmAction={resetCounter}
    />
  </div>
  <hr />
  <label for="set-time-local">Set Local Time</label>
  <div class="flex justify-between gap-2 items-center px-3">
    <div class="text-center self-center">{espTime ? espTime : "No Time Received"}</div>
    <ConfirmButton
      title="Set Local Time"
      message="Are you sure you want to set the RTC time to current device local time?"
      confirmLabel="OK"
      cancelLabel="Cancel"
      btnLabel="Set"
      onConfirmAction={setTime}
    />
  </div>
  <hr />
  <label for="wifi-setting">Set WiFi Connection</label>
  <form onsubmit={handleFormSubmit} id="wifi-setting" class="my-2 mx-3">
    <!-- <button>Scan WiFi</button> -->
    <div class="md:flex justify-between gap-2">
      <!-- <TextInputWithDropdown
        bind:value={fruitValue}
        items={allFruits}
        placeholder="Type to search fruits..."
        id="fruit-selector"
      /> -->
      <input id="ssid" name="ssid" type="ssid" placeholder="SSID" required />
      <input id="password" name="password" type="password" placeholder="Password" />
    </div>
    <input 
      type="submit"
      value="Save"
    />
  </form>

  <hr/>
  <div class="flex justify-between text-start my-2 mx-3 items-center">  
    <label for="data-files">Data Log Files</label>
    <a id="data-files" href="/files" class="btn btn-primary" role="button">Manage Files</a>
  </div>
  <hr/>
  <div class="flex justify-between text-start my-2 mx-3 items-center">
    <label for="restart">Restart Esp</label>
    <ConfirmButton
      title="Restart Esp"
      message="Are you sure you want to restart the ESP?"
      confirmLabel="OK"
      cancelLabel="Cancel"
      btnLabel="Restart"
      onConfirmAction={restartEsp}
    />
</article>
