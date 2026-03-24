#include <Arduino.h>
#include "BluetoothA2DPSink.h"
#include <TFT_eSPI.h>

BluetoothA2DPSink a2dp_sink;
TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite spriteMusica = TFT_eSprite(&tft); 

// --- CONTROLE DO MUTE REDUNDANTE ---
#define PINO_MUTE 32
bool esperando_desmutar = false;
unsigned long tempo_play_pressionado = 0;
const int ATRASO_DESMUTE_MS = 500; 

// --- BANDEIRAS VISUAIS ---
volatile bool atualizar_tela_conexao = false;
volatile bool atualizar_tela_metadados = false;
volatile bool atualizar_tela_status = false;
volatile bool atualizar_tela_tempo = false;
volatile bool atualizar_tela_volume = true; 

// --- MEMÓRIA DO SISTEMA ---
bool bluetooth_conectado = false;
bool musica_tocando = false;
String nome_celular = "Nao conectado";
String musica_atual = "";
String artista_atual = "";
int volume_celular = 60; 

// --- VARIÁVEIS DE ANIMAÇÃO ---
int largura_musica = 0;
int posicao_scroll = 460;
unsigned long ultimo_scroll = 0;
bool precisa_scroll = false;

uint32_t tempo_atual_ms = 0;
uint32_t duracao_total_ms = 0;
unsigned long ultimo_tick_tempo = 0;

String formatarTempo(uint32_t ms) {
  uint32_t segundos = ms / 1000;
  uint32_t minutos = segundos / 60;
  segundos = segundos % 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", minutos, segundos);
  return String(buf);
}

void volume_change_callback(int volume) {
  volume_celular = volume;
  atualizar_tela_volume = true;
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  if (id == ESP_AVRC_MD_ATTR_TITLE) {
    musica_atual = (char*)text;
    tempo_atual_ms = 0; 
    ultimo_tick_tempo = millis(); 
    atualizar_tela_metadados = true;
    atualizar_tela_tempo = true;
  } 
  else if (id == ESP_AVRC_MD_ATTR_ARTIST) {
    artista_atual = (char*)text;
    atualizar_tela_metadados = true;
  }
  else if (id == ESP_AVRC_MD_ATTR_PLAYING_TIME) {
    duracao_total_ms = atoi((char*)text);
    atualizar_tela_tempo = true;
  }
}

void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  if (state == ESP_A2D_AUDIO_STATE_STARTED) {
    musica_tocando = true;
    atualizar_tela_status = true;
    esperando_desmutar = true;
    tempo_play_pressionado = millis(); 
  } 
  else if (state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND || state == ESP_A2D_AUDIO_STATE_STOPPED) {
    musica_tocando = false;
    atualizar_tela_status = true;
    digitalWrite(PINO_MUTE, LOW); 
    esperando_desmutar = false;
  }
}

void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    bluetooth_conectado = true;
  } else {
    bluetooth_conectado = false;
    digitalWrite(PINO_MUTE, LOW); 
  }
  atualizar_tela_conexao = true;
}

void avrc_play_pos_callback(uint32_t play_pos) {
  tempo_atual_ms = play_pos;
  ultimo_tick_tempo = millis(); 
  atualizar_tela_tempo = true;
}

void avrc_track_change_callback(uint8_t *id) {
  tempo_atual_ms = 0;
  ultimo_tick_tempo = millis(); 
  atualizar_tela_tempo = true;
}

void desenhar_logo_bluetooth(int bx, int by, uint16_t cor) {
  // 1. O Fundo Clássico: Uma elipse preenchida (raio X = 15, raio Y = 19)
  // Isso cria aquela proporção ovalada elegante atrás do símbolo
  tft.fillEllipse(bx, by, 15, 19, TFT_BLUE);              
  
  // 2. A Runa "B": Agora em branco sobre o fundo azul
  tft.drawLine(bx, by - 12, bx, by + 12, TFT_WHITE);              
  tft.drawLine(bx, by - 12, bx + 10, by - 6, TFT_WHITE);          
  tft.drawLine(bx + 10, by - 6, bx - 10, by + 6, TFT_WHITE);      
  tft.drawLine(bx - 10, by - 6, bx + 10, by + 6, TFT_WHITE);      
  tft.drawLine(bx + 10, by + 6, bx, by + 12, TFT_WHITE);          
}

