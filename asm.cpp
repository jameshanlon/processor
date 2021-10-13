#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <boost/format.hpp>

#include "Instructions.hpp"

// With inspiration from the LLVM Kaleidoscope tutorial.
// https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl02.html

// EBNF grammar:
//
// program        := { <label> | <data> | <instruction> | <func> | <proc> }
// label          := <alpha> <natural-number>
// data           := <data> <integer-number>
// func           := "FUNC" <identifier>
// proc           := "PROC" <identifier>
// instruction    := <opcode> <number>
//                 | <opcode> <label>
//                 | "OPR" <opcode>
// operand        := <number>
//                 | <label>
// opcode         := "LDAM" | "LDBM" | "STAM" | "LDAC" | "LDBC" | "LDAP"
//                 | "LDAI" | "LDBI | "STAI" | "BR" | "BRZ" | "BRN" | "BRB"
//                 | "SVC" | "ADD" | "SUB
// identifier     := <alpha> { <aplha> | <digit> | '_' }
// alpha          := 'a' | 'b' | ... | 'x' | 'A' | 'B' | ... | 'X'
// digit-not-zero := '1' | '2' | ... | '9'
// digit          := '0' | <digit-not-zero>
// natural-number := <digit-not-zero> { <digit> }
// integer-number := '0' | [ '-' ] <natural-number>
//
// Comments start with '#' and continue to the end of the line.

//===---------------------------------------------------------------------===//
// Lexer
//===---------------------------------------------------------------------===//

enum class Token {
  NUMBER,
  MINUS,
  DATA,
  PROC,
  FUNC,
  LDAM,
  LDBM,
  STAM,
  LDAC,
  LDBC,
  LDAP,
  LDAI,
  LDBI,
  STAI,
  BR,
  BRZ,
  BRN,
  BRB,
  SVC,
  ADD,
  SUB,
  OPR,
  IDENTIFIER,
  NONE,
  END_OF_FILE
};

static const char *tokenEnumStr(Token token) {
  switch (token) {
  case Token::NUMBER:      return "NUMBER";
  case Token::MINUS:       return "MINUS";
  case Token::DATA:        return "DATA";
  case Token::PROC:        return "PROC";
  case Token::FUNC:        return "FUNC";
  case Token::LDAM:        return "LDAM";
  case Token::LDBM:        return "LDBM";
  case Token::STAM:        return "STAM";
  case Token::LDAC:        return "LDAC";
  case Token::LDBC:        return "LDBC";
  case Token::LDAP:        return "LDAP";
  case Token::LDAI:        return "LDAI";
  case Token::LDBI:        return "LDBI";
  case Token::STAI:        return "STAI";
  case Token::BR:          return "BR";
  case Token::BRZ:         return "BRZ";
  case Token::BRN:         return "BRN";
  case Token::BRB:         return "BRB";
  case Token::SVC:         return "SVC";
  case Token::ADD:         return "ADD";
  case Token::SUB:         return "SUB";
  case Token::OPR:         return "OPR";
  case Token::IDENTIFIER:  return "IDENTIFIER";
  case Token::NONE:        return "NONE";
  case Token::END_OF_FILE: return "END_OF_FILE";
  default:
    throw std::runtime_error(std::string("unexpected token: ")+std::to_string(static_cast<int>(token)));
  }
};

static Instr tokenToInstr(Token token) {
  switch (token) {
  case Token::LDAM: return Instr::LDAM;
  case Token::LDBM: return Instr::LDBM;
  case Token::STAM: return Instr::STAM;
  case Token::LDAC: return Instr::LDAC;
  case Token::LDBC: return Instr::LDBC;
  case Token::LDAP: return Instr::LDAP;
  case Token::LDAI: return Instr::LDAI;
  case Token::LDBI: return Instr::LDBI;
  case Token::STAI: return Instr::STAI;
  case Token::BR:   return Instr::BR;
  case Token::BRZ:  return Instr::BRZ;
  case Token::BRN:  return Instr::BRN;
  case Token::OPR:  return Instr::OPR;
  default:
    throw std::runtime_error(std::string("unexpected instrucion token: ")+tokenEnumStr(token));
  }
}

static OprInstr tokenToOprInstr(Token token) {
  switch (token) {
  case Token::BRB: return OprInstr::BRB;
  case Token::SVC: return OprInstr::SVC;
  case Token::ADD: return OprInstr::ADD;
  case Token::SUB: return OprInstr::SUB;
  default:
    throw std::runtime_error(std::string("unexpected operand instrucion token: ")+tokenEnumStr(token));
  }
}

