const loadingSquares = document.getElementById('loadingSquares');
const startScreen = document.getElementById('startScreen');
const projectList = document.getElementById('projectList');
const keySound = document.getElementById('key');
const enterSound = document.getElementById('enter');

let currentView = projectList;
let selectedIndex = 0;
let ready = false;               // ignore keys until the loading animation is done

// === Loading animation, then show the list ===
setTimeout(() => {
    loadingSquares.classList.add('fade-out');

    setTimeout(() => {
        loadingSquares.classList.add('hidden');
        startScreen.classList.remove('hidden');
        startScreen.classList.add('fade-in');
        ready = true;
        updateSelection();
    }, 500);
}, 2000);

// === Helpers ===
function playSound(sound) {
    sound.currentTime = 0;              // restart, so fast key presses all click
    sound.play().catch(() => {});       // browsers may block sound before a click
}

function itemsInView() {
    return Array.from(currentView.querySelectorAll('.menu-item'));
}

function updateSelection() {
    itemsInView().forEach((item, i) => {
        item.classList.toggle('selected', i === selectedIndex);
    });
}

function showView(view) {
    // Hide the current view and stop its video
    currentView.classList.add('hidden');
    const oldVideo = currentView.querySelector('video');
    if (oldVideo) oldVideo.pause();

    // Show the new view and start its video from the beginning
    view.classList.remove('hidden');
    const video = view.querySelector('video');
    if (video) {
        video.currentTime = 0;
        video.play().catch(() => {});
    }

    currentView = view;
    selectedIndex = 0;
    updateSelection();
    window.scrollTo(0, 0);
}

// === Clicks ===
// Open a project
document.querySelectorAll('[data-project]').forEach(item => {
    item.addEventListener('click', () => {
        playSound(enterSound);
        showView(document.getElementById('project-' + item.dataset.project));
    });
});

// Back to the project list
document.querySelectorAll('[data-close]').forEach(item => {
    item.addEventListener('click', () => {
        playSound(enterSound);
        showView(projectList);
    });
});

// Back to the home page
document.querySelectorAll('[data-back]').forEach(item => {
    item.addEventListener('click', () => {
        playSound(enterSound);
        setTimeout(() => {
            window.location.href = '../index.html';
        }, 200);
    });
});

document.querySelectorAll('a.menu-item').forEach(link => {
    link.addEventListener('click', () => playSound(enterSound));
});

// Hovering an item selects it, so mouse and keyboard highlight the same one
document.querySelectorAll('.menu-item').forEach(item => {
    item.addEventListener('mouseenter', () => {
        const index = itemsInView().indexOf(item);
        if (index !== -1) {
            selectedIndex = index;
            updateSelection();
        }
    });
});

// === Keyboard ===
document.addEventListener('keydown', (event) => {
    if (!ready) return;
    const items = itemsInView();

    if (event.code === 'ArrowUp') {
        event.preventDefault();
        selectedIndex = (selectedIndex - 1 + items.length) % items.length;
        updateSelection();
        playSound(keySound);
    }

    if (event.code === 'ArrowDown') {
        event.preventDefault();
        selectedIndex = (selectedIndex + 1) % items.length;
        updateSelection();
        playSound(keySound);
    }

    if (event.code === 'Enter') {
        items[selectedIndex].click();
    }

    if (event.code === 'Escape' && currentView !== projectList) {
        playSound(enterSound);
        showView(projectList);
    }
});