void setup() {
  Serial.begin(115200);

  pinMode(PINO_MUTE, OUTPUT);
  digitalWrite(PINO_MUTE, LOW);
  
  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK); 
  
  // 1. SPRITE MAIOR PARA PROTEGER OS DESCENDENTES
  spriteMusica.createSprite(450, 70);

  tft.drawLine(0, 70, tft.width(), 70, TFT_DARKGREY); 
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4); 
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Nao conectado", tft.width() / 2, 35);

  i2s_pin_config_t my_pin_config = {
      .bck_io_num = 26,
      .ws_io_num = 25,
      .data_out_num = 22,
      .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  a2dp_sink.set_pin_config(my_pin_config);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed); 
  a2dp_sink.set_on_connection_state_changed(connection_state_changed); 
  a2dp_sink.set_avrc_rn_play_pos_callback(avrc_play_pos_callback); 
  a2dp_sink.set_avrc_rn_track_change_callback(avrc_track_change_callback); 
  a2dp_sink.set_on_volumechange(volume_change_callback);
  
  a2dp_sink.start("Cerato_Bluetooth"); 
}

void loop() {
  if (esperando_desmutar) {
    if (millis() - tempo_play_pressionado >= ATRASO_DESMUTE_MS) {
      digitalWrite(PINO_MUTE, HIGH);
      esperando_desmutar = false;
    }
  }

  if (musica_tocando) {
    if (millis() - ultimo_tick_tempo >= 1000) {
      ultimo_tick_tempo = millis();
      tempo_atual_ms += 1000;
      if (duracao_total_ms > 0 && tempo_atual_ms > duracao_total_ms) {
        tempo_atual_ms = duracao_total_ms; 
      }
      atualizar_tela_tempo = true;
    }
  }

  // --- DESENHO DA BARRA DE VOLUME ---
  if (atualizar_tela_volume) {
    atualizar_tela_volume = false;
    int x_barra = 455; 
    int y_barra = 150; 
    int largura_barra = 12;
    int altura_total = 150; 
    
    tft.drawRect(x_barra, y_barra, largura_barra, altura_total, TFT_DARKGREY);
    tft.fillRect(x_barra + 2, y_barra + 2, largura_barra - 4, altura_total - 4, TFT_BLACK);
    int altura_preenchida = map(volume_celular, 0, 127, 0, altura_total - 4);
    if (altura_preenchida > 0) {
      int y_inicio_preenchimento = (y_barra + altura_total - 2) - altura_preenchida;
      tft.fillRect(x_barra + 2, y_inicio_preenchimento, largura_barra - 4, altura_preenchida, TFT_WHITE);
    }
  }

  if (atualizar_tela_tempo) {
    atualizar_tela_tempo = false;
    tft.fillRect(0, 215, 450, 40, TFT_BLACK); 
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans12pt7b); 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    String texto_tempo = formatarTempo(tempo_atual_ms);
    if (duracao_total_ms > 0) {
      texto_tempo += " / " + formatarTempo(duracao_total_ms);
    }
    tft.drawString(texto_tempo, tft.width() / 2, 240); 
    tft.setTextFont(4); 
  }

 // ... (Cronômetro e Barra de volume ficam intocados lá em cima)

  if (atualizar_tela_conexao) {
    atualizar_tela_conexao = false; 
    tft.fillRect(0, 0, tft.width(), 65, TFT_BLACK); 
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    
    if (bluetooth_conectado) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      // CORREÇÃO: Se já tivermos o nome real do S25 gravado, mostramos ele. 
      // Se a memória estiver vazia, mostramos a frase genérica provisória.
      if (nome_celular == "" || nome_celular == "Nao conectado") {
        tft.drawString("Dispositivo Conectado", tft.width() / 2, 35);
      } else {
        tft.drawString(nome_celular, tft.width() / 2, 35);
      }
      desenhar_logo_bluetooth(450, 32, TFT_CYAN); 
    } else {
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.drawString("Nao conectado", tft.width() / 2, 35);
      tft.fillRect(0, 80, tft.width(), 240, TFT_BLACK); 
      precisa_scroll = false;
      duracao_total_ms = 0;
      tempo_atual_ms = 0;
      
      // O RESET FUNDAMENTAL: Limpa a memória para o próximo celular que conectar!
      nome_celular = "Nao conectado"; 
      musica_atual = "";
      artista_atual = "";
      
      atualizar_tela_volume = true; 
    }
  }

  if (bluetooth_conectado) {
    String nome_real = a2dp_sink.get_peer_name();
    // Impede que "picos" vazios ou nomes genéricos da biblioteca sobrescrevam o nome correto
    if (nome_real != "" && nome_real != "<unknown>" && nome_real != nome_celular) {
        nome_celular = nome_real;
        tft.fillRect(0, 0, 430, 65, TFT_BLACK); 
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(4);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(nome_celular, tft.width() / 2, 35);
    }
  }

  // ... (Daqui para baixo segue a parte dos Metadados, inalterada)

  if (atualizar_tela_metadados) {
    atualizar_tela_metadados = false; 
    tft.setTextDatum(MC_DATUM);

    // 2. A "SUPER BORRACHA" EXPANDIDA
    // Inicia no Y=71 (para não apagar a sua linha cinza do Y=70)
    // E vai até o Y=149 (Altura 79). 
    // Isso cobre tanto o letreiro rolante (que subiu) quanto as pernas do estático (que desceram).
    tft.fillRect(0, 71, tft.width(), 79, TFT_BLACK); 
    
    tft.fillRect(0, 150, 450, 60, TFT_BLACK); 
    
    tft.setFreeFont(&FreeSansBold18pt7b); 
    largura_musica = tft.textWidth(musica_atual); 
    
    if (largura_musica > 450) { 
      precisa_scroll = true;
      posicao_scroll = 40; 
    } else {
      precisa_scroll = false;
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      // Mantém a posição original do texto estático que você gosta
      tft.drawString(musica_atual, tft.width() / 2, 125);
    }

    tft.setFreeFont(&FreeSans12pt7b); 
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK); 
    tft.drawString(artista_atual, tft.width() / 2, 180);
    
    tft.setTextFont(4); 
  }

  if (atualizar_tela_status) {
    atualizar_tela_status = false; 
    int cx = tft.width() / 2;
    int cy = 285; 
    
    tft.fillRect(cx - 20, cy - 20, 40, 40, TFT_BLACK); 
    if (musica_tocando) {
      tft.fillTriangle(cx - 10, cy - 15, cx - 10, cy + 15, cx + 15, cy, TFT_CYAN); 
    } else {
      tft.fillRect(cx - 12, cy - 15, 8, 30, TFT_RED); 
      tft.fillRect(cx + 4, cy - 15, 8, 30, TFT_RED);  
    }
  }

  if (precisa_scroll && musica_tocando) { 
    if (millis() - ultimo_scroll > 20) { 
      ultimo_scroll = millis();
      spriteMusica.fillSprite(TFT_BLACK);
      
      spriteMusica.setFreeFont(&FreeSansBold18pt7b); 
      spriteMusica.setTextColor(TFT_WHITE, TFT_BLACK);
      spriteMusica.setTextDatum(ML_DATUM); 
      
      int espaco_vazio = 100; 
      // 3. TEXTO NO MEIO EXATO DO NOVO SPRITE
      // Com altura 70, o Y=35 é o centro. Pernas e topos ficam intactos.
      spriteMusica.drawString(musica_atual, posicao_scroll, 35);
      spriteMusica.drawString(musica_atual, posicao_scroll + largura_musica + espaco_vazio, 35);
      
      // 4. POSIÇÃO MAIS ALTA DO LETREIRO
      // Desenhado no Y=75 para acomodar o espaço das pernas da fonte sem tocar na Barra de Volume.
      spriteMusica.pushSprite(10, 75);
      
      posicao_scroll -= 2;
      if (posicao_scroll <= -(largura_musica + espaco_vazio)) {
        posicao_scroll = 0; 
      }
    }
  }
}