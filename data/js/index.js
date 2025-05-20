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

        // // Создаем и показываем toast-уведомление
        // const toast = document.createElement('div');
        // toast.className = 'position-fixed bottom-0 end-0 p-3';
        // toast.innerHTML = `
        //   <div class="toast show" role="alert" aria-live="assertive" aria-atomic="true">
        //     <div class="toast-header">
        //       <strong class="me-auto">Уведомление</strong>
        //       <button type="button" class="btn-close" data-bs-dismiss="toast" aria-label="Close"></button>
        //     </div>
        //     <div class="toast-body">
        //       Скопировано в буфер обмена!
        //     </div>
        //   </div>
        // `;
        // document.body.appendChild(toast);

        // // Удаляем toast через 3 секунды
        // setTimeout(() => {
        //     toast.remove();
        // }, 3000);
    } catch (err) {
        console.log('Oops, unable to copy');
    }

    document.body.removeChild(textArea);
}



// Удаление элемента из истории
function deleteHistoryItem(index) {
    const modal = new bootstrap.Modal(document.getElementById('confirmDeleteModal'));
    modal.show();

    document.getElementById('confirmDeleteBtn').onclick = function () {
        let history = JSON.parse(localStorage.getItem('irHistory')) || [];
        history.splice(index, 1);
        localStorage.setItem('irHistory', JSON.stringify(history));
        renderHistory();
        modal.hide();
    };
}

// Добавление записи в историю
function addToHistory(data) {
    if (data.raw.toLowerCase().includes("ожидание")) return;
    const historyItem = {
        timestamp: new Date().toLocaleString(),
        protocol: data.protocol,
        code: data.code,
        raw: data.raw,
        label: ''
    };

    // Получаем текущую историю из localStorage
    let history = JSON.parse(localStorage.getItem('irHistory')) || [];

    // Добавляем новую запись в начало массива
    history.unshift(historyItem);

    // Ограничиваем историю (например, последние 50 записей)
    if (history.length > 50) {
        history = history.slice(0, 50);
    }

    // Сохраняем обновленную историю
    localStorage.setItem('irHistory', JSON.stringify(history));

    // Обновляем отображение истории
    renderHistory();
}

// Обновление метки в истории
function updateLabel(index, label) {
    let history = JSON.parse(localStorage.getItem('irHistory')) || [];
    if (history[index]) {
        history[index].label = label;
        localStorage.setItem('irHistory', JSON.stringify(history));
    }
}

// Отображение истории
function renderHistory() {
    const history = JSON.parse(localStorage.getItem('irHistory')) || [];
    const historyBody = document.getElementById('history-body');

    historyBody.innerHTML = history.map((item, index) => {
        return `
          <tr>
            <td class="align-middle" data-label="Дата/Время">${item.timestamp}</td>
            <td class="align-middle" data-label="Протокол">${item.protocol}</td>
            <td class="align-middle" data-label="Hexcode">${item.code}</td>
            <td class="align-middle" data-label="Метка">
              <input type="text" class="label-input form-control" value="${item.label || ''}" 
                     onchange="updateLabel(${index}, this.value)" 
                     placeholder="Введите метку">
            </td>
            <td class="align-middle" data-label="Действия">
              <div class="row g-2">
                <button class="col col-12 col-md-6 btn btn-sm btn-outline-primary" onclick="copyToClipboard('${item.raw.replace(/'/g, "\\'")}')">
                    <div class="d-flex align-content-center justify-content-center"><span class="material-icons me-1 ph-2">content_copy</span>RAW</div>
                </button>
                <button class="col col-12 col-md-6 btn btn-sm btn-outline-danger" onclick="deleteHistoryItem(${index})">
                    <span class="material-icons">delete</span>
                </button>
              </div>
            </td>
          </tr>
        `;
    }).join('');
}

// Очистка истории
document.getElementById('clear-history').addEventListener('click', () => {
    const modal = new bootstrap.Modal(document.getElementById('confirmClearModal'));
    modal.show();

    document.getElementById('confirmClearBtn').onclick = function () {
        localStorage.removeItem('irHistory');
        renderHistory();
        modal.hide();
    };
});

// Сбросить и приготовиться
document.getElementById('reset_to_ready').addEventListener('click', () => {
    fetch('/reset').then(data => { console.log(data); });
})

// Скопировать последний RAW
document.getElementById('copy-last-raw').addEventListener('click', () => {
    const raw = document.getElementById('ir-raw').value;
    copyToClipboard(raw);
})

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
