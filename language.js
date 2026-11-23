document.addEventListener('DOMContentLoaded', () => {
    const switcher = document.querySelector('.language-switch');
    if (!switcher) return;

    const syncHash = () => {
        switcher.hash = location.hash;
    };
    syncHash();
    window.addEventListener('hashchange', syncHash);
});
