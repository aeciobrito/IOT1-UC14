#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ================= CONFIGURAÇÕES DE REDE =================
// Configure aqui o HOTSPOT do seu celular / roteador
const char* ssid = "SEU_WIFI";
const char* password = "SUA_SENHA";

// ================= CONFIGURAÇÕES DO SENSOR ================
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Variáveis para armazenar leitura
float temperaturaInterna = 0.0;
float umidadeInterna = 0.0;

// Cria o servidor na porta 80 (padrão web)
WebServer server(80);

// ================= O SITE (FRONTEND) ======================
// HTML + JS robusto (timeout, primeiro fetch imediato, interval)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Dashboard IoT ESP32</title>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
  <style>
    body { font-family: 'Segoe UI', sans-serif; background-color: #e0e5ec; margin: 0; padding: 20px; }
    .dashboard-container { display: flex; flex-wrap: wrap; gap: 20px; justify-content: center; max-width: 1000px; margin: 0 auto; }
    .card { background: white; padding: 20px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); width: 100%; max-width: 300px; text-align: center; display: flex; flex-direction: column; align-items: center; }
    .card-icone { font-size: 80px; color: #2980b9; margin-bottom: 15px; }
    h3 { margin-top: 0; color: #555; margin-bottom: 5px; }
    .valor-destaque { font-size: 2.5em; font-weight: bold; color: #2980b9; margin: 10px 0; }
    .status-box { font-weight: bold; padding: 10px; border-radius: 5px; background-color: #eee; margin-top: 15px; width: 100%; transition: 0.3s; }
    .footer { text-align: center; margin-top: 20px; color: #888; font-size: 0.9em; }
  </style>
</head>
<body>
  <h1 style="text-align: center; color: #333;">Monitoramento Inteligente</h1>
  
  <div class="dashboard-container">
    <div class="card">
      <i class="fa-solid fa-house-chimney card-icone"></i>
      <h3>Ambiente Interno</h3>
      <div class="valor-destaque" id="tempInternaDisplay">-- ºC</div>
      <div id="umidInternaDisplay" style="color:#666">Umidade: --%</div>
    </div>

    <div class="card">
      <i class="fa-solid fa-cloud-sun card-icone"></i>
      <h3>Temperatura Externa</h3>
      <div class="valor-destaque" id="tempExternaDisplay">-- ºC</div>
    </div>

    <div class="card">
      <i class="fa-solid fa-wind card-icone"></i>
      <h3>Velocidade Vento</h3>
      <div class="valor-destaque" id="ventDisplay">-- km/h</div>
    </div>

    <div class="card">
      <i class="fa-solid fa-microchip card-icone"></i>
      <h3>Decisão do Sistema</h3>
      <div id="acao" class="status-box">Inicializando...</div>
    </div>
  </div>

  <div class="footer">Atualização automática a cada 5 segundos.</div>

  <script>
    // URLs
    const URL_METEO = "https://api.open-meteo.com/v1/forecast?latitude=-23.526&longitude=-46.690&current_weather=true";
    const URL_LOCAL = "/lerSensor"; // rota relativa (mesmo host / ESP)

    // Fetch com timeout usando AbortController
    async function fetchWithTimeout(url, ms = 5000, opts = {}) {
      const controller = new AbortController();
      const id = setTimeout(() => controller.abort(), ms);
      try {
        const resp = await fetch(url, {...opts, signal: controller.signal});
        clearTimeout(id);
        return resp;
      } catch (err) {
        clearTimeout(id);
        throw err;
      }
    }

    async function atualizarDashboard() {
      const statusBox = document.getElementById("acao");
      try {
        statusBox.innerText = "Atualizando dados...";
        statusBox.style.backgroundColor = "#eee";
        statusBox.style.color = "#333";

        // 1) Buscar interno primeiro (rápido)
        let tempInterna = NaN, umidInterna = NaN;
        try {
          const rLocal = await fetchWithTimeout(URL_LOCAL, 3000, {cache: "no-store"});
          if (rLocal.ok) {
            const jLocal = await rLocal.json();
            tempInterna = Number(jLocal.temperatura);
            umidInterna = Number(jLocal.umidade);
            console.log("[LOCAL] ok:", tempInterna, umidInterna);
          } else {
            console.warn("[LOCAL] status", rLocal.status);
          }
        } catch (errLocal) {
          console.warn("[LOCAL] erro:", errLocal);
        }

        // Atualiza UI interna se disponível
        if (!isNaN(tempInterna)) {
          document.getElementById("tempInternaDisplay").innerText = tempInterna.toFixed(1) + " ºC";
          document.getElementById("umidInternaDisplay").innerText = "Umidade: " + (isNaN(umidInterna) ? "--" : umidInterna.toFixed(0)) + "%";
        }

        // 2) Buscar externo (API) com timeout
        let tempExterna = NaN, vento = NaN;
        try {
          const rMeteo = await fetchWithTimeout(URL_METEO, 5000, {cache: "no-store"});
          if (rMeteo.ok) {
            const jMeteo = await rMeteo.json();
            tempExterna = jMeteo.current_weather.temperature;
            vento = jMeteo.current_weather.windspeed;
            console.log("[METEO] ok:", tempExterna, vento);
          } else {
            console.warn("[METEO] status", rMeteo.status);
          }
        } catch (errM) {
          console.warn("[METEO] erro:", errM);
        }

        if (!isNaN(tempExterna)) {
          document.getElementById("tempExternaDisplay").innerText = tempExterna + " ºC";
          document.getElementById("ventDisplay").innerText = vento + " km/h";
        }

        // 3) Decisão
        if (!isNaN(tempInterna) && !isNaN(tempExterna)) {
          if (tempExterna < (tempInterna - 2)) {
            statusBox.innerText = "DESLIGAR AC (Abra as janelas)";
            statusBox.style.backgroundColor = "#27ae60";
            statusBox.style.color = "white";
          } else {
            statusBox.innerText = "MANTER AC LIGADO (Está quente fora)";
            statusBox.style.backgroundColor = "#e74c3c";
            statusBox.style.color = "white";
          }
        } else if (!isNaN(tempInterna)) {
          statusBox.innerText = "Sem dados externos — mostrando interno";
          statusBox.style.backgroundColor = "#f39c12";
          statusBox.style.color = "#111";
        } else {
          statusBox.innerText = "Dados indisponíveis";
          statusBox.style.backgroundColor = "orange";
          statusBox.style.color = "#111";
        }

        console.log("Atualização concluída: " + new Date().toLocaleTimeString());
      } catch (fatal) {
        console.error("Erro fatal em atualizarDashboard:", fatal);
        document.getElementById("acao").innerText = "Erro inesperado";
        document.getElementById("acao").style.backgroundColor = "orange";
      }
    }

    // Garante execução da primeira leitura e registro do interval sem risco de bloquear
    window.addEventListener('load', () => {
      atualizarDashboard().catch(e => console.error("Erro na primeira leitura:", e));
      setInterval(() => {
        atualizarDashboard().catch(e => console.error("Erro no agendamento:", e));
      }, 5000); // 5 segundos
    });
  </script>
</body>
</html>
)rawliteral";

// ================= FUNÇÕES DO SERVIDOR ====================

// Responde a opções CORS (preflight) para /lerSensor
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204, "text/plain", "");
}

// Rota Raiz (/): Entrega o site HTML (com headers e log)
void handleRoot() {
  Serial.println("[HTTP] Requisição / (root) recebida");
  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Access-Control-Allow-Origin", "*"); // para facilitar testes; remova se não necessário
  server.send(200, "text/html", index_html);
}

// Rota API (/lerSensor): Entrega os dados em JSON (sem cache, com headers e log)
void handleSensorData() {
  Serial.println("[HTTP] Requisição /lerSensor recebida");
  String json = "{\"temperatura\": " + String(temperaturaInterna, 2) + ", \"umidade\": " + String(umidadeInterna, 2) + "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "application/json", json);
}

// 404 handler (log)
void handleNotFound() {
  Serial.print("[HTTP] NotFound: ");
  Serial.println(server.uri());
  server.send(404, "text/plain", "Not found");
}

// ================= SETUP E LOOP ===========================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== ESP32 DHT22 WebServer ===");

  dht.begin();

  // Conexão Wi-Fi
  Serial.print("Conectando em ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  const unsigned long maxWait = 20000; // 20s para conectar
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < maxWait) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi conectado!");
    Serial.print("Endereço IP para acessar no navegador: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Falha ao conectar ao WiFi dentro do tempo. Continuando em modo STA (pode tentar reconectar).");
    // opcional: entrar em AP mode se desejar
    // WiFi.mode(WIFI_AP);
    // WiFi.softAP("ESP32_AP", "12345678");
    // Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  }

  // Configura as rotas do servidor
  server.on("/", HTTP_GET, handleRoot);
  server.on("/lerSensor", HTTP_GET, handleSensorData);
  server.on("/lerSensor", HTTP_OPTIONS, handleOptions); // preflight CORS
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor Web iniciado!");
}

void loop() {
  // Mantém o servidor ouvindo os clientes
  server.handleClient();

  // Leitura do Sensor (não bloqueante)
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 2000) { // leitura a cada 2s
    lastTime = millis();
    
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    // Só atualiza se a leitura for válida
    if (!isnan(t) && !isnan(h)) {
      temperaturaInterna = t;
      umidadeInterna = h;
      Serial.print("Leitura DHT -> Temp: ");
      Serial.print(temperaturaInterna);
      Serial.print(" C | Umid: ");
      Serial.print(umidadeInterna);
      Serial.println(" %");
    } else {
      Serial.println("Leitura DHT inválida");
    }
  }
}
