const baseUrl = window.location.origin + window.location.pathname.replace(/index\.html$/, '');
const menu = document.querySelector('.menu');
const startScreen = document.querySelectorAll('.start-screen');
const spaceSound = document.getElementById('start');
const keySound = document.getElementById('key');
const enterSound = document.getElementById('enter');
const items = document.querySelectorAll('.menu-item');

let hasStarted = false;
let selectedIndex = 0;

function updateSelection(index) {
    items.forEach((item, i) => {
        item.classList.toggle('selected', i === index);
    });
}

items.forEach((item) => {
    item.addEventListener('click', () => {
        const path = item.dataset.path;
        enterSound.play();
        setTimeout(() => {
            window.location.href = `${baseUrl}${path}`;
        }, 200);
    });
});


document.addEventListener('keydown', (event) => {
    if (!hasStarted && event.code === 'Space') {
        hasStarted = true;
        startScreen.forEach(screen => screen.classList.add('hidden'));
        menu.classList.remove('hidden');
        menu.classList.add('show');
        spaceSound.play();
        updateSelection(selectedIndex);
        return;
    }

    if (!hasStarted) return;

    if (event.code === 'ArrowUp') {
        selectedIndex = (selectedIndex - 1 + items.length) % items.length;
        updateSelection(selectedIndex);
        keySound.play();
    }

    if (event.code === 'ArrowDown') {
        selectedIndex = (selectedIndex + 1) % items.length;
        updateSelection(selectedIndex);
        keySound.play();
    }

    if (event.code === 'Enter') {
        const path = items[selectedIndex].dataset.path;
        enterSound.play();
        setTimeout(() => {
        window.location.href = `${baseUrl}${path}`;
        }, 200);
    }
});

document.addEventListener('click', () => {
    if (!hasStarted) {
        hasStarted = true;
        startScreen.forEach(screen => screen.classList.add('hidden'));
        menu.classList.remove('hidden');
        menu.classList.add('show');
        spaceSound.play();
        updateSelection(selectedIndex);
    }
});
