<script lang="ts">
  import { onMount } from "svelte";
  let darkTheme = $state(false);

  onMount(() => {
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
  function submitForm(event: Event) {
    event.preventDefault(); // Prevent the default form submission
    const form = event.target as HTMLFormElement;
    const formData = new FormData(form);
    const data = Object.fromEntries(formData.entries());
    console.log("Form submitted:", data);
    fetch("/saveSettings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(data),
    })
      .then((response) => {
        if (!response.ok) {
          console.error("Failed to save settings:", response.statusText);
        } else {
          console.log("Settings saved successfully");
        }
      })
      .catch((error) => {
        console.error("Error saving settings:", error);
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
  <label>Reset Counter</label>
  <div class="flex justify-between gap-2 items-center px-3">
    <div class="w-auto text-center self-center">0</div>
    <button id="reset-counter" class="hidded">Reset</button>
  </div>
  <hr />
  <label for="set-time-local">Set Local Time</label>
  <div class="flex justify-between gap-2 items-center px-3">
    <div class="text-center self-center">2025/02/01 13:54:39</div>
    <button onclick={setTime} id="set-time-local" class="hidded">Set</button>
  </div>
  <hr />
  <label for="sensor-type">Set Sensor</label>
  <div class="my-2 mx-3">
    <form>
      <select name="sensor-type" id="sensor-type" required>
        <option value="" selected disabled>Select your sensor</option>
        <option value="">Active High</option>
        <option value="">Active Low</option>
      </select>
      <input type="submit" value="Save" />
    </form>
  </div>
  <hr />
  <label for="wifi-setting">Set WiFi Connection</label>
  <form onsubmit={submitForm} id="wifi-setting" class="my-2 mx-3">
    <div class="md:flex justify-between gap-2">
      <input name="ssid" type="ssid" placeholder="SSID" required />
      <input name="password" type="password" placeholder="Password" required />
    </div>
    <input type="submit" value="Save" />
  </form>
</article>
