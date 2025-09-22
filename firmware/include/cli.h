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

    // FIXME: remove these variables
    float power, power_percentage, energy_24h, energy_total;

    // Metodi privati per la formattazione
    void printHeader(const String &title, uint8_t background = 255, uint8_t foreground = 255);
    void printFooter(bool failure = false);
    void printSeparator(char character = '-');
    void printCenteredText(const String &text);
    void clearArea(uint8_t start_row, uint8_t end_row);

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
    void drawDiagnosticsScreen(bool full_update = true);
    void drawSystemInfoScreen(bool full_update = true);
    void drawCalibrationScreen(bool full_update = true, char input = 0);

    // Metodi di utilità per la formattazione
    void printColoredText(const String &text, uint8_t color, bool newline = true);
    void printProgressBar(uint8_t progress, uint8_t width = 50, const String &label = "");
    void printTable(const String headers[], const String data[][2], uint8_t rows, uint8_t cols);
    void printStatusIndicator(const String &label, bool status, const String &ok_text = "OK", const String &error_text = "ERROR");

    // Accesso diretto alla libreria ANSI per funzioni avanzate
    ANSI *getANSI() { return ansi; }
};

#endif // CLI_H