static int instrToInstrOpc(Instr instr) {
  return static_cast<int>(instr);
}

static int tokenToInstrOpc(Token token) {
  return static_cast<int>(tokenToInstr(token));
}

static int tokenToOprInstrOpc(Token token) {
  return static_cast<int>(tokenToOprInstr(token));
}

class Table {
  std::map<std::string, Token> table;

public:
  void insert(const std::string &name, const Token token) {
    table.insert(std::make_pair(name, token));
  }

  /// Lookup a token type by identifier.
  Token lookup(const std::string &name) {
    auto it = table.find(name);
    if(it != table.end()) {
      return it->second;
    }
    table.insert(std::make_pair(name, Token::IDENTIFIER));
    return Token::IDENTIFIER;
  }
};

class Lexer {

  Table         table;
  std::ifstream file;
  char          lastChar;
  std::string   identifier;
  unsigned      value;
  Token         lastToken;
  size_t        currentLine;

  void declareKeywords() {
    table.insert("ADD",  Token::ADD);
    table.insert("BRN",  Token::BRN);
    table.insert("BR",   Token::BR);
    table.insert("BRB",  Token::BRB);
    table.insert("BRZ",  Token::BRZ);
    table.insert("DATA", Token::DATA);
    table.insert("FUNC", Token::FUNC);
    table.insert("LDAC", Token::LDAC);
    table.insert("LDAI", Token::LDAI);
    table.insert("LDAM", Token::LDAM);
    table.insert("LDAP", Token::LDAP);
    table.insert("LDBC", Token::LDBC);
    table.insert("LDBI", Token::LDBI);
    table.insert("LDBM", Token::LDBM);
    table.insert("OPR",  Token::OPR);
    table.insert("PROC", Token::PROC);
    table.insert("STAI", Token::STAI);
    table.insert("STAM", Token::STAM);
    table.insert("SUB",  Token::SUB);
    table.insert("SVC",  Token::SVC);
  }

  int readChar() {
    file.get(lastChar);
    //std::cout << lastChar;
    if (file.eof()) {
      lastChar = EOF;
    }
    return lastChar;
  }

  Token readToken() {
    // Skip whitespace.
    while (std::isspace(lastChar)) {
      if (lastChar == '\n') {
        currentLine++;
      }
      readChar();
    }
    // Comment.
    if (lastChar == '#') {
      do {
        readChar();
      } while (lastChar != EOF && lastChar != '\n');
      if (lastChar == '\n') {
        currentLine++;
      }
      return readToken();
    }
    // Identifier.
    if (std::isalpha(lastChar)) {
      identifier = std::string(1, lastChar);
      while (std::isalnum(readChar()) || lastChar == '_') {
        identifier += lastChar;
      }
      return table.lookup(identifier);
    }
    // Number.
    if (std::isdigit(lastChar)) {
      std::string number(1, lastChar);
      while (std::isdigit(readChar())) {
        number += lastChar;
      }
      value = std::strtoul(number.c_str(), nullptr, 10);
      return Token::NUMBER;
    }
    // Symbols.
    if (lastChar == '-') {
      readChar();
      return Token::MINUS;
    }
    // End of file.
    if (lastChar == EOF) {
      file.close();
      return Token::END_OF_FILE;
    }
    readChar();
    return Token::NONE;
  }

public:

  Lexer() : currentLine(0) {
    declareKeywords();
  }

  Token getNextToken() {
    return lastToken = readToken();
  }

  void openFile(const char *filename) {
    file.open(filename, std::ifstream::in);
    if (!file.is_open()) {
      throw std::runtime_error("could not open file");
    }
    readChar();
  }

  const std::string &getIdentifier() const { return identifier; }
  unsigned getNumber() const { return value; }
  Token getLastToken() const { return lastToken; }
  size_t getLine() const { return currentLine; }
};

//===---------------------------------------------------------------------===//
// Functions for determining instruction encoding sizes.
//===---------------------------------------------------------------------===//

/// Return the number of 4-bit immediates required to represent the value.
static size_t numNibbles(int value) {
  if (value == 0) {
    return 1;
  }
  if (value < 0) {
    value = ~value;
  }
  size_t n = 1;
  while (value >= 16) {
    value >>= 4;
    n++;
  }
  return n;
}

/// Return the length of an instruction that has a relative label reference.
/// The length of the encoding depends on the distance to the label, which in
/// turn depends on the length of the instruction. Calculate the value by
/// increasing the length until they match.
static int instrLen(int labelOffset, int byteOffset) {
  int length = 1;
  while (length < numNibbles(labelOffset - byteOffset - length)) {
    length++;
  }
  return length;
}

