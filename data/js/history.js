// history.js - Функции для работы с историей ИК-сигналов

// Получить историю из localStorage
function getHistory() {
    return JSON.parse(localStorage.getItem('irHistory')) || [];
}

// Сохранить историю в localStorage
function saveHistory(history) {
    localStorage.setItem('irHistory', JSON.stringify(history));
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

    let history = getHistory();
    history.unshift(historyItem);

    // Ограничиваем историю последними 500 записями
    if (history.length > 500) {
        history = history.slice(0, 500);
    }

    saveHistory(history);
    renderHistory();
}

// Обновление метки в истории
function updateLabel(index, label) {
    let history = getHistory();
    if (history[index]) {
        history[index].label = label;
        saveHistory(history);
    }
}

// Создание HTML для одной записи истории
function createHistoryRow(item, index) {
    const row = document.createElement('tr');

    row.innerHTML = `
        <td class="align-middle" data-label="Дата/Время">${item.timestamp}</td>
        <td class="align-middle" data-label="Протокол">${item.protocol}</td>
        <td class="align-middle" data-label="Hexcode">${item.code}</td>
        <td class="align-middle" data-label="Метка">
            <input type="text" class="label-input form-control" value="${item.label || ''}" 
                   placeholder="Введите метку">
        </td>
        <td class="align-middle" data-label="Действия">
            <div class="btn-group" role="group">
                <button class="btn btn-sm btn-outline-primary copy-raw-btn ">
                    <i class="material-icons pt-1">content_copy</i>RAW
                </button>
                <button class="btn btn-sm btn-outline-danger delete-btn">
                    <i class="material-icons pt-1">delete</i>
                </button>
            </div>
        </td>
    `;

    // Настройка обработчиков
    row.querySelector('.label-input').addEventListener('change', (e) => {
        updateLabel(index, e.target.value);
    });

    row.querySelector('.copy-raw-btn').addEventListener('click', () => {
        copyToClipboard(item.raw);
    });

    row.querySelector('.delete-btn').addEventListener('click', () => {
        deleteHistoryItem(index);
    });

    return row;
}

// Отображение истории (упрощенная версия)
function renderHistory() {
    const history = getHistory();
    const historyBody = document.getElementById('history-body');
    historyBody.innerHTML = '';

    history.forEach((item, index) => {
        historyBody.appendChild(createHistoryRow(item, index));
    });
}

// Удаление элемента из истории
function deleteHistoryItem(index) {
    const modal = new bootstrap.Modal(document.getElementById('confirmDeleteModal'));

    document.getElementById('confirmDeleteBtn').onclick = function () {
        let history = getHistory();
        history.splice(index, 1);
        saveHistory(history);
        renderHistory();
        modal.hide();
    };

    modal.show();
}

// Экспорт истории в CSV
function exportHistoryToCSV() {
    const history = getHistory();
    if (history.length === 0) {
        alert('История пуста!');
        return;
    }

    const headers = 'Дата/Время,Протокол,Hexcode,Метка,RAW данные\n';
    const csvContent = history.reduce((csv, item) => {
        return csv + `"${item.timestamp}","${item.protocol}","${item.code}","${item.label}","${item.raw}"\n`;
    }, headers);

    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    const datetime = new Date().toISOString()
        .slice(0, 20)
        .replaceAll('-', '')
        .replaceAll(":", '')
        .replaceAll('T', '_');

    link.href = url;
    link.setAttribute('download', `ir_history_${datetime}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}

// Очистка всей истории
function clearHistory() {
    const modal = new bootstrap.Modal(document.getElementById('confirmClearModal'));

    document.getElementById('confirmClearBtn').onclick = function () {
        localStorage.removeItem('irHistory');
        renderHistory();
        modal.hide();
    };

    modal.show();
}