// Only the bare site URL restores a saved language; explicit page links always win.
const language = document.documentElement.lang;
try {
  if (location.pathname.endsWith('/') && localStorage.getItem('wakewizard-language') === 'it') {
    location.replace('it.html' + location.search + location.hash);
  } else {
    localStorage.setItem('wakewizard-language', language);
  }
} catch {
  // Language links also work when browser storage is disabled.
}