//===---------------------------------------------------------------------===//
// Directive data types.
//===---------------------------------------------------------------------===//

// Base class for all directives.
struct Directive {
  Token token;
  Directive(Token token) : token(token) {}
  virtual ~Directive() = default;
  Token getToken() const { return token; }
  virtual bool operandIsLabel() const = 0;
  virtual size_t getSize() const = 0;
  virtual int getValue() const = 0;
  virtual std::string toString() const = 0;
};

class Data : public Directive {
  int value;
public:
  Data(Token token, int value) : Directive(token), value(value) {}
  bool operandIsLabel() const { return false; }
  size_t getSize() const { return 4; } // Data entries are always one word.
  int getValue() const { return value; }
  std::string toString() const {
    return "DATA " + std::to_string(value);
  }
};

class Func : public Directive {
  std::string identifier;
public:
  Func(Token token, std::string identifier) : Directive(token), identifier(identifier) {}
  bool operandIsLabel() const { return false; }
  size_t getSize() const { return 0; }
  int getValue() const { return 0; }
  std::string toString() const {
    return "FUNC " + identifier;
  }
};

class Proc : public Directive {
  std::string identifier;
public:
  Proc(Token token, std::string identifier) : Directive(token), identifier(identifier) {}
  bool operandIsLabel() const { return false; }
  size_t getSize() const { return 0; }
  int getValue() const { return 0; }
  std::string toString() const {
    return "PROC " + identifier;
  }
};

class Label : public Directive {
  std::string label;
  int labelValue;
public:
  Label(Token token, std::string label) : Directive(token), label(label) {}
  bool operandIsLabel() const { return false; }
  size_t getSize() const { return 0; }
  /// Update the label value and return true if it was changed.
  bool setLabelValue(int newValue) {
    int oldValue = labelValue;
    labelValue = newValue;
    return oldValue != newValue;
  }
  int getValue() const { return labelValue; }
  std::string getLabel() const { return label; }
  std::string toString() const { return label; }
};

class InstrImm : public Directive {
  int immValue;
public:
  InstrImm(Token token, int immValue) : Directive(token), immValue(immValue) {}
  bool operandIsLabel() const { return false; }
  size_t getSize() const {
    return (immValue < 0 && numNibbles(immValue) == 1) ? 2 : numNibbles(immValue);
  }
  int getValue() const { return immValue; }
  std::string toString() const {
    return std::string(tokenEnumStr(token)) + " " + std::to_string(immValue);
  }
};

class InstrLabel : public Directive {
  std::string label;
  int labelValue;
public:
  InstrLabel(Token token, std::string label) : Directive(token), label(label) {}
  void setLabelValue(int newValue) { labelValue = newValue; }
  bool operandIsLabel() const { return true; }
  size_t getSize() const {
    return (labelValue < 0 && numNibbles(labelValue) == 1) ? 2 : numNibbles(labelValue);
  }
  int getValue() const { return labelValue; }
  std::string getLabel() const { return label; }
  std::string toString() const {
    return std::string(tokenEnumStr(token)) + " " + label + " (" + std::to_string(labelValue) + ")";
  }
};

class InstrOp : public Directive {
  Token opcode;
public:
  InstrOp(Token token, Token opcode) : Directive(token), opcode(opcode) {
    if (opcode != Token::BRB &&
        opcode != Token::ADD &&
        opcode != Token::SUB &&
        opcode != Token::SVC) {
      throw std::runtime_error(std::string("unexpected operand to OPR ")+tokenEnumStr(opcode));
    }
  }
  bool operandIsLabel() const { return false; }
  size_t getSize() const { return 1; }
  int getValue() const { return tokenToOprInstrOpc(opcode); }
  std::string toString() const {
    return std::string("OPR ") + tokenEnumStr(opcode);
  }
};

//===---------------------------------------------------------------------===//
// Parser
//===---------------------------------------------------------------------===//

class Parser {
  Lexer &lexer;

  void expectLast(Token token) const {
    if (token != lexer.getLastToken()) {
      throw std::runtime_error(std::string("expected ")+tokenEnumStr(token));
    }
  }

  void expectNext(Token token) const {
    lexer.getNextToken();
    expectLast(token);
  }

  int parseInteger() {
    if (lexer.getLastToken() == Token::MINUS) {
       expectNext(Token::NUMBER);
       return -lexer.getNumber();
    }
    expectLast(Token::NUMBER);
    return lexer.getNumber();
  }

