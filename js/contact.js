const loadingSquares = document.getElementById('loadingSquares');
const startScreen = document.getElementById('startScreen');
const introText = document.getElementById('introText');
const feedback = document.getElementById('feedback');
const sendButton = document.querySelector('.send-button');
const blipSound = document.getElementById('blipSound');
const sendSound = document.getElementById('sendSound');

const message = `
> Accessing contact interface...
> Loading modules...
> Connection established.
`;

let index = 0;
function typeIntro() {
    if (index < message.length) {
        introText.textContent += message.charAt(index);
        index++;
        blipSound.currentTime = 0;
        blipSound.play();
        setTimeout(typeIntro, 40);
    }
}

setTimeout(() => {
    loadingSquares.classList.add('fade-out');

    setTimeout(() => {
        loadingSquares.classList.add('hidden');
        startScreen.classList.remove('hidden');
        startScreen.classList.add('fade-in');
        typeIntro();
    }, 500);
}, 2000);

sendButton.addEventListener('click', () => {
    sendSound.play();
    feedback.classList.remove('hidden');
    feedback.textContent = '>>> transmitting...';
    setTimeout(() => {
        feedback.textContent = '>>> transmission sent successfully...';
    }, 1500);
});
