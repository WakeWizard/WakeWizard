const consent = document.querySelector('#erase-consent');
const install = document.querySelector('#install');
const status = document.querySelector('#status');
let ready = false;
function refresh() {
  install.disabled = !ready || !consent.checked;
  if (ready) status.textContent = consent.checked
    ? 'Pronto. Seleziona la porta USB della tua scheda.'
    : 'Conferma la cancellazione dei dati per continuare.';
}
consent.addEventListener('change', refresh);
if (!window.isSecureContext) {
  status.textContent = 'Apri il sito tramite HTTPS oppure localhost per usare la porta USB.';
} else if (!('serial' in navigator)) {
  status.textContent = 'Installazione USB non disponibile: usa Chrome o Edge su computer.';
} else {
  const timeout = setTimeout(() => {
    status.textContent = 'Caricamento lento. Controlla la connessione e ricarica la pagina se necessario.';
  }, 15000);
  try {
    await import('https://unpkg.com/esp-web-tools@10.4.0/dist/web/install-button.js?module');
    await customElements.whenDefined('esp-web-install-button');
    clearTimeout(timeout);
    ready = true;
    refresh();
  } catch (error) {
    clearTimeout(timeout);
    status.textContent = 'Impossibile caricare il programma di installazione. Controlla la connessione e ricarica la pagina.';
    console.error('ESP Web Tools could not be loaded', error);
  }
}
