const italianMessages = {
  "Ready. Select your board’s USB port.": "Pronto. Seleziona la porta USB della tua scheda.",
  "Confirm that your data will be erased to continue.": "Conferma la cancellazione dei dati per continuare.",
  "Open this site over HTTPS or on localhost to use the USB port.": "Apri il sito tramite HTTPS oppure localhost per usare la porta USB.",
  "USB installation is unavailable: use Chrome or Edge on a computer.": "Installazione USB non disponibile: usa Chrome o Edge su computer.",
  "Loading is taking longer than expected. Check your connection and reload the page if needed.": "Caricamento lento. Controlla la connessione e ricarica la pagina se necessario.",
  "Unable to load the installer. Check your connection and reload the page.": "Impossibile caricare il programma di installazione. Controlla la connessione e ricarica la pagina."
};
const translate = (message) => document.documentElement.lang === "it" ? (italianMessages[message] || message) : message;
const consent = document.querySelector('#erase-consent');
const install = document.querySelector('#install');
const status = document.querySelector('#status');
let ready = false;
function refresh() {
  install.disabled = !ready || !consent.checked;
  if (ready) status.textContent = consent.checked
    ? translate("Ready. Select your board’s USB port.")
    : translate("Confirm that your data will be erased to continue.");
}
consent.addEventListener('change', refresh);
if (!window.isSecureContext) {
  status.textContent = translate("Open this site over HTTPS or on localhost to use the USB port.");
} else if (!('serial' in navigator)) {
  status.textContent = translate("USB installation is unavailable: use Chrome or Edge on a computer.");
} else {
  const timeout = setTimeout(() => {
    status.textContent = translate("Loading is taking longer than expected. Check your connection and reload the page if needed.");
  }, 15000);
  try {
    await import('https://unpkg.com/esp-web-tools@10.4.0/dist/web/install-button.js?module');
    await customElements.whenDefined('esp-web-install-button');
    clearTimeout(timeout);
    ready = true;
    refresh();
  } catch (error) {
    clearTimeout(timeout);
    status.textContent = translate("Unable to load the installer. Check your connection and reload the page.");
    console.error('ESP Web Tools could not be loaded', error);
  }
}
