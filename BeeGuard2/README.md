# BeeGuard2 — execução

Esta pasta já contém o modelo treinado dentro de `ia/classifier_model.h`. **Não precisa instalar Python científico, copiar dataset, treinar nem exportar modelo.** No computador, Python roda apenas a API e serve a dashboard. A ESP32 executa extração das features e classificação local.

## 1. Iniciar servidor e dashboard no PC

Instale Python 3.10 ou superior. Na pasta `BeeGuard2`, execute:

```powershell
python server\main.py --host 0.0.0.0 --port 8000
```

No navegador do PC, abra <http://localhost:8000>. A API usa somente bibliotecas incluídas no Python; não há `pip install` necessário para executar o servidor. O histórico mantém até 200 classificações na memória e é apagado ao fechar o servidor. A API recebe apenas resultados, não áudio.

## 2. Preparar a ESP32 uma vez

No Arduino IDE, instale o pacote de placas ESP32 da Espressif e a biblioteca `arduinoFFT` compatível com `ArduinoFFT<double>`. Crie a pasta `Documents\Arduino\BeeGuardESP32` e copie para ela estes três arquivos do repositório:

```text
ia/BeeGuardESP32.ino
ia/BeeGuardFeatures.h
ia/classifier_model.h
```

Abra `BeeGuardESP32.ino` nessa pasta. No sketch, configure:

- `WIFI_SSID` e `WIFI_PASSWORD` com sua rede;
- `API_URL` como `http://IP_DO_PC:8000/api/classification` (por exemplo, `http://192.168.1.20:8000/api/classification`).

Não use `localhost` no `API_URL`: para a placa isso apontaria para a própria ESP32. O PC e a placa precisam estar na mesma rede; permita a porta TCP 8000 no firewall local se necessário. Depois, selecione a placa/porta e grave o sketch. Abra o Monitor Serial em 115200 baud.

Ligação do INMP441:

| Microfone | ESP32 |
| --- | --- |
| SCK / BCLK | GPIO 26 |
| WS / LRC | GPIO 25 |
| SD / DOUT | GPIO 33 |
| VDD | 3V3 |
| GND | GND |
| L/R | GND |

A placa analisa janelas consecutivas de 10 s e envia classe, confiança, frequência dominante, dB relativo, RMS e energias de banda. Não grava WAV nem transmite o áudio.

## 3. Arquivos necessários

- `ia/BeeGuardESP32.ino`: captura I2S, classifica na ESP32 e envia o resultado.
- `ia/BeeGuardFeatures.h`: extrai as 40 características do sinal em blocos.
- `ia/classifier_model.h`: parâmetros C++ da árvore já treinada.
- `server/main.py`: API e servidor de arquivos da dashboard.
- `dashboard/`: página, estilos e atualização automática.

O código usa a API I2S legada (`driver/i2s.h`) e foi escrito para ESP32 clássico. Outros modelos, como ESP32-S3, podem exigir adaptação do driver. O servidor usa HTTP sem autenticação para protótipo em rede local; não o exponha diretamente à internet. Se editar SSID/senha dentro do sketch, mantenha esses dados fora de commits públicos.

## Modelo e métricas

O firmware incluído roda a Decision Tree de 11 nós. A validação agrupada por gravação reportou 84,4% de acurácia, 86,3% de acurácia balanceada e F1 macro 0,871. O dataset tinha somente quatro gravações `ALERT`; o número acima de 90% encontrado em artefatos antigos pertence a outro modelo e não é o firmware incluído aqui. A confiança da árvore não é calibrada. O firmware deve ser compilado e testado na combinação específica de placa e bibliotecas antes de uso.
