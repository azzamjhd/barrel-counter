<script lang="ts">
  // Define types for the props for better type safety
  type Props = {
    btnLabel?: string;
    title?: string;
    message?: string;
    confirmLabel?: string;
    cancelLabel?: string;
    onConfirmAction: () => void; // The function to run on confirmation
    btnClass?: string; // Optional class for the trigger button
    // modalBackdropClass is not directly used with <dialog> in the same way,
    // but you can style the ::backdrop pseudo-element if needed.
    modalContentClass?: string; // Optional class for the modal's <article>
    cancelBtnClass?: string; // Optional class for the cancel button
    confirmBtnClass?: string; // Optional class for the confirm button
  };

  // Use $props() with the defined type
  const {
    btnLabel = "Click",
    title = "Confirm",
    message = "Are you sure?",
    confirmLabel = "OK",
    cancelLabel = "Cancel",
    onConfirmAction, // Destructure the function prop
    btnClass = "px-3 py-1 bg-blue-600 text-white rounded",
    // modalBackdropClass = "fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50", // Style ::backdrop instead
    modalContentClass = "bg-gray-800 rounded-lg shadow-lg p-6 w-full max-w-sm", // Applied to article
    cancelBtnClass = "secondary", // PicoCSS default or your custom
    confirmBtnClass = "", // PicoCSS default or your custom
  } = $props();

  let visible = $state(false);
  let dialogElement: HTMLDialogElement | undefined = $state();

  function openDialog(): void {
    visible = true;
    dialogElement?.showModal(); // Use showModal for proper modal behavior
  }

  function handleConfirm(): void {
    if (typeof onConfirmAction === "function") {
      onConfirmAction(); // Execute the passed function
    }
    closeDialog();
  }

  function handleCancel(): void {
    closeDialog();
  }

  function closeDialog(): void {
    visible = false;
    dialogElement?.close();
  }

  function handleClickOverlay(event: MouseEvent): void {
    if (event.target === dialogElement) {
      handleCancel();
    }
  }

  $effect(() => {
    if (dialogElement) {
      if (visible) {
        dialogElement.showModal();
      } else {
        dialogElement.close();
      }
    }
  });

</script>

<input type="submit" class={btnClass} onclick={openDialog} value={btnLabel} />

<dialog bind:this={dialogElement} onclick={handleClickOverlay}>
  <article class={modalContentClass}>
    <header>
      <button aria-label="Close" rel="prev" onclick={handleCancel}></button>
      <h3>{title}</h3>
    </header>
    <p>{message}</p>
    <footer>
      <button class={cancelBtnClass} onclick={handleCancel}>{cancelLabel}</button>
      <button class={confirmBtnClass} onclick={handleConfirm}>{confirmLabel}</button>
    </footer>
  </article>
</dialog>

<style>
  dialog::backdrop {
    background-color: rgba(0, 0, 0, 0.5);
  }
</style>
