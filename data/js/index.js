// WebSocket для реального времени
const socket = new WebSocket('ws://' + window.location.hostname + ':81/');
// const socket = new WebSocket('ws://irhub.local:81/');

// Обработчик WebSocket
socket.onmessage = function (event) {
    const data = JSON.parse(event.data);
    updateUI(data);
    addToHistory(data);
};

// Обновление UI
function updateUI(data) {
    document.getElementById('ir-protocol').textContent = data.protocol;
    document.getElementById('ir-code').textContent = data.code;
    document.getElementById('ir-raw').value = data.raw;
}

function copyToClipboard(text) {
    var textArea = document.createElement("textarea");
    textArea.style.position = 'fixed';
    textArea.style.top = 0;
    textArea.style.left = 0;
    textArea.style.width = '2em';
    textArea.style.height = '2em';
    textArea.style.padding = 0;
    textArea.style.border = 'none';
    textArea.style.outline = 'none';
    textArea.style.boxShadow = 'none';
    textArea.style.background = 'transparent';
    textArea.value = text;

    document.body.appendChild(textArea);
    textArea.focus();
    textArea.select();

    try {
        var successful = document.execCommand('copy');
        var msg = successful ? 'successful' : 'unsuccessful';
        console.log('Copying text command was ' + msg);
    } catch (err) {
        console.log('Oops, unable to copy');
    }

    document.body.removeChild(textArea);
}

// Сбросить и приготовиться
function resetToReady() {
    fetch('/reset').then(data => { console.log(data); });
}

// Скопировать последний RAW
function copyLastReceivedRaw() {
    const raw = document.getElementById('ir-raw').value;
    copyToClipboard(raw);
}

// Инициализация
window.onload = function () {
    // Загружаем текущее состояние при загрузке страницы
    fetch('/api/v1/last-received-data')
        .then(response => response.json())
        .then(data => {
            updateUI(data);
            renderHistory();
        })
        .catch(error => console.error('Error:', error));
};