  std::string parseIdentifier() {
    lexer.getNextToken();
    return lexer.getIdentifier();
  }

  std::unique_ptr<Directive> parseDirective() {
    switch (lexer.getLastToken()) {
      case Token::DATA:
        lexer.getNextToken();
        return std::make_unique<Data>(Token::DATA, parseInteger());
      case Token::FUNC:
        return std::make_unique<Func>(Token::FUNC, parseIdentifier());
      case Token::PROC:
        return std::make_unique<Proc>(Token::PROC, parseIdentifier());
      case Token::IDENTIFIER:
        return std::make_unique<Label>(Token::IDENTIFIER, lexer.getIdentifier());
      case Token::OPR:
        return std::make_unique<InstrOp>(Token::OPR, lexer.getNextToken());
      case Token::LDAM:
      case Token::LDBM:
      case Token::STAM:
      case Token::LDAC:
      case Token::LDBC:
      case Token::LDAI:
      case Token::LDBI:
      case Token::STAI:
      case Token::LDAP:
      case Token::BRN:
      case Token::BR:
      case Token::BRZ: {
        auto opcode = lexer.getLastToken();
        if (lexer.getNextToken() == Token::IDENTIFIER) {
          return std::make_unique<InstrLabel>(opcode, lexer.getIdentifier());
        } else {
          return std::make_unique<InstrImm>(opcode, parseInteger());
        }
      }
      default:
        throw std::runtime_error(std::string("unrecognised token ")+tokenEnumStr(lexer.getLastToken()));
    }
  }

public:
  Parser(Lexer &lexer) : lexer(lexer) {}

  std::vector<std::unique_ptr<Directive>> parseProgram() {
    std::vector<std::unique_ptr<Directive>> program;
    while (lexer.getNextToken() != Token::END_OF_FILE) {
      program.push_back(parseDirective());
    }
    return program;
  }
};

/// Create a map of label strings to label Directives.
static std::map<std::string, Label*>
createLabelMap(std::vector<std::unique_ptr<Directive>> &program) {
  std::map<std::string, Label*> labelMap;
  for (auto &directive : program) {
    if (directive->getToken() == Token::IDENTIFIER) {
      auto label = dynamic_cast<Label*>(directive.get());
      labelMap[label->getLabel()] = label;
    }
  }
  return labelMap;
}

/// Iteratively update label values until the program size does not change.
static void resolveLabels(std::vector<std::unique_ptr<Directive>> &program,
                          std::map<std::string, Label*> &labelMap) {
  int lastSize = -1;
  int byteOffset = 0;
  //int count = 0;
  while (lastSize != byteOffset) {
    //std::cout << "Resolving labels iteration " << count++ << "\n";
    lastSize = byteOffset;
    byteOffset = 0;
    for (auto &directive : program) {
      if (directive->getToken() == Token::DATA) {
        // Data must be on 4-byte boundaries.
        if (byteOffset & 0x3) {
          byteOffset += 4 - (byteOffset & 0x3);
        }
      }
      // Update the label value.
      if (directive->getToken() == Token::IDENTIFIER) {
        dynamic_cast<Label*>(directive.get())->setLabelValue(byteOffset);
      }
      // Update the label operand value of an instruction.
      if (directive->operandIsLabel()) {
        auto instrLabel = dynamic_cast<InstrLabel*>(directive.get());
        int labelValue = labelMap[instrLabel->getLabel()]->getValue();
        int offset = labelValue - byteOffset;
        //std::cout << "label value " << labelValue << " byteOffset " << byteOffset << " instrlen " << instrLen(labelValue, byteOffset) << "\n";
        if (offset >= 0) {
          instrLabel->setLabelValue(offset - instrLen(labelValue, byteOffset));
        } else {
          instrLabel->setLabelValue(offset - instrLen(labelValue, byteOffset));
        }
      }
      byteOffset += directive->getSize();
    }
  }
}

/// Emit the program to stdout.
static void emitProgramText(std::vector<std::unique_ptr<Directive>> &program) {
  int byteOffset = 0;
  for (auto &directive : program) {
    // Data at 4-byte alignment.
    if (directive->getToken() == Token::DATA && (byteOffset & 0x3)) {
      byteOffset += 4 - (byteOffset & 0x3);
    }
    std::cout << boost::format("%#08x %-20s (%d bytes)\n") % byteOffset % directive->toString() % directive->getSize();
    byteOffset += directive->getSize();
  }
}

