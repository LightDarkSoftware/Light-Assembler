#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

const std::unordered_set<std::string> mnemonics = {"MOVE", "ADD", "SUB", "INT", "DC", "TIMES", "PUSHA", "POPA", "CLI", "STI", "HLT"};
const std::unordered_set<std::string> mnemTypes = {"B", "W", "L", "Q"};
const std::unordered_map<std::string, int> registers8 = {{"AL", 0}, {"CL", 1}, {"DL", 2}, {"BL", 3}, {"AH", 4}, {"CH", 5}, {"DH", 6}, {"BH", 7}};
const std::unordered_map<std::string, int> registers16 = {{"AX", 0}, {"CX", 1}, {"DX", 2}, {"BX", 3}, {"SP", 4}, {"BP", 5}, {"SI", 6}, {"DI", 7}};
const std::unordered_map<std::string, int> segments = {{"ES", 0}, {"CS", 1}, {"SS", 2}, {"DS", 3}, {"FS", 4}, {"GS", 5}};

std::vector<std::string> splitSentence(std::string sentence) {
    std::stringstream ss(sentence);
    std::string word;
    std::vector<std::string> words;

    while (ss >> word) {
        words.push_back(word);
    }
    return words;
}

uint64_t returnByte(const std::string& immediate) {
    if (immediate.starts_with("0x")) {
        return std::stoi(immediate.substr(2), nullptr, 16); // Hexadecimal
    } else if (immediate.starts_with("0b")) {
        return std::stoi(immediate.substr(2), nullptr, 2); // Binary
    } else {
        return std::stoi(immediate); // Decimal
    }
}

int evaluateExpression(std::string expression, size_t bufferSize) {
    size_t operatorPos = expression.find_first_of("+-", 1);

    auto parseOperand = [bufferSize](const std::string& operand) -> int {
        if (operand == ".") {
            return static_cast<int>(bufferSize);
        }
        if (operand.starts_with("$") && operand.size() > 1) {
            return returnByte(operand.substr(1));
        }
        if (!operand.empty()) {
            returnByte(operand);
        }
        std::cin.get();
        std::exit(1);
    };

    if (operatorPos == std::string::npos) {
        return parseOperand(expression);
    }
    char op = expression[operatorPos];

    std::string first = expression.substr(0, operatorPos);
    std::string second = expression.substr(operatorPos + 1);

    int firstValue = parseOperand(first);
    int secondValue = parseOperand(second);

    switch (op) {
        case '+': return firstValue + secondValue;
        case '-': return firstValue - secondValue;
        default: return 0;
    }
}

void saveToFile(const std::string& filename, const std::vector<uint8_t>& buffer) {
    std::ofstream outFile(filename, std::ios::out | std::ios::binary);

    if (!outFile) {
        std::cerr << "Failed to save file: " << filename << std::endl;
        return;
    }

    outFile.write(reinterpret_cast<const char*>(buffer.data()),
                  static_cast<std::streamsize>(buffer.size()));

    std::cout << "Sucessfully saved file to: " << filename << std::endl;
}

