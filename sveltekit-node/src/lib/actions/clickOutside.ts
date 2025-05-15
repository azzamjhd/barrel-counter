/**
 * Action to detect clicks outside a given node.
 * @param {HTMLElement} node - The node to monitor.
 * @param {() => void} callback - Function to call when a click outside occurs.
 */
export function clickOutside(node: HTMLElement, callback: () => void) {
  const handleClick = (event: MouseEvent) => {
    if (node && !node.contains(event.target as Node) && !event.defaultPrevented) {
      callback();
    }
  };

  document.addEventListener('click', handleClick, true);

  return {
    destroy() {
      document.removeEventListener('click', handleClick, true);
    }
  };
}