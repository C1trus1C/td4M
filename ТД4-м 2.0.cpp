#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <thread>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

// Инициализация консоли для Windows
void initConsole() {
#ifdef _WIN32
    // Устанавливаем кодовую страницу на русский (1251)
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    // Альтернатива - UTF-8 (65001)
    // SetConsoleCP(65001);
    // SetConsoleOutputCP(65001);
#else
    // Для Linux/macOS
    setlocale(LC_ALL, "");
#endif
}

// Cross-platform file dialog functions
class FileDialog {
public:
    static std::string openFileDialog(const char* title, const char* filter) {
#ifdef _WIN32
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.lpstrTitle = title;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            return std::string(ofn.lpstrFile);
        }
        return "";
#else
        std::string command = "zenity --file-selection --title=\"";
        command += title;
        command += "\" 2>/dev/null";

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::cout << "Oshibka: ne udalos' otkryt' dialog vybora faila." << std::endl;
            return "";
        }

        char buffer[1024];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result = buffer;
            if (!result.empty() && result[result.length() - 1] == '\n') {
                result.erase(result.length() - 1);
            }
        }
        pclose(pipe);
        return result;
#endif
    }

    static std::string saveFileDialog(const char* title, const char* filter) {
#ifdef _WIN32
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.lpstrTitle = title;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

        if (GetSaveFileNameA(&ofn) == TRUE) {
            return std::string(ofn.lpstrFile);
        }
        return "";
#else
        std::string command = "zenity --file-selection --save --title=\"";
        command += title;
        command += "\" 2>/dev/null";

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return "";
        }

        char buffer[1024];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result = buffer;
            if (!result.empty() && result[result.length() - 1] == '\n') {
                result.erase(result.length() - 1);
            }
        }
        pclose(pipe);
        return result;
#endif
    }
};

// TD4-M Emulator
class TD4Emulator {
private:
    uint8_t regA = 0;
    uint8_t regB = 0;
    uint8_t regPC = 0;
    uint8_t regIN = 0;
    uint8_t regOUT = 0;
    uint8_t regXY = 0;
    bool flagC = false;
    bool flagZ = false;

    std::vector<uint8_t> ROM;
    std::vector<uint8_t> RAM;

    std::vector<uint8_t> inputBuffer;
    int inputIndex = 0;
    std::ofstream outputFile;

    bool running = true;
    bool paused = false;
    bool autoMode = false;
    int autoClockDelay = 100;
    int inputChangeRate = 1;
    int clockCycles = 0;

    bool useInputFile = false;
    bool useOutputFile = false;
    std::string inputFilePath;
    std::string outputFilePath;

public:
    TD4Emulator() : ROM(256, 0), RAM(256, 0) {
        srand(time(0));
    }

    ~TD4Emulator() {
        if (outputFile.is_open()) {
            outputFile.close();
        }
    }

    bool loadROM(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cout << "[OSHIBKA] Ne mozno otkryt' ROM fail: " << filename << std::endl;
            return false;
        }

        file.read(reinterpret_cast<char*>(ROM.data()), 256);
        std::streamsize read = file.gcount();

        for (int i = read; i < 256; i++) {
            ROM[i] = rand() % 256;
        }

