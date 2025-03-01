#include <pgmspace.h>

void handleConfig() {
  String ssidParam = server.arg("ssid");  // Получаем SSID из параметра URL
  String html = "<html><head>";
  html += getCSS();
  html += "<title>ИК-приемник</title>";
  html += "<script>";
  html += "function setSSID(ssid) {";
  html += "  document.getElementsByName('ssid')[0].value = ssid;";
  html += "  document.getElementsByName('mode')[0].value = '0';";
  html += "}";
  html += "function togglePassword() {";
  html += "  var passwordField = document.getElementsByName('password')[0];";
  html += "  if (document.getElementById('togglePassword').checked) {";
  html += "     passwordField.type = 'text';";
  html += "  } else {";
  html += "     passwordField.type = 'password';";
  html += "  }";
  html += "}";
  html += "</script>";
  html += "</head>";
  html += "<body><h1>Настройки</h1>";
  html += "<p>Версия прошивки: <strong>";
  html += FIRMWARE_VER;
  html += "</strong></p>";
  html += "<h3>WiFi</h3>";
  html += "<form action='/save' method='POST'>";
  html += "Режим работы WiFi: <select name='mode'>";
  html += "<option value='0'" + String(settings.isAPMode ? "" : " selected") + ">Клиент</option>";
  html += "<option value='1'" + String(settings.isAPMode ? " selected" : "") + ">Точка доступа</option>";
  html += "</select><br>";
  html += "SSID: <input type='text' name='ssid' value='" + String(settings.ssid) + "'><br>";
  html += "Пароль: <input type='password' name='password' value='" + String(settings.password) + "'><br>";
  html += "<label onclick='togglePassword();'><input type='checkbox' id='togglePassword'>Показать пароль</label>";
  html += "<br><br>";
  html += "<input type='submit' value='СОХРАНИТЬ'>";
  html += "</form>";
  html += "<hr>";
  html += "<p><a href='/scan'>Сканировать доступные сети</a></p>";
  html += "<p><a href='/'>назад</a></p>";
  html += "<br><br><br><br>";
  html += "<p><a href='/eraseWiFiCredentials' style='color: red;'>Сбросить настройки WiFi</a></p>";
  html += "</body></html>";

  if (ssidParam.length() > 0) {
    html += "<script>setSSID('" + ssidParam + "');</script>";  // Вызываем функцию для подстановки SSID
  }

  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.method() == HTTP_POST) {
    settings.isAPMode = server.arg("mode").toInt() == 1;
    strncpy(settings.ssid, server.arg("ssid").c_str(), sizeof(settings.ssid));
    strncpy(settings.password, server.arg("password").c_str(), sizeof(settings.password));
    saveSettings();

    String html = "<html><head>";
    html += getCSS();
    html += "<title>ИК-приемник</title>";
    html += "<meta http-equiv='refresh' content='10;URL=http://";
    html += HOSTNAME;
    html += ".local'>";
    html += "</head>";
    html += "<body><h1>Настройки сохранены, перезагрузка... (около 10 сек)</h1></body></html>";
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
  }
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String html = "<html><head>";
  html += getCSS();
  html += "<title>Сканирование доступных сетей</title>";
  html += "</head>";
  html += "<body><h1>Доступные сети</h1>";
  html += "<ul>";
  for (int i = 0; i < n; ++i) {
    html += "<li><a href='/config?ssid=" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</a></li>";
  }
  html += "</ul>";
  html += "<hr>";
  html += "<p><a href='/config'>назад</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleNotFound() {
  // Перенаправляем все неизвестные запросы на главную страницу
  IPAddress ip = WiFi.softAPIP();
  String ipString = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  String url = "http://" + ipString + "/";
  server.sendHeader("Location", url, true);
  server.send(302, "text/plain", "");
}

void handleEraseWifiCredentials() {
  String html = "<html><head>";
  html += getCSS();
  html += "<title>ИК-приемник</title>";
  html += "</head>";
  html += "<body><h1>Настройки сброшены, переходим в режим точки доступа ";
  html += SSID_DEFAULT;
  html += ", перезагрузка...</h1></body></html>";
  server.send(200, "text/html", html);

  Serial.println("Сброс параметров подключения WiFi");
  setDefaultSettings();
  saveSettings();
  Serial.println("Перезагрузка...");
  delay(2000);
  ESP.reset();
}


const char css[] PROGMEM = R"=====(
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; margin: 20px; }
h1 { color: #444; }
p { font-size: 16px;  word-break: break-all;}
form { margin-top: 20px; }
a { color: #06c; text-decoration: none; }
a:hover { text-decoration: underline; }
ul { list-style-type: none; padding: 0; }
li { margin: 10px 0; }
label { cursor: pointer; display: flex; align-items: center; margin: 10px 0; }
input[type='text'], input[type='password'], select { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; }
input[type='checkbox'] { width: 18px; height: 18px; border: 2px solid #007bff; border-radius: 4px; outline: none; cursor: pointer; margin-right: 10px; position: relative; }
input[type='checkbox']:checked { background-color: #007bff; border-color: #007bff; }
input[type='submit'], button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }
input[type='submit']:hover, button:hover { background-color: #45a049; }
input[type='submit'], button { width: 100%; }
@media (min-width: 768px) {
  input[type='submit'], button { width: auto; }
}
</style>
<script>

</script>
)=====";

String getCSS() {
  return FPSTR(css);
}