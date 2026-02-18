#include <Arduino.h>
#include "BluetoothA2DPSink.h"
#include <TFT_eSPI.h>

BluetoothA2DPSink a2dp_sink;
TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite spriteMusica = TFT_eSprite(&tft); 

volatile bool atualizar_tela_conexao = false;
volatile bool atualizar_tela_metadados = false;
volatile bool atualizar_tela_status = false;
volatile bool atualizar_tela_tempo = false;

bool bluetooth_conectado = false;
bool musica_tocando = false;
String nome_celular = "Nao conectado";
String musica_atual = "";
String artista_atual = "";

int largura_musica = 0;
int posicao_scroll = 460;
unsigned long ultimo_scroll = 0;
bool precisa_scroll = false;

uint32_t tempo_atual_ms = 0;
uint32_t duracao_total_ms = 0;
unsigned long ultimo_tick_tempo = 0;

int volume_atual = 0;
int volume_alvo = 127;
bool fazendo_fadein = false;
unsigned long tempo_ultimo_fade = 0;

String formatarTempo(uint32_t ms) {
  uint32_t segundos = ms / 1000;
  uint32_t minutos = segundos / 60;
  segundos = segundos % 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", minutos, segundos);
  return String(buf);
}

// --- FUNÇÃO 1: Metadados ---
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

// --- FUNÇÃO 2: Status ---
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  if (state == ESP_A2D_AUDIO_STATE_STARTED) {
    volume_atual = 0;
    a2dp_sink.set_volume(0);
    fazendo_fadein = true;
    tempo_ultimo_fade = millis();
    musica_tocando = true;
    atualizar_tela_status = true;
  } 
  else if (state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND || state == ESP_A2D_AUDIO_STATE_STOPPED) {
    a2dp_sink.set_volume(0);
    fazendo_fadein = false;
    musica_tocando = false;
    atualizar_tela_status = true;
  }
}

// --- FUNÇÃO 3: Conexão ---
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    bluetooth_conectado = true;
  } else {
    bluetooth_conectado = false;
  }
  atualizar_tela_conexao = true;
}

// --- FUNÇÃO 4: Saltos no Tempo (Seek) ---
void avrc_play_pos_callback(uint32_t play_pos) {
  tempo_atual_ms = play_pos;
  ultimo_tick_tempo = millis(); 
  atualizar_tela_tempo = true;
}

// --- FUNÇÃO 5: Mudança ou Reinício de Faixa (O Alarme!) ---
void avrc_track_change_callback(uint8_t *id) {
  // O celular bateu nesse alarme? Significa que a faixa reiniciou ou trocou!
  tempo_atual_ms = 0;
  ultimo_tick_tempo = millis(); 
  atualizar_tela_tempo = true;
}

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK); 
  spriteMusica.createSprite(460, 60);

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
  
  // Registra a nova função de detecção de faixa!
  a2dp_sink.set_avrc_rn_track_change_callback(avrc_track_change_callback); 
  
  a2dp_sink.start("Cerato_Bluetooth"); 
}

void loop() {
  // Fade-in
  if (fazendo_fadein) {
    if (millis() - tempo_ultimo_fade > 10) { 
      tempo_ultimo_fade = millis();
      volume_atual += 5;
      if (volume_atual >= volume_alvo) {
        volume_atual = volume_alvo;
        fazendo_fadein = false; 
      }
      a2dp_sink.set_volume(volume_atual);
    }
  }

  // Relógio Lógico
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

  // --- DESENHO DO CRONÔMETRO (AGORA COM FONTE MAIOR) ---
  if (atualizar_tela_tempo) {
    atualizar_tela_tempo = false;
    tft.fillRect(0, 215, tft.width(), 40, TFT_BLACK); // Borracha maior
    tft.setTextDatum(MC_DATUM);
    
    // Usa a fonte GFX Premium para o relógio também!
    tft.setFreeFont(&FreeSans12pt7b); 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    String texto_tempo = formatarTempo(tempo_atual_ms);
    if (duracao_total_ms > 0) {
      texto_tempo += " / " + formatarTempo(duracao_total_ms);
    }
    // Desce um pouco a posição Y (240) para acomodar o tamanho da fonte
    tft.drawString(texto_tempo, tft.width() / 2, 240); 
    
    tft.setTextFont(4); // Volta para a fonte padrão para não bagunçar o resto!
  }

  // Desenho da Conexão
  if (atualizar_tela_conexao) {
    atualizar_tela_conexao = false; 
    tft.fillRect(0, 0, tft.width(), 65, TFT_BLACK); 
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    
    if (bluetooth_conectado) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString("Dispositivo Conectado", tft.width() / 2, 35);
    } else {
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.drawString("Nao conectado", tft.width() / 2, 35);
      tft.fillRect(0, 80, tft.width(), 240, TFT_BLACK); 
      precisa_scroll = false;
      duracao_total_ms = 0;
      tempo_atual_ms = 0;
    }
  }

  // Busca do Nome do Celular
  if (bluetooth_conectado) {
    String nome_real = a2dp_sink.get_peer_name();
    if (nome_real != "" && nome_real != nome_celular) {
        nome_celular = nome_real;
        tft.fillRect(0, 0, tft.width(), 65, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(4);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(nome_celular, tft.width() / 2, 35);
    }
  }

  // Desenho dos Metadados (Música e Artista)
  if (atualizar_tela_metadados) {
    atualizar_tela_metadados = false; 
    tft.setTextDatum(MC_DATUM);
    tft.setTextPadding(tft.width());
    
    tft.setFreeFont(&FreeSansBold18pt7b); 
    largura_musica = tft.textWidth(musica_atual); 
    
    if (largura_musica > 460) {
      precisa_scroll = true;
      posicao_scroll = 40; 
      tft.fillRect(0, 80, tft.width(), 70, TFT_BLACK); 
    } else {
      precisa_scroll = false;
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(musica_atual, tft.width() / 2, 130);
    }

    tft.setFreeFont(&FreeSans12pt7b); 
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK); 
    tft.drawString(artista_atual, tft.width() / 2, 180);
    
    tft.setTextFont(4); 
  }

  // Desenho dos Símbolos de Play/Pause
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

  // Motor do Letreiro Rolante
  if (precisa_scroll && musica_tocando) { 
    if (millis() - ultimo_scroll > 20) { 
      ultimo_scroll = millis();
      spriteMusica.fillSprite(TFT_BLACK);
      
      spriteMusica.setFreeFont(&FreeSansBold18pt7b); 
      spriteMusica.setTextColor(TFT_WHITE, TFT_BLACK);
      spriteMusica.setTextDatum(ML_DATUM); 
      
      int espaco_vazio = 100; 
      spriteMusica.drawString(musica_atual, posicao_scroll, 40);
      spriteMusica.drawString(musica_atual, posicao_scroll + largura_musica + espaco_vazio, 40);
      
      spriteMusica.pushSprite(10, 80);
      
      posicao_scroll -= 2;
      if (posicao_scroll <= -(largura_musica + espaco_vazio)) {
        posicao_scroll = 0; 
      }
    }
  }
}