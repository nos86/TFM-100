#include "cli.h"

#include <config.h>
#include <PT100.h>
#include <flow.h>
#include <scheduler.h>

// External variables
extern PT100 supply_sensor; // main.cpp
extern PT100 return_sensor; // main.cpp
extern Flow flowObj;        // main.cpp
extern Scheduler scheduler; // main.cpp
extern uint8_t node_id;     // main.cpp

// Costruttore
CLIScreenManager::CLIScreenManager(Stream *stream, uint16_t width, uint16_t height)
{
    ansi = new ANSI(stream);
    terminal_width = width;
    terminal_height = height;
    last_screen = ScreenType::NONE;
    ansi_enabled = true;
    clientConnected = false;
    periodic_update = false;
}

// Distruttore
CLIScreenManager::~CLIScreenManager()
{
    // Non delete ansi dato che non ha distruttore virtuale
    // La memoria sarà liberata automaticamente alla fine del programma
}

// Inizializzazione
void CLIScreenManager::begin()
{
    if (!ansi_enabled)
        return;

    // Tenta di rilevare la dimensione dello schermo
    uint16_t w, h;
    if (ansi->getScreenSize(w, h, 500))
    {
        terminal_width = w;
        terminal_height = h;
    }
    clearScreen();
    ansi->cursorHide();
}

void CLIScreenManager::enableANSI(bool enable)
{
    ansi_enabled = enable;
}

void CLIScreenManager::setTerminalSize(uint16_t width, uint16_t height)
{
    terminal_width = width;
    terminal_height = height;
}

void CLIScreenManager::periodic()
{
    periodic_update = true;
}

// Loop
void CLIScreenManager::process()
{
    ScreenType next_screen = ScreenType::NONE;
    if (Serial.dtr() == LOW)
    {
        if (clientConnected)
        {
            clientConnected = false;
            last_screen = ScreenType::NONE;
        }
        return;
    }
    else if (Serial.dtr() == HIGH && !clientConnected)
    {
        clientConnected = true;
        ansi->clearScreen();
        next_screen = ScreenType::MAIN_MENU;
    }
    // Check input
    char input = 0;
    if (ansi->available() > 0)
    {
        input = ansi->read();
        // Check if user is changing page
        switch (input)
        {
        case 'v':
        case 'V':
            next_screen = ScreenType::MAIN_MENU;
            input = 0;
            break;
        case 'd':
        case 'D':
            next_screen = ScreenType::DIAGNOSTICS;
            input = 0;
            break;
        case 'e':
        case 'E':
            next_screen = ScreenType::SYSTEM_INFO;
            input = 0;
            break;
        case 'c':
        case 'C':
            next_screen = ScreenType::CALIBRATION;
            input = 0;
            break;
        case 'l':
        case 'L':
            next_screen = ScreenType::LOG;
            input = 0;
            break;
        default:
            break;
        }
    }
    if (next_screen != ScreenType::NONE || input > 0 || periodic_update)
    {
        bool full_update = false;
        if (next_screen == ScreenType::NONE)
            next_screen = last_screen;
        else
        {
            clearScreen();
            full_update = true;
            last_screen = next_screen;
        }
        switch (next_screen)
        {
        case ScreenType::NONE: // this case should never happen. Fallback to MAIN_MENU
        case ScreenType::MAIN_MENU:
            drawRealTimeSignals(full_update);
            break;
        case ScreenType::DIAGNOSTICS:
            drawDiagnosticsScreen();
            break;
        case ScreenType::SYSTEM_INFO:
            drawSystemInfoScreen();
            break;
        case ScreenType::CALIBRATION:
            drawCalibrationScreen(input);
            break;
        case ScreenType::LOG:
            drawLogScreen(full_update);
            break;
        }
        next_screen = ScreenType::NONE;
        periodic_update = false;
    }
}

// Metodi privati per la formattazione

void CLIScreenManager::printCenteredText(const String &text)
{
    if (!ansi_enabled)
        return;

    uint8_t padding = (terminal_width - text.length()) / 2;
    for (uint8_t i = 0; i < padding; i++)
    {
        ansi->print(" ");
    }
    ansi->println(text);
}

bool CLIScreenManager::drawHardwareFailure(bool mcp_init, bool mcp_normal, bool supply_init, bool return_init)
{
    if (!ansi_enabled)
        return false;

    if ((Serial.dtr() == LOW) && (clientConnected))
    {
        clientConnected = false;
        return false;
    }
    else if (Serial.dtr() == HIGH && !clientConnected)
    {
        clientConnected = true;
        ansi->cursorHide();

        printHeader("TFM-100 - HARDWARE FAILURE", ansi->red);
        printSeparator('-');

        ansi->gotoXY(1, 4);
        ansi->println();
        printStatusIndicator(F("CAN Controller Initialization"), mcp_init);
        printStatusIndicator(F("CAN Controller Operation"), mcp_normal);
        printStatusIndicator(F("PT100 Supply Sensor Initialization"), supply_init);
        printStatusIndicator(F("PT100 Return Sensor Initialization"), return_init);
        printFooter(true);
    }

    ansi->gotoXY(1, 11);
    ansi->print(F("Reboot in "));
    ansi->print(60 - (millis() / 1000) % 60);
    ansi->println(F(" seconds..."));

    if (ansi->available() > 0)
    {
        String inputStr = String((char)ansi->read());
        inputStr.toLowerCase();
        if (inputStr.equals("r"))
            return true;
    }
    return false;
}