/// Emit the program in binary.
static void emitProgramBin(std::vector<std::unique_ptr<Directive>> &program,
                           std::fstream &outputFile) {
  int byteOffset = 0;
  for (auto &directive : program) {
    size_t size = directive->getSize();
    // Data
    if (directive->getToken() == Token::DATA) {
      // Add padding for 4-byte data alignment.
      if (byteOffset & 0x3) {
        int paddingBytes = 4 - (byteOffset & 0x3);
        int paddingValue = 0;
        outputFile.write(reinterpret_cast<const char*>(&paddingValue), paddingBytes);
        byteOffset += paddingBytes;
      }
      auto dataDirective = dynamic_cast<Data*>(directive.get());
      auto value = dataDirective->getValue();
      outputFile.write(reinterpret_cast<const char*>(&value), size);
      byteOffset += size;
    // Instruction
    } else if (size > 0) {
      if (size > 1) {
        Instr instr = (directive->getValue() < 0) ? Instr::NFIX : Instr::PFIX;
        // Output PFIX/NFIX to extend the immediate value.
        for (size_t i=size-1; i>0; i--) {
          char instrValue = instrToInstrOpc(instr) << 4 |
                            ((directive->getValue() >> (i * 4)) & 0xF);
          outputFile.put(instrValue);
          byteOffset++;
        }
      }
      // Output the instruction
      char instrValue = (tokenToInstrOpc(directive->getToken()) & 0xF) << 4 |
                        (directive->getValue() & 0xF);
      outputFile.put(instrValue);
      byteOffset++;
    }
  }
}

//===---------------------------------------------------------------------===//
// Driver
//===---------------------------------------------------------------------===//

static void help(const char *argv[]) {
  std::cout << "Hex assembler\n\n";
  std::cout << "Usage: " << argv[0] << " file\n\n";
  std::cout << "Positional arguments:\n";
  std::cout << "  file              A source file to assemble\n\n";
  std::cout << "Optional arguments:\n";
  std::cout << "  -h,--help         Display this message\n";
  std::cout << "  --tokens          Tokenise the input only\n";
  std::cout << "  --tree            Display the syntax tree only\n";
  std::cout << "  -o,--output file  Specify a file for binary output (default a.out)\n";
}

int main(int argc, const char *argv[]) {
  Lexer lexer;
  Parser parser(lexer);

  try {

    // Handle arguments.
    bool tokensOnly = false;
    bool treeOnly = false;
    const char *filename = nullptr;
    const char *outputFilename = "a.out";
    for (unsigned i = 1; i < argc; ++i) {
      if (std::strcmp(argv[i], "-h") == 0 ||
          std::strcmp(argv[i], "--help") == 0) {
        help(argv);
        std::exit(1);
      } else if (std::strcmp(argv[i], "--tokens") == 0) {
        tokensOnly = true;
      } else if (std::strcmp(argv[i], "--tree") == 0) {
        treeOnly = true;
      } else if (std::strcmp(argv[i], "--output") == 0 ||
                 std::strcmp(argv[i], "-o") == 0) {
        outputFilename = argv[++i];
      } else if (argv[i][0] == '-') {
          throw std::runtime_error(std::string("unrecognised argument: ")+argv[i]);
      } else {
        if (!filename) {
          filename = argv[i];
        } else {
          throw std::runtime_error("cannot specify more than one file");
        }
      }
    }

    // A file must be specified.
    if (!filename) {
      help(argv);
      std::exit(1);
    }

    // Open the file.
    lexer.openFile(filename);

    // Tokenise only.
    if (tokensOnly && !treeOnly) {
      while (true) {
        switch (lexer.getNextToken()) {
          case Token::IDENTIFIER:
            std::cout << "IDENTIFIER " << lexer.getIdentifier() << "\n";
            break;
          case Token::NUMBER:
            std::cout << "NUMBER " << lexer.getNumber() << "\n";
            break;
          case Token::END_OF_FILE:
            std::cout << "EOF\n";
            std::exit(0);
          default:
            std::cout << tokenEnumStr(lexer.getLastToken()) << "\n";
            break;
        }
      }
      return 0;
    }

    // Parse the program.
    auto program = parser.parseProgram();

    // Iteratively resolve label values.
    auto labelMap = createLabelMap(program);
    resolveLabels(program, labelMap);

    // Parse and print program only.
    if (treeOnly) {
      emitProgramText(program);
      return 0;
    }

    // Emit the program binary.
    std::fstream outputFile(outputFilename, std::ios::out | std::ios::binary);
    emitProgramBin(program, outputFile);
    outputFile.close();

  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << " : " << lexer.getLine() << "\n";
    std::exit(1);
  }
  return 0;
}
