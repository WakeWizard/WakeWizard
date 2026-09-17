const consent = document.querySelector('#erase-consent');
const install = document.querySelector('#install');
const status = document.querySelector('#status');
let ready = false;
function refresh() {
  install.disabled = !ready || !consent.checked;
  if (ready) status.textContent = consent.checked
    ? 'Ready. Select your board’s USB port.'
    : 'Confirm that your data will be erased to continue.';
}
consent.addEventListener('change', refresh);
if (!window.isSecureContext) {
  status.textContent = 'Open this site over HTTPS or on localhost to use the USB port.';
} else if (!('serial' in navigator)) {
  status.textContent = 'USB installation is unavailable: use Chrome or Edge on a computer.';
} else {
  const timeout = setTimeout(() => {
    status.textContent = 'Loading is taking longer than expected. Check your connection and reload the page if needed.';
  }, 15000);
  try {
    await import('https://unpkg.com/esp-web-tools@10.4.0/dist/web/install-button.js?module');
    await customElements.whenDefined('esp-web-install-button');
    clearTimeout(timeout);
    ready = true;
    refresh();
  } catch (error) {
    clearTimeout(timeout);
    status.textContent = 'Unable to load the installer. Check your connection and reload the page.';
    console.error('ESP Web Tools could not be loaded', error);
  }
}