void handleInstruction(std::vector<std::string> words, std::string instruction, std::string instrucType, std::vector<uint8_t>& buffer) {
    std::cout << instruction << " " << instrucType << std::endl;

    if (instruction == "MOVE") {
        if (instrucType.empty()) return;
        if (!mnemTypes.count(instrucType)) return;

        std::string destination = words[1];
        std::string source = words[2];

        bool is8 = (instrucType == "B");
        bool is16 = (instrucType == "W");

        bool isImmediate = source.starts_with("$");

        std::string destinationRegister = destination.starts_with("%") ? destination.substr(1) : destination;
        std::string sourceRegister = source.starts_with("%") || source.starts_with("$") ? source.substr(1) : source;

        if (isImmediate) {
            uint32_t immediateValue = 0;
            immediateValue = returnByte(sourceRegister);

            if (is8) {
                if (registers8.contains(destinationRegister)) {
                    int registerVal = registers8.find(destinationRegister)->second;
                    buffer.push_back(0xB0 + registerVal);
                    buffer.push_back(static_cast<uint8_t>(immediateValue & 0xFF));
                } else return;
            } else if (is16) {
                if (registers16.contains(destinationRegister)) {
                    int registerVal = registers16.find(destinationRegister)->second;
                    //buffer.push_back(0x66); // Tell processor to use 16bit and not default to 32bit
                    buffer.push_back(0xB8 + registerVal);
                    buffer.push_back(static_cast<uint8_t>(immediateValue & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((immediateValue >> 8) & 0xFF));
                } else return;
            } else return;
        } else if (segments.contains(destinationRegister) && registers16.contains(sourceRegister)) {
            if (!is16) return;
            uint8_t opcode = 0x8E;
            uint8_t mod = 0b11;
            uint8_t modrm = 0;
            modrm = ((mod << 6) | (segments.find(destinationRegister)->second << 3) | (registers16.find(sourceRegister)->second & 0x07));
            buffer.push_back(opcode);
            buffer.push_back(modrm);
        } else if (segments.contains(sourceRegister) && registers16.contains(destinationRegister)) {
            if (!is16) return;
            uint8_t opcode = 0x8C;
            uint8_t mod = 0b11;
            uint8_t modrm = 0;
            modrm = ((mod << 6) | (segments.find(sourceRegister)->second << 3) | (registers16.find(destinationRegister)->second & 0x07));
            buffer.push_back(opcode);
            buffer.push_back(modrm);
        } else {
            uint8_t opcode;
            uint8_t mod = 0b11;
            uint8_t modrm = 0;

            if (is8) {
                opcode = 0x88;
                if (registers8.contains(destinationRegister) && registers8.contains(sourceRegister)) {
                    int sourceBits = registers8.find(sourceRegister)->second;
                    int destinationBits = registers8.find(destinationRegister)->second;
                    modrm = ((mod << 6) | (sourceBits << 3) | (destinationBits & 0x07));
                } else return;
            } else if (is16) {
                //buffer.push_back(0x66); // Tell processor to use 16bit and not default to 32bit
                opcode = 0x89;
                if (registers16.contains(destinationRegister) && registers16.contains(sourceRegister)) {
                    int sourceBits = registers16.find(sourceRegister)->second;
                    int destinationBits = registers16.find(destinationRegister)->second;
                    modrm = ((mod << 6) | (sourceBits << 3) | (destinationBits & 0x07));
                } else return;
            } else return;

            buffer.push_back(opcode);
            buffer.push_back(modrm);
        }
    }

    if (instruction == "ADD" || instruction == "SUB") {
        if (instrucType.empty()) return;
        if (!mnemTypes.count(instrucType)) return;

        std::string destination = words[1];
        std::string source = words[2];

        bool is8 = (instrucType == "B");
        bool is16 = (instrucType == "W");

        bool isImmediate = source.starts_with("$");

        std::string destinationRegister = destination.starts_with("%") ? destination.substr(1) : destination;
        std::string sourceRegister = source.starts_with("%") || source.starts_with("$") ? source.substr(1) : source;

        if (isImmediate) {
            if (registers8.contains(destinationRegister) || registers16.contains(destinationRegister)) {
                if (!is8 && !is16) return;
                int value = returnByte(sourceRegister);

                if (destinationRegister == "AX" || destinationRegister == "AL") {
                    uint8_t accOpcode;
                    if (instruction == "ADD") {
                        accOpcode = (is16) ? 0x05 : 0x04;
                    } else {
                        accOpcode = (is16) ? 0x2D : 0x2C;
                    }
                    buffer.push_back(accOpcode);
                    if (is16) {
                        buffer.push_back(value & 0xFF);
                        buffer.push_back((value >> 8) & 0xFF);
                    } else {
                        buffer.push_back(value & 0xFF);
                    }
                } else {
                    int destinationBits = (is16) ? registers16.find(destinationRegister)->second : registers8.find(destinationRegister)->second;
                    int extension = (instruction == "ADD") ? 0 : 5;
                    uint8_t opcode = (is16) ? 0x81 : 0x80;
                    uint8_t mod = 0b11;
                    uint8_t modrm = 0;
                    modrm = ((mod << 6) | (extension << 3) | (destinationBits & 0x07));
                    buffer.push_back(opcode);
                    buffer.push_back(modrm);
                    if (is16) {
                        buffer.push_back(value & 0xFF);
                        buffer.push_back((value >> 8) & 0xFF);
                    } else {
                        buffer.push_back(value & 0xFF);
                    }
                }
            } else return;
        } else if (registers8.contains(sourceRegister) || registers16.contains(sourceRegister)) {
            if (!registers8.contains(destinationRegister) && !registers16.contains(destinationRegister)) return;
            uint8_t opcode;
            if (!is8 && !is16) return;
            if (instruction == "ADD") {
                opcode = (is16) ? 0x01 : 0x00;
            } else {
                opcode = (is16) ? 0x29 : 0x28;
            }
            int sourceBits = (is16) ? registers16.find(sourceRegister)->second : registers8.find(sourceRegister)->second;
            int destinationBits = (is16) ? registers16.find(destinationRegister)->second : registers8.find(destinationRegister)->second;
            uint8_t mod = 0b11;
            uint8_t modrm = 0;
            modrm = ((mod << 6) | (sourceBits << 3) | (destinationBits & 0x07));
            buffer.push_back(opcode);
            buffer.push_back(modrm);
        } else return;
    }

    if (instruction == "INT") {
        if (words.size() != 2) return;
        if (!instrucType.empty()) return;
        int value = returnByte(words[1].substr(1));
        if (value == 3) {
            buffer.push_back(0xCC);
        } else {
            buffer.push_back(0xCD);
            buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        }
    }

    if (instruction == "PUSHA" || instruction == "POPA") {
        if (words.size() != 1) return;
        if (!instrucType.empty()) return;
        uint8_t opcode = (instruction == "PUSHA") ? 0x60 : 0x61;
        buffer.push_back(opcode);
    }

    if (instruction == "CLI" || instruction == "STI") {
        if (words.size() != 1) return;
        if (!instrucType.empty()) return;
        uint8_t opcode = (instruction == "CLI") ? 0xFA : 0xFB;
        buffer.push_back(opcode);
    }

    if (instruction == "HLT") {
        if (words.size() != 1) return;
        if (!instrucType.empty()) return;
        buffer.push_back(0xF4);
    }

    if (instruction == "DC") {
        if (instrucType.empty()) return;
        if (!mnemTypes.count(instrucType)) return;

        bool is8 = (instrucType == "B");
        bool is16 = (instrucType == "W");
        bool is32 = (instrucType == "L");
        bool is64 = (instrucType == "Q");

        uint64_t value = returnByte(words[1].substr(1));
        if (is8) {
            buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        } else if (is16) {
            buffer.push_back(static_cast<uint8_t>(value & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        } else if (is32) {
            buffer.push_back(static_cast<uint8_t>(value & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        } else if (is64) {
            buffer.push_back(static_cast<uint8_t>(value & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
        }
    }

    if (instruction == "TIMES") {
        if (!instrucType.empty()) return;

        auto split = words.begin() + std::min(words.size(), size_t(2));
        std::vector<std::string> instruction(words.begin(), split);
        std::vector<std::string> command(split, words.end());
        int value = evaluateExpression(instruction[1], buffer.size());

        if (command.empty()) return;

        std::stringstream ss(command[0]);
        std::string _instruction;
        std::string _instrucType = "";

        if (std::getline(ss, _instruction, '.')) {
            std::getline(ss, _instrucType);
        }

        for (int i = 0; i < value; ++i) {
            handleInstruction(command, _instruction, _instrucType, buffer);
        }
    }
}

void compileAssembly(const std::string& filename) {
    std::ifstream inFile(filename);
    std::vector<uint8_t> buffer;
    std::vector<std::string> lines;
    std::string line;

    if (!inFile) {
        std::cerr << "Failed to read file: " << filename << std::endl;
        return;
    }

    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }

    for (const auto& line : lines) {
        if (line.empty() || line.starts_with("#")) continue;

        std::string newLine = line;
        size_t commentPos = newLine.find('#');
        if (commentPos != std::string::npos) {
            newLine = newLine.substr(0, commentPos);
        }

        std::vector<std::string> words = splitSentence(newLine);
        std::stringstream ss(words[0]);
        std::string instruction;
        std::string instrucType = "";

        if (std::getline(ss, instruction, '.')) {
            std::getline(ss, instrucType);
        }

        if (mnemonics.count(instruction)) {
            handleInstruction(words, instruction, instrucType, buffer);
        } else {
            std::cout << "Cannot compile, instruction: " << instruction << " not found." << std::endl;
            return;
        }
    }
    saveToFile("test.bin", buffer);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "No file" << std::endl;
        return 1;
    }

    std::string filePath = argv[1];
    compileAssembly(filePath);

    return 0;
}