void CLIScreenManager::drawRealTimeSignals(bool full_update)
{
    if (!ansi_enabled)
        return;

    if (full_update)
    {
        clearScreen();
        ansi->cursorHide();
        printHeader(F("TFM-100 - LETTURA VALORI"));
        printSeparator('-');

        ansi->gotoXY(1, 4);
        ansi->println();

        ansi->println(F("Temperatura Mandata: "));
        ansi->foreground(ansi->green);
        ansi->normal();

        ansi->println(F("Temperatura Ritorno: "));
        ansi->foreground(ansi->green);
        ansi->normal();

        ansi->println(F("Flusso: "));
        ansi->foreground(ansi->green);
        ansi->normal();

        ansi->println();
        ansi->bold();
        ansi->println(F("Potenza termica: "));
        ansi->normal();
        ansi->println();
        ansi->println();

        ansi->println();
        ansi->println(F("Energia nelle ultime 24h: "));

        ansi->println(F("Energia totale: "));

        ansi->println();
        ansi->println(F("Uptime: "));
        ansi->println();
        printFooter();
    }
    // Update values
    float hours = (float)scheduler.getUptime() / 3600.0;
    ansi->gotoXY(22, 5);
    ansi->foreground(ansi->green);
    ansi->print(String(supply_sensor.last_temperature, 1) + " °C   ");
    ansi->gotoXY(22, 6);
    ansi->print(String(return_sensor.last_temperature, 1) + " °C   ");
    ansi->gotoXY(9, 7);
    ansi->print(String(flowObj.getFlow()) + " l/h   ");
    ansi->normal();
    ansi->bold();
    ansi->gotoXY(18, 9);
    ansi->println(String(power, 2) + " kW   ");
    printProgressBar(power_percentage, 30, "");
    ansi->print("  ");
    ansi->normal();
    ansi->gotoXY(27, 13);
    ansi->print(String(energy_24h, 2) + " kWh   ");
    ansi->gotoXY(17, 14);
    ansi->print(String(energy_total / 1000.0, 2) + " MWh   ");
    ansi->gotoXY(9, 16);
    ansi->print(String(hours, 2) + " hours   ");
}

void CLIScreenManager::drawDiagnosticsScreen(bool full_update)
{
    if (!ansi_enabled)
        return;

    printHeader("TFM-100 - MEMORIA ERRORI");
    printSeparator('=');

    ansi->gotoXY(1, 4);
}

void CLIScreenManager::drawSystemInfoScreen(bool full_update)
{
    if (!ansi_enabled)
        return;

    printHeader("TFM-100 - INFORMAZIONI SISTEMA");
    printSeparator('=');

    ansi->gotoXY(1, 4);

    String headers[] = {"Parametro", "Valore"};
    String data[][2] = {
        {"Firmware Version", String(FIRMWARE_VERSION, HEX)},
        {"Model", "TFM-100"},
        {"Variant", String(VARIANT, HEX)},
        {"Node ID Base", String(NODE_ID_BASE, HEX)},
        {"Uptime", String(millis() / 1000) + " s"},
        {"Terminal Size", String(terminal_width) + "x" + String(terminal_height)}};

    printTable(headers, data, 7, 2);
}

void CLIScreenManager::drawCalibrationScreen(bool full_update, char input)
{
    if (!ansi_enabled)
        return;

    printHeader(F("TFM-100 - CONFIGURAZIONE"));
    printSeparator('=');

    ansi->gotoXY(1, 4);

    ansi->println("Parametri configurabili:");
    ansi->println();
}

void CLIScreenManager::drawLogScreen(bool full_update)
{
    if (!ansi_enabled || !full_update)
        return;

    printHeader(F("TFM-100 - LOG EVENTI"));
    printSeparator('=');

    logRow = 0;

    printFooter();
}

void CLIScreenManager::logMessage(const String &message, LogType severity)
{
    uint8_t available_rows = terminal_height - 5;
    if (!ansi_enabled || last_screen != ScreenType::LOG)
        return;

    if (++logRow > available_rows)
        logRow = 1;

    ansi->gotoXY(1, 2 + logRow);
    uint8_t color = ansi->white;
    String level;
    switch (severity)
    {
    case LogType::INFO:
        color = ansi->cyan;
        level = "INFO";
        break;
    case LogType::WARNING:
        color = ansi->yellow;
        level = "WARNING";
        break;
    case LogType::ERROR:
        color = ansi->red;
        level = "ERROR";
        break;
    case LogType::SUCCESS:
        color = ansi->green;
        level = "SUCCESS";
        break;
    }

    ansi->foreground(color);
    ansi->bold();
    ansi->print("[");
    ansi->print(level);
    ansi->print("] ");
    ansi->normal();
    ansi->println(message);
    if (logRow < available_rows)
        ansi->clearLine(ansi->entireLine);
}

