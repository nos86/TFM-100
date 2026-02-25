#ifndef CLI_H
#define CLI_H

#include <Arduino.h>
#include <ansi.h>

// Tipi di schermata disponibili
enum class ScreenType
{
    NONE,        // A nessuna schermata (stato iniziale)
    MAIN_MENU,   // Dati in tempo reale dei sensori
    DIAGNOSTICS, // Schermata diagnostica hardware/software
    SYSTEM_INFO, // Informazioni sistema e configurazione
    CALIBRATION, // Schermata di calibrazione
    LOG,         // Log dei messaggi di sistema
};

enum class LogType
{
    INFO,    // Informazioni generali
    WARNING, // Avvisi di attenzione
    ERROR,   // Errori critici
    SUCCESS, // Operazioni riuscite
};

class CLIScreenManager
{
private:
    ANSI *ansi;
    uint16_t terminal_width;
    uint16_t terminal_height;
    ScreenType last_screen;
    bool ansi_enabled;
    bool periodic_update;
    uint8_t logRow = 0;

    // Metodi privati per la formattazione
    void printHeader(const __FlashStringHelper *title, uint8_t background = 255, uint8_t foreground = 255);
    void printFooter(bool failure = false);
    void printSeparator(char character = '-');
    void printMenuFooter();
    void printCenteredText(const __FlashStringHelper *text);
    void clearArea(uint8_t start_row, uint8_t end_row);
    void logMessage(const __FlashStringHelper *message, LogType severity = LogType::INFO);
    void logMessage(const char *message, LogType severity = LogType::INFO);

public:
    // Costruttore
    CLIScreenManager(Stream *stream = &Serial, uint16_t width = 80, uint16_t height = 24);

    // Distruttore
    ~CLIScreenManager();

    // Inizializzazione
    void begin();
    void enableANSI(bool enable = true);
    void setTerminalSize(uint16_t width, uint16_t height);

    // Loop
    void process();
    void periodic();

    // Controllo schermo
    void clearScreen();

    bool clientConnected = false;

    // Metodi di disegno delle schermate
    void drawRealTimeSignals(bool full_update = true);
    bool drawHardwareFailure(bool mcp_init, bool mcp_normal, bool supply_init, bool return_init);
    void drawDiagnosticsScreen(bool full_update = true, char input = 0);
    void drawSystemInfoScreen(bool full_update = true);
    void drawCalibrationScreen(bool full_update = true, char input = 0);
    void drawLogScreen(bool full_update = true);

    inline void logInfo(const __FlashStringHelper *message) { logMessage(message, LogType::INFO); }
    inline void logWarning(const __FlashStringHelper *message) { logMessage(message, LogType::WARNING); }
    inline void logError(const __FlashStringHelper *message) { logMessage(message, LogType::ERROR); }
    inline void logSuccess(const __FlashStringHelper *message) { logMessage(message, LogType::SUCCESS); }

    // Overload per compatibilità con stringhe C normali
    void logInfo(const char *message);
    void logWarning(const char *message);
    void logError(const char *message);
    void logSuccess(const char *message);

    // Metodi di utilità per la formattazione
    void printColoredText(const __FlashStringHelper *text, uint8_t color, bool newline = true);
    void printProgressBar(uint8_t progress, uint8_t width = 50, const __FlashStringHelper *label = nullptr);
    void printTable(const __FlashStringHelper *headers[], const __FlashStringHelper *data[][2], uint8_t rows, uint8_t cols);
    void printStatusIndicator(const __FlashStringHelper *label, bool status, const __FlashStringHelper *ok_text = nullptr, const __FlashStringHelper *error_text = nullptr);

    // Accesso diretto alla libreria ANSI per funzioni avanzate
    ANSI *getANSI() { return ansi; }
};

#endif // CLI_H