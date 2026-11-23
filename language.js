document.addEventListener('DOMContentLoaded', () => {
    const switcher = document.querySelector('.language-switch');
    if (!switcher) return;

    const syncHash = () => {
        const isEnglishGallery = location.hash === '#screenshots' && switcher.hreflang === 'zh-CN';
        switcher.hash = isEnglishGallery ? '#download' : location.hash;
    };
    syncHash();
    window.addEventListener('hashchange', syncHash);
});