        file.close();
        std::cout << "ROM zagruzhen: " << read << " bajt iz " << filename << std::endl;
        return true;
    }

    bool loadRAM(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cout << "[OSHIBKA] Ne mozno otkryt' RAM fail: " << filename << std::endl;
            return false;
        }

        file.read(reinterpret_cast<char*>(RAM.data()), 256);
        std::streamsize read = file.gcount();

        for (int i = read; i < 256; i++) {
            RAM[i] = rand() % 256;
        }

        file.close();
        std::cout << "RAM zagruzhen: " << read << " bajt iz " << filename << std::endl;
        return true;
    }

    void initializeRAM() {
        for (int i = 0; i < 256; i++) {
            RAM[i] = rand() % 256;
        }
    }

    bool loadInputFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cout << "[OSHIBKA] Ne mozno otkryt' vkhodnoj fail: " << filename << std::endl;
            return false;
        }

        uint8_t byte;
        while (file.read(reinterpret_cast<char*>(&byte), 1)) {
            inputBuffer.push_back(byte & 0x0F);
        }

        file.close();
        useInputFile = true;
        inputFilePath = filename;
        std::cout << "Vkhodnoj fail zagruzhen: " << inputBuffer.size() << " znachenij" << std::endl;
        return true;
    }

    bool openOutputFile(const std::string& filename) {
        outputFile.open(filename, std::ios::binary);
        if (!outputFile) {
            std::cout << "[OSHIBKA] Ne mozno sozdat' vyhodnoj fail: " << filename << std::endl;
            return false;
        }

        useOutputFile = true;
        outputFilePath = filename;
        std::cout << "Vyhodnoj fail otkryt: " << filename << std::endl;
        return true;
    }

    static uint8_t parseNumber(const std::string& str) {
        if (str.empty()) return 0;

        if (str.back() == 'b' || str.back() == 'B') {
            return (uint8_t)std::stoi(str.substr(0, str.size() - 1), nullptr, 2);
        }
        else if (str.back() == 'h' || str.back() == 'H') {
            return (uint8_t)std::stoi(str.substr(0, str.size() - 1), nullptr, 16);
        }
        else {
            return (uint8_t)std::stoi(str, nullptr, 10);
        }
    }

    static std::string toBinary(uint8_t val, int bits = 8) {
        return std::bitset<8>(val).to_string().substr(8 - bits);
    }

    static std::string toHex(uint8_t val) {
        std::stringstream ss;
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)val;
        return ss.str();
    }

    uint8_t getInputValue() {
        if (useInputFile) {
            if (inputIndex < inputBuffer.size()) {
                return inputBuffer[inputIndex];
            }
            else {
                return rand() % 16;
            }
        }
        else {
            std::cout << "Vvedite vkhodnyje dannyje (dvoichnyj: 0101b, shestnadcatyj: 0Ah, desyatochnyj: 5): ";
            std::string input;
            std::cin >> input;
            return parseNumber(input) & 0x0F;
        }
    }

    void displayState() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif

        std::cout << std::endl;
        std::cout << "===== EMULQTOR TD4-M (4-bitnyj processor) =====" << std::endl;
        std::cout << std::endl;

        std::cout << "--- REGISTRI ---" << std::endl;
        std::cout << "A:   " << toBinary(regA, 4) << "  (" << toHex(regA) << ")" << std::endl;
        std::cout << "B:   " << toBinary(regB, 4) << "  (" << toHex(regB) << ")" << std::endl;
        std::cout << "PC:  " << toBinary(regPC, 8) << "  (" << toHex(regPC) << ")" << std::endl;
        std::cout << "IN:  " << toBinary(regIN, 4) << "  (" << toHex(regIN) << ")" << std::endl;
        std::cout << "OUT: " << toBinary(regOUT, 4) << "  (" << toHex(regOUT) << ")" << std::endl;
        std::cout << "XY:  " << toBinary(regXY, 8) << "  (" << toHex(regXY) << ")" << std::endl;
        std::cout << std::endl;
        std::cout << "FLAGY: C=" << (flagC ? "1" : "0") << "  Z=" << (flagZ ? "1" : "0") << std::endl;
        std::cout << std::endl;

        std::cout << "--- PROGRAMMA ---" << std::endl;
        for (int i = 0; i < 6 && regPC + i < 256; i++) {
            std::string prefix = (i == 0) ? ">" : " ";
            std::cout << prefix << "[" << toHex(regPC + i) << "] "
                << toBinary(ROM[regPC + i], 8) << " (" << toHex(ROM[regPC + i]) << ") "
                << disassemble(ROM[regPC + i]) << std::endl;
        }
        std::cout << std::endl;

        std::cout << "--- SOSTOQNIE ---" << std::endl;
        std::cout << "Rejim: " << (autoMode ? "AVTO" : "RUCHNOY") << std::endl;
        if (paused) {
            std::cout << "Status: PAUZA" << std::endl;
        }
        std::cout << "Taktov: " << clockCycles << std::endl;
        std::cout << std::endl;
    }

    std::string disassemble(uint8_t instruction) {
        uint8_t opcode = (instruction >> 4) & 0x0F;
        uint8_t immediate = instruction & 0x0F;

        std::stringstream ss;
        switch (opcode) {
        case 0x0: ss << "ADD A," << (int)immediate; break;
        case 0x1: ss << "MOV A,B"; break;
        case 0x2: ss << "IN A"; break;
        case 0x3: ss << "MOV A," << (int)immediate; break;
        case 0x4: ss << "MOV B,A"; break;
        case 0x5: ss << "ADD B," << (int)immediate; break;
        case 0x6: ss << "IN B"; break;
        case 0x7: ss << "MOV B," << (int)immediate; break;
        case 0x9: ss << "OUT B"; break;
        case 0xB: ss << "OUT " << (int)immediate; break;
        case 0xE: ss << "JNC " << (int)immediate; break;
        case 0xF: ss << "JMP " << (int)immediate; break;
        default: ss << "UNKNOWN"; break;
        }
        return ss.str();
    }

    void executeInstruction() {
        uint8_t instruction = ROM[regPC];
        uint8_t opcode = (instruction >> 4) & 0x0F;
        uint8_t immediate = instruction & 0x0F;

        uint16_t result;

        flagC = false;
        flagZ = false;

        switch (opcode) {
        case 0x0: // ADD A,Im
            result = regA + immediate;
            regA = result & 0x0F;
            flagC = (result > 0x0F);
            flagZ = (regA == 0);
            break;

        case 0x1: // MOV A,B
            regA = regB;
            flagZ = (regA == 0);
            break;

        case 0x2: // IN A
            regA = regIN;
            flagZ = (regA == 0);
            inputIndex++;
            break;

        case 0x3: // MOV A,Im
            regA = immediate;
            flagZ = (regA == 0);
            break;

        case 0x4: // MOV B,A
            regB = regA;
            flagZ = (regB == 0);
            break;

        case 0x5: // ADD B,Im
            result = regB + immediate;
            regB = result & 0x0F;
            flagC = (result > 0x0F);
            flagZ = (regB == 0);
            break;

        case 0x6: // IN B
            regB = regIN;
            flagZ = (regB == 0);
            inputIndex++;
            break;

        case 0x7: // MOV B,Im
            regB = immediate;
            flagZ = (regB == 0);
            break;

        case 0x9: // OUT B
            regOUT = regB;
            if (useOutputFile) {
                char byte = regOUT;
                outputFile.write(&byte, 1);
            }
            break;

        case 0xB: // OUT Im
            regOUT = immediate;
            if (useOutputFile) {
                char byte = regOUT;
                outputFile.write(&byte, 1);
            }
            break;

        case 0xE: // JNC
            if (!flagC) {
                regPC = immediate;
            }
            else {
                regPC++;
            }
            return;

        case 0xF: // JMP
            regPC = immediate;
            return;
        }

        regPC++;
    }


    void updateInput() {
        if (useInputFile) {
            if ((clockCycles % inputChangeRate) == 0) {
                if (inputIndex < inputBuffer.size()) {
                    regIN = inputBuffer[inputIndex];
                }
                else {
                    regIN = rand() % 16;
                }
            }
        }
    }

    void displayRAM() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        std::cout << std::endl;
        std::cout << "===== Soderzhimoe RAM (256 bajtov) =====" << std::endl;
        for (int i = 0; i < 256; i++) {
            if (i % 16 == 0) {
                std::cout << "\n[" << toHex(i) << "] ";
            }
            std::cout << toHex(RAM[i]) << " ";
        }
        std::cout << std::endl;
    }

    void editRAM() {
        std::cout << "\nVvedite adres (hex): ";
        std::string addrStr;
        std::cin >> addrStr;
        uint8_t addr = parseNumber(addrStr);

        std::cout << "Vvedite novoe znachenie (hex/bin/dec): ";
        std::string valStr;
        std::cin >> valStr;
        uint8_t val = parseNumber(valStr);

        RAM[addr] = val;
        std::cout << "RAM[" << toHex(addr) << "] = " << toHex(val) << std::endl;
    }

    void run(bool automatic) {
        autoMode = automatic;
        displayState();

        while (running) {
            if (!paused) {
                updateInput();
                executeInstruction();
                clockCycles++;

                displayState();

                if (autoMode) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(autoClockDelay));
                }
                else {
                    std::cout << "\n[ENTER] next | [m] RAM | [q] exit: ";
                    std::string cmd;
                    std::getline(std::cin, cmd);

                    if (cmd == "q") {
                        running = false;
                    }
                    else if (cmd == "m") {
                        displayRAM();
                        std::cout << "\nRedaktirovať RAM? (y/n): ";
                        std::getline(std::cin, cmd);
                        if (cmd == "y") {
                            editRAM();
                        }
                    }
                }
            }
        }
    }
};

