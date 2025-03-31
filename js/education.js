const loadingSquares = document.getElementById('loadingSquares');
const startScreen = document.getElementById('startScreen');

setTimeout(() => {
  loadingSquares.classList.add('fade-out');

  setTimeout(() => {
    loadingSquares.classList.add('hidden');
    startScreen.classList.remove('hidden');
    startScreen.classList.add('fade-in');
  }, 500);
}, 2000);
