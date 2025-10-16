#include "cli.h"

#include <config.h>
#include <PT100.h>
#include <flow.h>
#include <scheduler.h>
#include <mcp_can.h>
#include <MAX31865_NonBlocking.h>
#include <diagnostics.h>
#include <utils.h>
#include <energy.h>
#include <power.h>

// External variables
extern PT100 supply_sensor; // main.cpp
extern PT100 return_sensor; // main.cpp
extern Flow flowObj;        // main.cpp
extern Scheduler scheduler; // main.cpp
extern uint8_t node_id;     // main.cpp
extern MCP_CAN CAN0;        // main.cpp
extern Diagnostics DSM;     // main.cpp
extern Energy energyObj;    // main.cpp
extern Power powerObj;      // main.cpp

// Constructor
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

// Destructor
CLIScreenManager::~CLIScreenManager()
{
    // Do not delete ansi since it has no virtual destructor
    // Memory will be freed automatically at program end
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
            next_screen = ScreenType::SYSTEM_INFO;
            input = 0;
            break;
        case 'e':
        case 'E':
            next_screen = ScreenType::DIAGNOSTICS;
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
            drawDiagnosticsScreen(full_update, input);
            break;
        case ScreenType::SYSTEM_INFO:
            drawSystemInfoScreen(full_update);
            break;
        case ScreenType::CALIBRATION:
            drawCalibrationScreen(full_update, input);
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

void CLIScreenManager::printCenteredText(const __FlashStringHelper *text)
{
    if (!ansi_enabled)
        return;

    uint8_t text_length = strlen_P((const char *)text);
    uint8_t padding = (terminal_width - text_length) / 2;
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

        printHeader(F("TFM-100 - HARDWARE FAILURE"), ansi->red);
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
        char inputChar = ansi->read();
        if (inputChar == 'r' || inputChar == 'R')
        {
            return true; // Request reboot
        }
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
    char buf[20];
    float hours = (float)scheduler.getUptime() / 3600.0;
    ansi->gotoXY(22, 5);
    ansi->foreground(ansi->green);
    ftostr(buf, supply_sensor.last_temperature, 1);
    ansi->print(buf);
    ansi->print(" °C   ");
    ansi->gotoXY(22, 6);
    ansi->foreground(ansi->green);
    ftostr(buf, return_sensor.last_temperature, 1);
    ansi->print(buf);
    ansi->print(" °C   ");
    ansi->gotoXY(9, 7);
    ansi->foreground(ansi->green);
    ansi->print(flowObj.getFlow());
    ansi->print(" l/h   ");
    ansi->normal();
    ansi->bold();
    ansi->gotoXY(18, 9);
    ftostr(buf, powerObj.getPower(), 1);
    ansi->print(buf);
    ansi->print(" kW   ");
    printProgressBar((uint8_t)powerObj.getPowerPercent(), 30);
    ansi->normal();
    ansi->gotoXY(27, 13);
    ftostr(buf, energyObj.getEnergy24h(), 2);
    ansi->print(buf);
    ansi->print(" kWh   ");
    ansi->gotoXY(17, 14);
    ftostr(buf, energyObj.getEnergyTotal() / 1000.0, 2);
    ansi->print(buf);
    ansi->print(" MWh   ");
    ansi->gotoXY(9, 16);
    ftostr(buf, hours, 2);
    ansi->print(buf);
    ansi->print(" hours   ");
}

void CLIScreenManager::drawDiagnosticsScreen(bool full_update, char input)
{
    static bool no_errors = true;
    if (!ansi_enabled)
        return;

    // Handle input for reset errors
    if (input == 'r' || input == 'R')
        DSM.clear();

    if (full_update)
    {
        printHeader(F("TFM-100 - MEMORIA ERRORI"));
        printSeparator('=');

        // Print table header using printTable style
        ansi->foreground(ansi->yellow);
        ansi->bold();
        ansi->print(F("ID\tError Description\t\tState\t\tSeverity\tOcc."));
        ansi->normal();
        ansi->println();
        printSeparator('-');
    }

    /* Show Diagnostics Data in Table Format */
    const dtc_history_t *errors = DSM.getMemory();
    uint8_t numberOfStoredErrors = DSM.numberOfErrors();

    // Erase old printed area
    clearArea(5 + numberOfStoredErrors, 5 + DIAGNOSTICS_MEMORY_SIZE);
    ansi->gotoXY(1, 5);
    if (numberOfStoredErrors == 0)
    {
        ansi->println(F("No errors stored in memory"));
    }
    else
    {
        if (no_errors)
            ansi->clearLine(ansi->entireLine);
        // Print table rows
        for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
        {
            if (dtc_get_state(&errors[i]) != DTC_EMPTY)
            {
                // ID column (cyan color like printTable)
                ansi->foreground(ansi->cyan);
                ansi->print(i);
                ansi->normal();
                ansi->print(F("\t"));

                // Error Description column (white color)
                ansi->foreground(ansi->white);
                const char *description = DSM.getErrorDescription(i);
                if (description)
                {
                    ansi->print(description);
                }
                else
                {
                    ansi->print(F("Unknown DTC"));
                }
                ansi->normal();
                ansi->print(F("\t\t"));

                // State column (white color)
                ansi->foreground(ansi->white);
                switch (dtc_get_state(&errors[i]))
                {
                case DTC_PENDING:
                    ansi->print(F("Pending"));
                    break;
                case DTC_ACTIVE:
                    ansi->print(F("Active "));
                    break;
                case DTC_HEALING:
                    ansi->print(F("Healing"));
                    break;
                case DTC_HISTORY:
                    ansi->print(F("History"));
                    break;
                default:
                    ansi->print(F("Unknown"));
                    break;
                }
                ansi->normal();
                ansi->print(F("\t\t"));

                // Severity column (color-coded by severity level)
                uint8_t severity = DSM.getErrorSeverity(i);
                if (severity != 0xFF)
                {
                    // Color-code severity: 1=white, 2-4=red 5+=background red
                    if (severity < 1)
                        ansi->foreground(ansi->white);
                    else if (severity < 4)
                        ansi->foreground(ansi->red);
                    else
                    {
                        ansi->background(ansi->red);
                        ansi->foreground(ansi->white);
                    }
                    ansi->print("  ");
                    ansi->print(severity);
                    ansi->print("  ");
                }
                else
                {
                    ansi->foreground(ansi->white);
                    ansi->print(F("?"));
                }
                ansi->normal();
                ansi->print(F("\t\t"));

                // Occurrences column (white color)
                ansi->foreground(ansi->white);
                ansi->print(errors[i].occurrence);
                ansi->normal();
                ansi->println();
            }
        }
    }

    if (full_update)
    {
        ansi->gotoXY(1, terminal_height - 2);
        printSeparator('-');
        printMenuFooter();
    }
    ansi->gotoXY(1, terminal_height - 3);
    ansi->print(F("Total errors: "));
    ansi->print(numberOfStoredErrors);
    ansi->print(F(" | Active: "));
    ansi->print(DSM.numberOfActiveErrors());
    ansi->println(F("\tPress 'R' to reset errors"));

    no_errors = (numberOfStoredErrors == 0);
}

void CLIScreenManager::drawSystemInfoScreen(bool full_update)
{
    if (!ansi_enabled)
        return;

    if (full_update)
    {
        printHeader(F("TFM-100 - INFORMAZIONI SISTEMA"));
        printSeparator('=');

        printFooter();
    }
    ansi->gotoXY(1, 4);
    /* Show data for PT100 sensors */
    MAX31865 *sensor;
    for (uint8_t s = 0; s < 2; s++)
    {
        ansi->foreground(ansi->green);
        ansi->clearLine(ansi->entireLine);
        if (s == 0)
        {
            ansi->print(F("Supply"));
            sensor = &supply_sensor.sensor;
        }
        else
        {
            ansi->print(F("Return"));
            sensor = &return_sensor.sensor;
        }
        ansi->println(F(" Temperature: "));
        ansi->normal();
        ansi->print(F(" LOW(0x05) = "));
        ansi->print(sensor->getLowerThreshold(), HEX);
        ansi->print(" - HIGH(0x06) = ");
        ansi->print(sensor->getUpperThreshold(), HEX);
        ansi->print(" - ERR(0x07) = ");
        ansi->println(sensor->getFault(), HEX);
        ansi->clearLine(ansi->entireLine);
        ansi->print(F(" Vbias="));
        ansi->print(sensor->getBias() ? F("ON") : F("OFF"));
        ansi->print(F(" - Conversion Mode="));
        ansi->print(sensor->getConvMode() ? F("Continuous") : F("Single"));
        ansi->print(F(" - Wires="));
        ansi->print(sensor->getWires() ? F("3") : F("2/4"));
        ansi->print(F(" - Filter="));
        ansi->println(sensor->getFilter() ? F("50Hz") : F("60Hz"));
        ansi->println();
    }

    /* Show data for CAN Driver */
    ansi->foreground(ansi->green);
    ansi->clearLine(ansi->entireLine);
    ansi->println(F("MCP25625 CAN Driver: "));
    ansi->normal();
    ansi->print(F(" Errors="));
    ansi->println(CAN0.checkError() != CAN_OK ? F("YES") : F("NO"));
    ansi->clearLine(ansi->entireLine);
    ansi->print(F(" TEC="));
    ansi->println(CAN0.errorCountTX());
    ansi->clearLine(ansi->entireLine);
    ansi->print(F(" REC="));
    ansi->println(CAN0.errorCountRX());
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

    printFooter();
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

void CLIScreenManager::logMessage(const __FlashStringHelper *message, LogType severity)
{
    uint8_t available_rows = terminal_height - 5;
    if (!ansi_enabled || last_screen != ScreenType::LOG)
        return;

    if (++logRow > available_rows)
        logRow = 1;

    ansi->gotoXY(1, 2 + logRow);
    uint8_t color = ansi->white;
    const __FlashStringHelper *level;
    switch (severity)
    {
    case LogType::INFO:
        color = ansi->cyan;
        level = F("INFO");
        break;
    case LogType::WARNING:
        color = ansi->yellow;
        level = F("WARNING");
        break;
    case LogType::ERROR:
        color = ansi->red;
        level = F("ERROR");
        break;
    case LogType::SUCCESS:
        color = ansi->green;
        level = F("SUCCESS");
        break;
    }

    // Stampa il timestamp a 10 caratteri, padding a sinistra con zeri
    char timestamp[11];
    mini_udec_padded(timestamp, (uint32_t)(millis() / 1000), 4);
    ansi->print(timestamp);
    ansi->foreground(color);
    ansi->bold();
    ansi->print(" [");
    ansi->print(level);
    ansi->print("] ");
    ansi->normal();
    ansi->println(message);
    if (logRow < available_rows)
        ansi->clearLine(ansi->entireLine);
}

// Overload per compatibilità con stringhe C normali
void CLIScreenManager::logMessage(const char *message, LogType severity)
{
    uint8_t available_rows = terminal_height - 5;
    if (!ansi_enabled || last_screen != ScreenType::LOG)
        return;

    if (++logRow > available_rows)
        logRow = 1;

    ansi->gotoXY(1, 2 + logRow);
    uint8_t color = ansi->white;
    const __FlashStringHelper *level;
    switch (severity)
    {
    case LogType::INFO:
        color = ansi->cyan;
        level = F("INFO");
        break;
    case LogType::WARNING:
        color = ansi->yellow;
        level = F("WARNING");
        break;
    case LogType::ERROR:
        color = ansi->red;
        level = F("ERROR");
        break;
    case LogType::SUCCESS:
        color = ansi->green;
        level = F("SUCCESS");
        break;
    }

    // Stampa il timestamp a 10 caratteri, padding a sinistra con zeri
    char timestamp[11];
    mini_udec_padded(timestamp, (uint32_t)(millis() / 1000), 4);
    ansi->print(timestamp);
    ansi->foreground(color);
    ansi->bold();
    ansi->print(" [");
    ansi->print(level);
    ansi->print("] ");
    ansi->normal();
    ansi->println(message);
    if (logRow < available_rows)
        ansi->clearLine(ansi->entireLine);
}

void CLIScreenManager::logInfo(const char *message) { logMessage(message, LogType::INFO); }
void CLIScreenManager::logWarning(const char *message) { logMessage(message, LogType::WARNING); }
void CLIScreenManager::logError(const char *message) { logMessage(message, LogType::ERROR); }
void CLIScreenManager::logSuccess(const char *message) { logMessage(message, LogType::SUCCESS); }

// Metodi di utilità per la formattazione
void CLIScreenManager::printColoredText(const __FlashStringHelper *text, uint8_t color, bool newline)
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

void CLIScreenManager::printProgressBar(uint8_t progress, uint8_t width, const __FlashStringHelper *label)
{
    if (!ansi_enabled)
        return;

    if (label)
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
    ansi->println("%   ");
}

/**
 * @brief Prints a status indicator with label and status, using ANSI colors.
 * @param label Status label (const __FlashStringHelper*)
 * @param status True for OK, false for error
 * @param ok_text Text to display when status is OK (const __FlashStringHelper*)
 * @param error_text Text to display when status is error (const __FlashStringHelper*)
 * @return None
 */
void CLIScreenManager::printStatusIndicator(const __FlashStringHelper *label, bool status, const __FlashStringHelper *ok_text, const __FlashStringHelper *error_text)
{
    if (!ansi_enabled)
        return;

    // Use default values if nullptr is passed
    if (!ok_text)
        ok_text = F("OK");
    if (!error_text)
        error_text = F("ERROR");

    ansi->print(label);
    ansi->print(": ");

    if (status)
    {
        ansi->foreground(ansi->green);
        ansi->print("[");
        ansi->print(ok_text);
        ansi->print("]");
    }
    else
    {
        ansi->foreground(ansi->red);
        ansi->print("[");
        ansi->print(error_text);
        ansi->print("]");
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

void CLIScreenManager::printHeader(const __FlashStringHelper *title, uint8_t background, uint8_t foreground)
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
    uint8_t title_length = strlen_P((const char *)title);
    uint8_t padding = (terminal_width - title_length) / 2;
    for (uint8_t i = 0; i < terminal_width; i++)
    {
        if (i == padding)
        {
            ansi->print(title);
            i += title_length - 1;
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
    ansi->gotoXY(1, terminal_height - 3);
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
        printMenuFooter();
}

void CLIScreenManager::printMenuFooter()
{
    ansi->gotoXY(1, terminal_height - 1);
    ansi->print(F("Navigazione: Valori (V) | Device (D) | Errori (E) | Calibrazione (C) | Log (L)"));
}