void printUsage(const char* programName) {
    std::cout << std::endl;
    std::cout << "===== EMULQTOR TD4-M - 4-bitnyj processor =====" << std::endl;
    std::cout << std::endl;
    std::cout << "Ispolzovanie: " << programName << " [opcii]" << std::endl;
    std::cout << std::endl;
    std::cout << "Esli zapuscheno bez parametrov - otkroytsya dialogoboe okno" << std::endl;
    std::cout << std::endl;
    std::cout << "Opcii:" << std::endl;
    std::cout << "  -rom <fail>      Zagruzit' ROM" << std::endl;
    std::cout << "  -ram <fail>      Zagruzit' RAM" << std::endl;
    std::cout << "  -in <fail>       Zagruzit' vkhodnyje dannyje" << std::endl;
    std::cout << "  -out <fail>      Sohranit' vyhodnyje dannyje" << std::endl;
    std::cout << "  -mode <rejim>    Rejim: manual ili auto" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // Inicializaciya konsoli
    initConsole();

    std::cout << std::endl;
    std::cout << "===== EMULQTOR TD4-M (4-bitnyj processor) =====" << std::endl;
    std::cout << std::endl;

    TD4Emulator emulator;
    std::string romFile, ramFile, inputFile, outputFile, mode;
    bool useGUI = true;

    // Parsing argumentov
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-rom" && i + 1 < argc) {
            romFile = argv[++i];
            useGUI = false;
        }
        else if (arg == "-ram" && i + 1 < argc) {
            ramFile = argv[++i];
        }
        else if (arg == "-in" && i + 1 < argc) {
            inputFile = argv[++i];
        }
        else if (arg == "-out" && i + 1 < argc) {
            outputFile = argv[++i];
        }
        else if (arg == "-mode" && i + 1 < argc) {
            mode = argv[++i];
        }
        else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }

    // Zagruzka ROM
    if (romFile.empty()) {
        std::cout << "Vyberite ROM fail..." << std::endl;
        romFile = FileDialog::openFileDialog(
            "Vyberite ROM fail",
            "Binary Files (*.rom *.bin)\0*.rom;*.bin\0All Files (*.*)\0*.*\0"
        );

        if (romFile.empty()) {
            std::cout << "ROM fail ne vybran. Vykhod." << std::endl;
            return 1;
        }
    }

    if (!emulator.loadROM(romFile)) {
        return 1;
    }

    // Zagruzka RAM (opciono)
    if (ramFile.empty() && useGUI) {
        std::cout << "\nZagruzit' RAM? (y/n): ";
        std::string answer;
        std::getline(std::cin, answer);

        if (answer == "y" || answer == "Y") {
            std::cout << "Vyberite RAM fail..." << std::endl;
            ramFile = FileDialog::openFileDialog(
                "Vyberite RAM fail",
                "Binary Files (*.ram *.bin)\0*.ram;*.bin\0All Files (*.*)\0*.*\0"
            );
        }
    }

    if (!ramFile.empty()) {
        emulator.loadRAM(ramFile);
    }
    else {
        emulator.initializeRAM();
    }

    // Vkhodnoj fail (opciono)
    if (inputFile.empty() && useGUI) {
        std::cout << "\nZagruzit' vkhodnoj fail? (y/n): ";
        std::string answer;
        std::getline(std::cin, answer);

        if (answer == "y" || answer == "Y") {
            std::cout << "Vyberite vkhodnoj fail..." << std::endl;
            inputFile = FileDialog::openFileDialog(
                "Vyberite vkhodnoj fail",
                "Binary Files (*.bin *.dat)\0*.bin;*.dat\0All Files (*.*)\0*.*\0"
            );
        }
    }

    if (!inputFile.empty()) {
        emulator.loadInputFile(inputFile);
    }

    // Vyhodnoj fail (opciono)
    if (outputFile.empty() && useGUI) {
        std::cout << "\nSohranit' vyvod v fail? (y/n): ";
        std::string answer;
        std::getline(std::cin, answer);

        if (answer == "y" || answer == "Y") {
            std::cout << "Vyberite vyhodnoj fail..." << std::endl;
            outputFile = FileDialog::saveFileDialog(
                "Sohranit' vyhodnoj fail",
                "Binary Files (*.bin *.dat)\0*.bin;*.dat\0All Files (*.*)\0*.*\0"
            );
        }
    }

    if (!outputFile.empty()) {
        emulator.openOutputFile(outputFile);
    }

    // Vybor rejima
    if (mode.empty() && useGUI) {
        std::cout << "\n--- Vybor rejima raboty ---" << std::endl;
        std::cout << "1. Ruchnoy (manual) - vypolnenie po nazhatiyu ENTER" << std::endl;
        std::cout << "2. Avtomaticheskiy (auto) - avtomaticheskoe vypolnenie" << std::endl;
        std::cout << "Vash vybor (1/2): ";
        std::string choice;
        std::getline(std::cin, choice);

        mode = (choice == "2") ? "auto" : "manual";
    }

    bool autoMode = (mode == "auto");

    std::cout << "\n===== ZAPUSK EMULQTORA =====" << std::endl;
    std::cout << "Rejim: " << (autoMode ? "Avtomaticheskiy" : "Ruchnoy") << std::endl;
    std::cout << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Zapusk
    emulator.run(autoMode);

    std::cout << "\n===== EMULQTOR ZAVERSHEN =====" << std::endl;
    return 0;
}