// Metodi di utilità per la formattazione
void CLIScreenManager::printColoredText(const String &text, uint8_t color, bool newline)
{
    if (!ansi_enabled)
    {
        ansi->print(text);
        if (newline)
            ansi->println();
        return;
    }

    ansi->foreground(color);
    ansi->print(text);
    ansi->normal();
    if (newline)
        ansi->println();
}

void CLIScreenManager::printProgressBar(uint8_t progress, uint8_t width, const String &label)
{
    if (!ansi_enabled)
        return;

    if (label.length() > 0)
    {
        ansi->print(label);
        ansi->print(": ");
    }

    ansi->print("[");

    uint8_t filled = (progress * width) / 100;

    ansi->foreground(ansi->green);
    for (uint8_t i = 0; i < filled; i++)
    {
        ansi->print("=");
    }

    ansi->foreground(ansi->grey2color(64));
    for (uint8_t i = filled; i < width; i++)
    {
        ansi->print("-");
    }

    ansi->normal();
    ansi->print("] ");
    ansi->print(progress);
    ansi->println("%");
}

void CLIScreenManager::printTable(const String headers[], const String data[][2], uint8_t rows, uint8_t cols)
{
    if (!ansi_enabled)
        return;

    // Stampa header
    ansi->foreground(ansi->yellow);
    ansi->bold();
    for (uint8_t c = 0; c < cols; c++)
    {
        ansi->print(headers[c]);
        if (c < cols - 1)
            ansi->print("\t\t");
    }
    ansi->normal();
    ansi->println();

    printSeparator('-');

    // Stampa dati
    for (uint8_t r = 0; r < rows; r++)
    {
        for (uint8_t c = 0; c < cols; c++)
        {
            if (c == 0)
            {
                ansi->foreground(ansi->cyan);
            }
            else
            {
                ansi->foreground(ansi->white);
            }
            ansi->print(data[r][c]);
            ansi->normal();
            if (c < cols - 1)
                ansi->print("\t\t");
        }
        ansi->println();
    }
}

void CLIScreenManager::printStatusIndicator(const String &label, bool status, const String &ok_text, const String &error_text)
{
    if (!ansi_enabled)
        return;

    ansi->print(label);
    ansi->print(": ");

    if (status)
    {
        ansi->foreground(ansi->green);
        ansi->print("[" + ok_text + "]");
    }
    else
    {
        ansi->foreground(ansi->red);
        ansi->print("[" + error_text + "]");
    }

    ansi->normal();
    ansi->println();
}

/* INTERNAL TECHNICAL FUNCTIONS*/

void CLIScreenManager::clearScreen()
{
    if (!ansi_enabled)
        return;
    ansi->clearScreen();
    ansi->home();
}

void CLIScreenManager::clearArea(uint8_t start_row, uint8_t end_row)
{
    if (!ansi_enabled)
        return;

    for (uint8_t row = start_row; row <= end_row; row++)
    {
        ansi->gotoXY(1, row);
        ansi->clearLine(ansi->entireLine);
    }
}

void CLIScreenManager::printHeader(const String &title, uint8_t background, uint8_t foreground)
{
    if (background == 255)
        background = ansi->blue;
    if (foreground == 255)
        foreground = ansi->white;

    if (!ansi_enabled)
        return;

    ansi->gotoXY(1, 1);
    ansi->color(foreground, background);
    ansi->bold();

    // Stampa il titolo centrato
    uint8_t padding = (terminal_width - title.length()) / 2;
    for (uint8_t i = 0; i < terminal_width; i++)
    {
        if (i == padding)
        {
            ansi->print(title);
            i += title.length() - 1;
        }
        else
        {
            ansi->print(" ");
        }
    }

    ansi->normal();
    ansi->println();
}

void CLIScreenManager::printSeparator(char character)
{
    if (!ansi_enabled)
        return;

    ansi->foreground(ansi->cyan);
    for (uint8_t i = 0; i < terminal_width; i++)
    {
        ansi->print(character);
    }
    ansi->normal();
    ansi->println();
}

void CLIScreenManager::printFooter(bool failure)
{
    ansi->gotoXY(1, terminal_height - 2);
    printSeparator('-');
    ansi->foreground(ansi->yellow);
    ansi->print(F("Firmware v"));
    ansi->print(FIRMWARE_VERSION, HEX);
    ansi->print(F(" | Modello: TFM-100 v"));
    ansi->print(MODEL, HEX);
    ansi->print(F(" | Node ID: 0x"));
    ansi->print(node_id, HEX);
    ansi->normal();
    ansi->println();

    if (failure)
    {
        ansi->print(F("Press R for Reboot"));
    }
    else
        // Istruzioni di navigazione
        ansi->print(F("Navigazione: Valori (V) | Device (D) | Errori (E) | Calibrazione (C) | Log (L)"));
}
