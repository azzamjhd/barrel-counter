<script lang="ts">
  import { clickOutside as clickOutsideAction } from '$lib/actions/clickOutside'; // Adjust path if needed

  type Props = {
    value: string; // Bindable string value for the input. Pass a $state() signal from parent for two-way binding.
    items: string[]; // Array of strings for dropdown suggestions.
    placeholder?: string;
    label?: string;
    name?: string; // For native form submission
    id?: string; // Optional ID for the input element
    inputClass?: string; // Custom classes for the input element
    dropdownClass?: string; // Custom classes for the dropdown container
    itemClass?: string; // Custom classes for individual dropdown items
    activeItemClass?: string; // Custom classes for the active/highlighted dropdown item
  };

  // Props with defaults
  let {
    value, // This is a WritableSignal<string> if parent uses bind:value={$state(...)}
    items,
    placeholder = '',
    label = '',
    name = '',
    id = undefined,
    inputClass = 'w-full', // PicoCSS styles the input by default, Tailwind for sizing etc.
    dropdownClass = 'absolute z-10 w-full bg-white border border-gray-300 rounded-md shadow-lg mt-1 max-h-30 overflow-y-auto dark:bg-gray-800 dark:border-gray-600',
    itemClass = 'px-4 py-2 text-sm text-gray-700 hover:bg-gray-100 dark:text-gray-200 dark:hover:bg-gray-700 cursor-pointer',
    activeItemClass = 'bg-gray-100 dark:bg-gray-700'
  } = $props();

  // Generate a unique ID if not provided, for label association and ARIA attributes
  const _id = $derived(id || `input-dropdown-${Math.random().toString(36).substring(2, 9)}`);

  let dropdownVisible = $state(false);
  let activeIndex = $state(-1); // Index of the currently highlighted item in the dropdown
  let listElement = $state<HTMLUListElement | null>(null); // Reference to the UL element for scrolling

  // Filter items based on the current input value
  const filteredItems = $derived(
    items.filter(item =>
      value ? item.toLowerCase().includes(value.toLowerCase()) : true // Show all if input is empty
    )
  );

  function showDropdown() {
    if (filteredItems.length > 0) {
      dropdownVisible = true;
    }
  }

  function hideDropdown() {
    dropdownVisible = false;
    activeIndex = -1; // Reset active index when dropdown is hidden
  }

  function handleFocus() {
    // Show dropdown if there are items to display
    if (filteredItems.length > 0) {
      dropdownVisible = true;
    }
  }

  function handleInput() {
    // `value` (the prop signal) is updated by bind:value on the input.
    // This function is called after `value` has been updated.
    if (value && filteredItems.length > 0) {
      dropdownVisible = true;
    } else if (!value && items.length > 0) { // Input is empty, but items exist
      dropdownVisible = true; // Show all items (filteredItems will contain all)
    } else {
      dropdownVisible = false; // No input text or no matching items
    }
    activeIndex = -1; // Reset active index whenever the user types
  }

  function selectItem(item: string) {
    value = item; // Update the bound value signal
    hideDropdown();
    // Optionally, dispatch a custom event if parent needs to know about selection beyond value change
    // dispatch(new CustomEvent('select', { detail: item }));
  }

  function scrollToActiveItem() {
    if (listElement && activeIndex >= 0) {
      const activeLi = listElement.children[activeIndex] as HTMLLIElement;
      if (activeLi) {
        activeLi.scrollIntoView({ block: 'nearest' });
      }
    }
  }

  function handleKeydown(event: KeyboardEvent) {
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
       if (!dropdownVisible && filteredItems.length > 0) {
        showDropdown(); // Open dropdown if closed and navigating
        if (!dropdownVisible) return; // If still not visible (e.g. no items), exit
      }
    }

    if (!dropdownVisible || filteredItems.length === 0) return;

    switch (event.key) {
      case 'ArrowDown':
        event.preventDefault();
        activeIndex = (activeIndex + 1) % filteredItems.length;
        scrollToActiveItem();
        break;
      case 'ArrowUp':
        event.preventDefault();
        activeIndex = (activeIndex - 1 + filteredItems.length) % filteredItems.length;
        scrollToActiveItem();
        break;
      case 'Enter':
        event.preventDefault();
        if (activeIndex >= 0 && activeIndex < filteredItems.length) {
          selectItem(filteredItems[activeIndex]);
        }
        break;
      case 'Escape':
        event.preventDefault();
        hideDropdown();
        break;
      case 'Tab':
        // Allow tab to function normally, potentially closing dropdown if focus moves out
        hideDropdown();
        break;
    }
  }

  // Optional: If you need to expose methods to parent component
  // let inputElement = $state<HTMLInputElement | null>(null);
  // $expose({ focus: () => inputElement?.focus() });

</script>

<div class="relative" use:clickOutsideAction={hideDropdown}>
  {#if label}
    <label for={_id} class="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">{label}</label>
  {/if}
  <input
    type="text"
    {name}
    id={_id}
    bind:value placeholder={placeholder}
    class={inputClass}
    onfocus={handleFocus}
    oninput={handleInput}
    onkeydown={handleKeydown}
    aria-autocomplete="list"
    aria-expanded={dropdownVisible}
    aria-controls={dropdownVisible ? `${_id}-listbox` : undefined}
    aria-activedescendant={activeIndex >= 0 ? `${_id}-item-${activeIndex}` : undefined}
    autocomplete="off"
    spellcheck="false"
  />

  {#if dropdownVisible && filteredItems.length > 0}
    <ul
      bind:this={listElement}
      id={`${_id}-listbox`}
      class={dropdownClass}
      role="listbox"
      aria-labelledby={label ? _id : undefined} >
      {#each filteredItems as item, index (item)} <li
          id={`${_id}-item-${index}`}
          class="{itemClass} {index === activeIndex ? activeItemClass : ''}"
          role="option"
          aria-selected={index === activeIndex}
          onmousedown={(event) => { event.preventDefault(); selectItem(item); }} onmouseenter={() => activeIndex = index} >
          {item}
        </li>
      {/each}
    </ul>
  {/if}
</div>