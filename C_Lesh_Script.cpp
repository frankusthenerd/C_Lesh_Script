// ============================================================================
// C-Lesh Script (Implementation)
// Programmed by Francois Lamini
// ============================================================================

#include "C_Lesh_Script.h"

Codeloader::cSimulator* simulator = NULL;

bool Source_Process();
bool Process_Keys();

// **************************************************************************
// Program Entry Point
// **************************************************************************

int main(int argc, char** argv) {
  if (argc == 2) {
    std::string program = argv[1];
    try {
      Codeloader::cConfig config("Config");
      int memory_size = config.Get_Property("memory");
      Codeloader::cMemory memory(memory_size);
      Codeloader::cCompiler compiler(program, &memory);
      int width = config.Get_Property("width");
      int height = config.Get_Property("height");
      Codeloader::cAllegro_IO allegro(program, width, height, 2, "Game");
      int prgm_start = config.Get_Property("program");
      simulator = new Codeloader::cSimulator(&memory, &allegro, prgm_start);
      allegro.Load_Resources("Resources");
      allegro.Load_Button_Names("Button_Names");
      allegro.Load_Button_Map("Buttons");
      allegro.Process_Messages(Source_Process, Process_Keys);
    }
    catch (Codeloader::cError error) {
      error.Print();
    }
    if (simulator) {
      delete simulator;
    }
  }
  else {
    std::cout << "Usage: " << argv[0] << " <program>" << std::endl;
  }
  std::cout << "Done." << std::endl;
  return 0;
}

// ****************************************************************************
// Source Processor
// ****************************************************************************

/**
 * Called when command needs to be processed.
 * @return True if the app needs to exit, false otherwise.
 */
bool Source_Process() {
  simulator->Run(20);
  return false;
}

/**
 * Called when keys are processed.
 * @return True if the app needs to exit, false otherwise.
 */
bool Process_Keys() {
  return false;
}

namespace Codeloader {

  // **************************************************************************
  // Memory Implementation
  // **************************************************************************

  /**
   * Creates a new memory module.
   * @param size The size of the memory.
   */
  cMemory::cMemory(int size) {
    this->count = size;
    this->memory = new cBlock[size];
    this->Clear();
  }

  /**
   * Frees up the memory module.
   */
  cMemory::~cMemory() {
    delete[] this->memory;
  }

  /**
   * Accesses an address of the memory.
   * @param address The address to access.
   * @return A reference to the block at the address.
   * @throws An error if the address is invalid.
   */
  cBlock& cMemory::operator[] (int address) {
    if ((address < 0) || (address >= this->count)) {
      throw cError("Invalid memory address " + Number_To_Text(address) + ".");
    }
    return this->memory[address];
  }

  /**
   * Clears out the memory.
   */
  void cMemory::Clear() {
    for (int block_index = 0; block_index < this->count; block_index++) {
      cBlock& block = this->memory[block_index];
      block.Clear();
    }
  }

  // **************************************************************************
  // Compiler Implementation
  // **************************************************************************

  /**
   * Creates a new compiler module and compiles the source.
   * @param source The source code name.
   * @param memory The memory module.
   */
  cCompiler::cCompiler(std::string source, cMemory* memory) {
    this->memory = memory;
    this->pointer = 0;
    this->Parse_Tokens(source);
    this->Preprocess();
    this->Parse_Statements();
    this->Replace_Placeholders();
  }

  /**
   * Parses tokens from a source file.
   * @param source The name of the source code.
   * @throws An error if something went wrong.
   */
  void cCompiler::Parse_Tokens(std::string source) {
    cFile source_file(source + ".clsh");
    source_file.Read();
    int line_count = source_file.Count();
    for (int line_index = 0; line_index < line_count; line_index++) {
      std::string line = source_file[line_index];
      if (line.find("import") != std::string::npos) { // Source import.
        cArray<std::string> tokens = Parse_C_Lesh_Line(line);
        if (tokens.Count() == 2) {
          std::string name = tokens[1];
          this->Parse_Tokens(name);
        }
        else {
          throw cError("Invalid import statement.");
        }
      }
      else {
        // Parse the tokens.
        cArray<std::string> tokens = Parse_C_Lesh_Line(line);
        int tok_count = tokens.Count();
        for (int tok_index = 0; tok_index < tok_count; tok_index++) {
          sToken token;
          token.line_no = line_index;
          token.source = source;
          token.token = tokens[tok_index];
          this->tokens.Add(token);
        }
      }
    }
  }

  /**
   * Parses a token from the token stack.
   * @return A token object.
   * @throws An error if there are no more tokens.
   */
  sToken cCompiler::Parse_Token() {
    sToken token = { "", 0, "" };
    if (this->tokens.Count() == 0) {
      throw cError("No more tokens to parse!");
    }
    token = this->tokens.Shift();
    return token;
  }

  /**
   * Returns a token from the stack but does not remove it.
   * @return The token.
   */
  sToken cCompiler::Peek_Token() {
    sToken token = { "", 0, "" };
    if (this->tokens.Count() > 0) {
      token = this->tokens.Peek_Front();
    }
    return token;
  }

  /**
   * Parses a keyword.
   * @param keyword The keyword to check for.
   * @throws An error if the keyword is missing.
   */
  void cCompiler::Parse_Keyword(std::string keyword) {
    sToken token = this->Parse_Token();
    if (token.token != keyword) {
      this->Generate_Parse_Error("Missing keyword " + keyword + ".", token);
    }
  }

  /**
   * Generates a parse error.
   * @param message The error message.
   * @param token The associted token.
   * @throws An error.
   */
  void cCompiler::Generate_Parse_Error(std::string message, sToken token) {
    throw cError("Error: " + message + "\nLine No: " + Number_To_Text(token.line_no) + "\nSource: " + token.source + "\nToken: " + token.token);
  }

  /**
   * Parses an expression.
   * @param command The command associated with the expression.
   * @return The index of the expression.
   * @throws An error if the expression is not valid.
   */
  int cCompiler::Parse_Expression(cBlock& command) {
    tExpression expression;
    sOperand_Operator operand = this->Parse_Operand();
    expression.push_back(operand);
    while (this->Is_Operator()) {
      sOperand_Operator oper = this->Parse_Operator();
      expression.push_back(oper);
      operand = this->Parse_Operand();
      expression.push_back(operand);
    }
    command.expressions.Add(expression);
    return (command.expressions.Count() - 1);
  }

  /**
   * Parses an operand.
   * @return The operand object.
   * @throws An error if the operand is invalid.
   */
  sOperand_Operator cCompiler::Parse_Operand() {
    sToken token = Parse_Token();
    sOperand_Operator operand;
    if (token.token.length() > 1) {
      std::string address = token.token.substr(1);
      if (token.token[0] == '#') { // Immediate addressing.
        operand.addr_mode = eADDR_IMMEDIATE;
        this->Parse_Address(address, operand);
      }
      else if (token.token[0] == '@') { // Pointer addressing.
        operand.addr_mode = eADDR_POINTER;
        this->Parse_Address(address, operand);
      }
      else if (token.token[0] == '$') { // String value.
        operand.addr_mode = eADDR_VAL_STRING;
        operand.value.Set_String(address); // No placeholder.
      }
      else { // Numeric value.
        operand.addr_mode = eADDR_VAL_NUMBER;
        this->Parse_Address(address, operand);
      }
    }
    else {
      this->Generate_Parse_Error("Invalid operand token.", token);
    }
    return operand;
  }

  /**
   * Parses an operator.
   * @return The parsed operator object.
   * @throws An error if the operator is invalid.
   */
  sOperand_Operator cCompiler::Parse_Operator() {
    sOperand_Operator oper;
    sToken token = this->Parse_Token();
    if (token.token == "+") {
      oper.oper_code = eOPER_ADD;
    }
    else if (token.token == "-") {
      oper.oper_code = eOPER_SUB;
    }
    else if (token.token == "*") {
      oper.oper_code = eOPER_MUL;
    }
    else if (token.token == "/") {
      oper.oper_code = eOPER_DIV;
    }
    else if (token.token == "rem") {
      oper.oper_code = eOPER_REM;
    }
    else if (token.token == "rand") {
      oper.oper_code = eOPER_RAND;
    }
    else if (token.token == "cos") {
      oper.oper_code = eOPER_COS;
    }
    else if (token.token == "sin") {
      oper.oper_code = eOPER_SIN;
    }
    else if (token.token == "cat") {
      oper.oper_code = eOPER_CAT;
    }
    else {
      this->Generate_Parse_Error("Invalid operator.", token);
    }
    return oper;
  }

  /**
   * Determines a token is an operator. Does not remove the token.
   * @return True if the token is an operator, false otherwise.
   */
  bool cCompiler::Is_Operator() {
    sToken token = this->Peek_Token();
    return ((token.token == "+") ||
            (token.token == "-") ||
            (token.token == "*") ||
            (token.token == "/") ||
            (token.token == "rem") ||
            (token.token == "rand") ||
            (token.token == "cos") ||
            (token.token == "sin") ||
            (token.token == "cat"));
  }

  /**
   * Parses an address. It can be an object or value address.
   * @param address The address text.
   * @param operand The associated operand.
   * @throws An error if the address is invalid.
   */
  void cCompiler::Parse_Address(std::string address, sOperand_Operator& operand) {
    cArray<std::string> parts = Parse_Sausage_Text(address, "->");
    std::string addr = "";
    if (parts.Count() == 1) {
      addr = parts[0];
    }
    else if (parts.Count() == 2) {
      addr = parts[0];
      operand.field = parts[1];
      if (operand.addr_mode == eADDR_VAL_NUMBER) {
        throw cError("Cannot have object notation with numeric value.");
      }
    }
    else {
      throw cError("Invalid address " + address + ".");
    }
    // Check if address is a number.
    try {
      operand.value.Set_Number(Text_To_Number(addr));
    }
    catch (cError error) {
      operand.placeholder = addr;
    }
  }

  /**
   * Parses a conditional.
   * @param command The associated command.
   * @throws An error if the conditional is invalid.
   */
  void cCompiler::Parse_Conditional(cBlock& command) {
    sCondition_Logic condition = this->Parse_Condition(command);
    command.conditional.Add(condition);
    while (this->Is_Logic()) {
      sCondition_Logic logic = this->Parse_Logic();
      command.conditional.Add(logic);
      condition = this->Parse_Condition(command);
      command.conditional.Add(condition);
    }
  }

  /**
   * Parses a condition.
   * @param block The associated block.
   * @return The condition operator.
   * @throws An error if the condition is invalid.
   */
  sCondition_Logic cCompiler::Parse_Condition(cBlock& block) {
    sCondition_Logic condition;
    condition.left_exp = this->Parse_Expression(block);
    sToken test = this->Parse_Token();
    if (test.token == "eq") {
      condition.test = eTEST_EQUALS;
    }
    else if (test.token == "not") {
      condition.test = eTEST_NOT;
    }
    else if (test.token == "lt") {
      condition.test = eTEST_LESS;
    }
    else if (test.token == "gt") {
      condition.test = eTEST_GREATER;
    }
    else if (test.token == "le") {
      condition.test = eTEST_LESS_OR_EQUAL;
    }
    else if (test.token == "ge") {
      condition.test = eTEST_GREATER_OR_EQUAL;
    }
    else {
      this->Generate_Parse_Error("Invalid test.", test);
    }
    condition.right_exp = this->Parse_Expression(block);
    return condition;
  }

  /**
   * Parses a logic operator.
   * @return The logic operator.
   * @throws An error if the logic operator is not valid.
   */
  sCondition_Logic cCompiler::Parse_Logic() {
    sCondition_Logic logic = { 0, 0, 0, 0 };
    sToken token = this->Parse_Token();
    if (token.token == "and") {
      logic.logic_code = eLOGIC_AND;
    }
    else if (token.token == "or") {
      logic.logic_code = eLOGIC_OR;
    }
    else {
      this->Generate_Parse_Error("Invalid logic token.", token);
    }
    return logic;
  }

  /**
   * Determines if the next token is a logic token. Does not remove it.
   * @return True if the next token is logic, false otherwise.
   */
  bool cCompiler::Is_Logic() {
    sToken token = this->Peek_Token();
    return ((token.token == "and") || (token.token == "or"));
  }

  /**
   * Parses statements.
   * @throws An error if the statement is invalid.
   */
  void cCompiler::Parse_Statements() {
    while (this->tokens.Count() > 0) {
      sToken token = this->Parse_Token();
      if (token.token == "define") {
        sToken name = this->Parse_Token();
        this->Parse_Keyword("as");
        sToken value = this->Parse_Token();
        this->symtab["[" + name.token + "]"] = Text_To_Number(value.token);
      }
      else if (token.token == "map") {
        sToken item = this->Parse_Token();
        int index = 0;
        while (item.token != "end") {
          this->symtab["[" + item.token + "]"] = index++;
          item = this->Parse_Token();
        }
      }
      else if (token.token == "label") {
        sToken name = this->Parse_Token();
        this->symtab["[" + name.token + "]"] = this->pointer;
      }
      else if (token.token == "number") {
        sToken number = this->Parse_Token();
        cBlock& block = (*this->memory)[this->pointer++];
        block.value.Set_Number(Text_To_Number(number.token));
      }
      else if (token.token == "list") {
        sToken count = this->Parse_Token();
        int item_count = Text_To_Number(count.token);
        for (int item_index = 0; item_index < item_count; item_index++) {
          cBlock& block = (*this->memory)[this->pointer++];
          block.value.Set_Number(0);
        }
      }
      else if (token.token == "object") {
        sToken property = this->Parse_Token();
        cBlock& block = (*this->memory)[this->pointer++];
        while (property.token != "end") {
          cArray<std::string> pair = Parse_Sausage_Text(property.token, "=");
          if (pair.Count() == 2) {
            std::string name = pair[0];
            std::string value = pair[1];
            block.fields[name] = cValue();
            try {
              int number = Text_To_Number(value);
              block.fields[name].Set_Number(number);
            }
            catch (cError error) { // A string.
              block.fields[name].Set_String(value);
            }
          }
          else {
            this->Generate_Parse_Error("Invalid property format.", property);
          }
          property = this->Parse_Token();
        }
      }
      else if (token.token == "{remark}") {
        sToken token = this->Parse_Token();
        while (token.token != "{end}") {
          // Do nothing.
        }
      }
      else if (token.token == "store") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_STORE;
        this->Parse_Expression(command);
        this->Parse_Keyword("at");
        this->Parse_Expression(command); // Pointer
      }
      else if (token.token == "set") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_SET;
        this->Parse_Expression(command); // Pointer
        this->Parse_Expression(command); // Field
        this->Parse_Keyword("to");
        this->Parse_Expression(command);
      }
      else if (token.token == "test") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_TEST;
        this->Parse_Conditional(command);
        this->Parse_Keyword("then");
        this->Parse_Expression(command);
        this->Parse_Keyword("otherwise");
        this->Parse_Expression(command);
      }
      else if (token.token == "call") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_CALL;
        this->Parse_Expression(command);
      }
      else if (token.token == "return") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_RETURN;
      }
      else if (token.token == "stop") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_STOP;
      }
      else if (token.token == "output") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_OUTPUT;
        this->Parse_Expression(command); // Contains the string.
        this->Parse_Keyword("at");
        this->Parse_Expression(command); // Coordinates
        this->Parse_Expression(command);
        this->Parse_Keyword("color");
        this->Parse_Expression(command); // Red
        this->Parse_Expression(command); // Green
        this->Parse_Expression(command); // Blue
      }
      else if (token.token == "draw") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_DRAW;
        this->Parse_Expression(command); // Picture name.
        this->Parse_Keyword("at");
        this->Parse_Expression(command); // Coordinates
        this->Parse_Expression(command);
        this->Parse_Expression(command); // Dimensions
        this->Parse_Expression(command);
        this->Parse_Keyword("angle");
        this->Parse_Expression(command);
        this->Parse_Keyword("flip");
        this->Parse_Expression(command);
        this->Parse_Expression(command);
      }
      else if (token.token == "refresh") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_REFRESH;
      }
      else if (token.token == "sound") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_SOUND;
        this->Parse_Expression(command); // The name of the sound.
      }
      else if (token.token == "music") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_MUSIC;
        this->Parse_Expression(command); // The name of the track.
      }
      else if (token.token == "silence") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_SILENCE;
      }
      else if (token.token == "input") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_INPUT;
        this->Parse_Expression(command); // Pointer
      }
      else if (token.token == "timeout") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_TIMEOUT;
        this->Parse_Expression(command);
      }
      else if (token.token == "color") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_COLOR;
        this->Parse_Expression(command); // Red
        this->Parse_Expression(command); // Green
        this->Parse_Expression(command); // Blue
      }
      else if (token.token == "load") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_LOAD;
        this->Parse_Expression(command); // Name
        this->Parse_Keyword("at");
        this->Parse_Expression(command); // Pointer
        this->Parse_Keyword("count");
        this->Parse_Expression(command); // Pointer
      }
      else if (token.token == "save") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_SAVE;
        this->Parse_Expression(command); // Pointer
        this->Parse_Keyword("to");
        this->Parse_Expression(command); // Name
        this->Parse_Keyword("count");
        this->Parse_Expression(command);
      }
      else if (token.token == "push") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_PUSH;
        this->Parse_Expression(command);
      }
      else if (token.token == "pop") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_POP;
        this->Parse_Expression(command); // Pointer
      }
      else if (token.token == "repeat") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_REPEAT;
        this->Parse_Expression(command);
        this->Parse_Keyword("to");
        this->Parse_Expression(command);
        this->Parse_Keyword("for");
        this->Parse_Expression(command); // Pointer
        this->Parse_Keyword("jump");
        this->Parse_Expression(command);
      }
      else if (token.token == "get-object") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_GET_OBJECT;
        this->Parse_Expression(command); // Pointer
        this->Parse_Keyword("from");
        this->Parse_Expression(command); // Pointer
        this->Parse_Expression(command); // Field
      }
      else if (token.token == "get-list") {
        cBlock& command = (*this->memory)[this->pointer++];
        command.code = eCMD_GET_LIST;
        this->Parse_Expression(command); // Pointer
        this->Parse_Keyword("from");
        this->Parse_Expression(command); // Pointer
        this->Parse_Expression(command); // Field
      }
      else {
        this->Generate_Parse_Error("Invalid statement " + token.token + ".", token);
      }
    }
  }

  /**
   * Replaces all placeholders.
   * @throws An error if a placeholder is not found.
   */
  void cCompiler::Replace_Placeholders() {
    for (int block_index = 0; block_index < this->memory->count; block_index++) {
      cBlock& block = (*this->memory)[block_index];
      int exp_count = block.expressions.Count();
      for (int exp_index = 0; exp_index < exp_count; exp_index++) {
        tExpression& expression = block.expressions[exp_index];
        int operand_count = expression.size();
        for (int operand_index = 0; operand_index < operand_count; operand_index += 2) { // Every other item is operand.
          sOperand_Operator& operand = expression[operand_index];
          if (operand.placeholder.length() > 0) {
            if (this->symtab.Does_Key_Exist(operand.placeholder)) { // Found it!
              int value = this->symtab[operand.placeholder];
              operand.value.Set_Number(value);
            }
            else {
              throw cError("Could not find placeholder " + operand.placeholder + ".");
            }
          }
        }
      }
    }
  }

  /**
   * Runs the preprocessor with default definitions.
   */
  void cCompiler::Preprocess() {
    this->symtab["[none]"] = 0;
    this->symtab["[take-no-jump]"] = -1;
    this->symtab["[true]"] = 1;
    this->symtab["[false]"] = 0;
  }

  // **************************************************************************
  // Block Implementation
  // **************************************************************************

  /**
   * Creates a new block.
   */
  cBlock::cBlock() {
    this->Clear();
  }

  void cBlock::Clear() {
    this->value.Set_Number(0);
    this->code = eCMD_NONE;
    this->fields.Clear();
  }

  // **************************************************************************
  // Simulator Implementation
  // **************************************************************************

  /**
   * Creates a new simulator.
   * @param memory The memory module reference.
   * @param io The I/O control module reference.
   * @param program The start address if the program.
   */
  cSimulator::cSimulator(cMemory* memory, cIO_Control* io, int program) {
    this->memory = memory;
    this->io = io;
    this->pointer = program;
    this->status = eSTATUS_IDLE;
  }

  /**
   * Runs the simulator.
   * @param timeout The amount of milliseconds run the program for.
   */
  void cSimulator::Run(int timeout) {
    auto start = std::chrono::system_clock::now();
    while (this->status == eSTATUS_RUNNING) {
      auto end = std::chrono::system_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      if (diff.count() < timeout) {
        cBlock& command = (*this->memory)[this->pointer++];
        this->Command_Processor(command);
      }
      else {
        break;
      }
    }
  }

  /**
   * Runs the command processor.
   * @param command The command to process.
   * @throws An error if the command is invalid.
   */
  void cSimulator::Command_Processor(cBlock& command) {
    switch (command.code) {
      case eCMD_NONE: {
        break; // Do nothing.
      }
      case eCMD_STORE: {
        cValue result = this->Eval_Expression(command, 0);
        cValue pointer = this->Eval_Expression(command, 1);
        cBlock& block = (*this->memory)[pointer.number];
        block.value = result;
        break;
      }
      case eCMD_SET: {
        cValue pointer = this->Eval_Expression(command, 0); // Pointer
        cValue field = this->Eval_Expression(command, 1); // Field
        cValue value = this->Eval_Expression(command, 2); // Value
        cBlock& block = (*this->memory)[pointer.number];
        block.fields[field.string] = value;
        break;
      }
      case eCMD_TEST: {
        int result = this->Eval_Conditional(command);
        cValue passed_address = this->Eval_Expression(command, command.expressions.Count() - 2);
        cValue failed_address = this->Eval_Expression(command, command.expressions.Count() - 1);
        if (result) {
          if (passed_address.number != TAKE_NO_JUMP) {
            this->pointer = passed_address.number;
          }
        }
        else {
          if (failed_address.number != TAKE_NO_JUMP) {
            this->pointer = failed_address.number;
          }
        }
        break;
      }
      case eCMD_CALL: {
        cValue jump_address = this->Eval_Expression(command, 0);
        this->stack.Push(this->pointer); // Save next command address.
        this->pointer = jump_address.number;
        break;
      }
      case eCMD_RETURN: {
        this->pointer = this->stack.Pop();
        break;
      }
      case eCMD_STOP: {
        this->status = eSTATUS_DONE;
        break;
      }
      case eCMD_OUTPUT: {
        cValue string = this->Eval_Expression(command, 0);
        cValue x = this->Eval_Expression(command, 1);
        cValue y = this->Eval_Expression(command, 2);
        cValue red = this->Eval_Expression(command, 3);
        cValue green = this->Eval_Expression(command, 4);
        cValue blue = this->Eval_Expression(command, 5);
        this->io->Output_Text(string.string, x.number, y.number, red.number, green.number, blue.number);
        break;
      }
      case eCMD_DRAW: {
        cValue name = this->Eval_Expression(command, 0);
        cValue x = this->Eval_Expression(command, 1);
        cValue y = this->Eval_Expression(command, 2);
        cValue width = this->Eval_Expression(command, 3);
        cValue height = this->Eval_Expression(command, 4);
        cValue angle = this->Eval_Expression(command, 5);
        cValue flip_x = this->Eval_Expression(command, 6);
        cValue flip_y = this->Eval_Expression(command, 7);
        this->io->Draw_Image(name.string, x.number, y.number, width.number, height.number, angle.number, flip_x.number, flip_y.number);
        break;
      }
      case eCMD_REFRESH: {
        this->io->Refresh();
        break;
      }
      case eCMD_SOUND: {
        cValue name = this->Eval_Expression(command, 0);
        this->io->Play_Sound(name.string);
        break;
      }
      case eCMD_MUSIC: {
        cValue name = this->Eval_Expression(command, 0);
        this->io->Play_Music(name.string);
        break;
      }
      case eCMD_SILENCE: {
        this->io->Silence();
        break;
      }
      case eCMD_INPUT: {
        cValue pointer = this->Eval_Expression(command, 0);
        cBlock& block = (*this->memory)[pointer.number];
        block.value.Set_Number(this->io->Read_Signal().code);
        break;
      }
      case eCMD_TIMEOUT: {
        cValue timeout = this->Eval_Expression(command, 0);
        this->io->Timeout(timeout.number);
        break;
      }
      case eCMD_COLOR: {
        cValue red = this->Eval_Expression(command, 0);
        cValue green = this->Eval_Expression(command, 1);
        cValue blue = this->Eval_Expression(command, 2);
        this->io->Color(red.number, green.number, blue.number);
        break;
      }
      case eCMD_LOAD: {
        cValue name = this->Eval_Expression(command, 0);
        cValue address = this->Eval_Expression(command, 1);
        cValue count = this->Eval_Expression(command, 2);
        cBlock& block = (*this->memory)[address.number];
        block.value.Set_Number(this->Load(name.string, this->memory, address.number));
        break;
      }
      case eCMD_SAVE: {
        cValue name = this->Eval_Expression(command, 0);
        cValue address = this->Eval_Expression(command, 1);
        cValue count = this->Eval_Expression(command, 2);
        this->Save(name.string, this->memory, address.number, count.number);
        break;
      }
      case eCMD_PUSH: {
        cValue result = this->Eval_Expression(command, 0);
        this->stack.Push(result.number);
        break;
      }
      case eCMD_POP: {
        cValue pointer = this->Eval_Expression(command, 0);
        cBlock& block = (*this->memory)[pointer.number];
        block.value.Set_Number(this->stack.Pop());
        break;
      }
      case eCMD_REPEAT: {
        cValue lower = this->Eval_Expression(command, 0);
        cValue upper = this->Eval_Expression(command, 1);
        cValue pointer = this->Eval_Expression(command, 2);
        cValue jump_address = this->Eval_Expression(command, 3);
        cBlock& var = (*this->memory)[pointer.number];
        if ((var.value.number < lower.number) || (var.value.number > upper.number)) { // Reset variable if out of bounds.
          var.value.Set_Number(lower.number++);
          this->pointer = jump_address.number; // Jump to loop location.
        }
        else {
          var.value.Set_Number(var.value.number++);
          if (var.value.number <= upper.number) {
            this->pointer = jump_address.number; // Jump to loop location.
          }
        }
        break;
      }
      case eCMD_GET_OBJECT: {
        cValue pointer = this->Eval_Expression(command, 0);
        cValue object = this->Eval_Expression(command, 1);
        cValue field = this->Eval_Expression(command, 2);
        cBlock& source = (*this->memory)[object.number];
        cBlock& dest = (*this->memory)[pointer.number];
        dest.fields.Clear();
        cArray<std::string> objects = Parse_Sausage_Text(source.fields[field.string].string, "|");
        int obj_count = objects.Count();
        for (int obj_index = 0; obj_index < obj_count; obj_index++) {
          cArray<std::string> properties = Parse_Sausage_Text(objects[obj_index], ";");
          int prop_count = properties.Count();
          for (int prop_index = 0; prop_index < prop_count; prop_index++) {
            cArray<std::string> pair = Parse_Sausage_Text(properties[prop_index], ":");
            if (pair.Count() == 2) {
              std::string name = pair[0];
              std::string value = pair[1];
              dest.fields[name] = cValue();
              try {
                int number = Text_To_Number(value);
                dest.fields[name].Set_Number(number);
              }
              catch (cError error) {
                dest.fields[name].Set_String(value);
              }
            }
            else {
              this->Generate_Execution_Error("Sub object property is invalid.", command);
            }
          }
        }
        break;
      }
      case eCMD_GET_LIST: {
        cValue pointer = this->Eval_Expression(command, 0);
        cValue object = this->Eval_Expression(command, 1);
        cValue field = this->Eval_Expression(command, 2);
        cBlock& source = (*this->memory)[object.number];
        cBlock& dest = (*this->memory)[pointer.number];
        cArray<std::string> items = Parse_Sausage_Text(source.fields[field.string].string, ",");
        int item_count = items.Count();
        for (int item_index = 0; item_index < item_count; item_index++) {
          cBlock& item = (*this->memory)[pointer.number + item_index];
          try {
            int number = Text_To_Number(items[item_index]);
            item.value.Set_Number(number);
          }
          catch (cError error) {
            item.value.Set_String(items[item_index]);
          }
        }
        break;
      }
      default: {
        this->Generate_Execution_Error("Invalid command.", command);
      }
    }
  }

  /**
   * Evaluates an operand.
   * @param operand The operand object.
   * @return The value from the operand.
   * @throws An error if something went wrong.
   */
  cValue cSimulator::Eval_Operand(sOperand_Operator& operand) {
    cValue value;
    switch (operand.addr_mode) {
      case eADDR_VAL_NUMBER: {
        value.Set_Number(operand.value.number);
        break;
      }
      case eADDR_VAL_STRING: {
        value.Set_String(operand.value.string);
        break;
      }
      case eADDR_IMMEDIATE: {
        cBlock& block = (*this->memory)[operand.value.number];
        // We need to determine if the value comes from an object or value.
        if (operand.field.length() > 0) {
          if (block.fields.Does_Key_Exist(operand.field)) {
            value = block.fields[operand.field];
          }
          else {
            throw cError("Could not find field " + operand.field + ".");
          }
        }
        else { // Value read.
          value = block.value;
        }
        break;
      }
      case eADDR_POINTER: {
        cBlock& pointer = (*this->memory)[operand.value.number];
        cBlock& block = (*this->memory)[pointer.value.number];
        // We need to determine if the value comes from an object or value.
        if (operand.field.length() > 0) {
          if (block.fields.Does_Key_Exist(operand.field)) {
            value = block.fields[operand.field];
          }
          else {
            throw cError("Could not find field " + operand.field + ".");
          }
        }
        else { // Value read.
          value = block.value;
        }
        break;
      }
      default: {
        throw cError("Invalid address mode " + Number_To_Text(operand.addr_mode) + ".");
      }
    }
    return value;
  }

  /**
   * Evalulates an expression.
   * @param command The associated command.
   * @param index The expression index.
   * @return The value from the expression evaluation.
   * @throws An error if something went wrong.
   */
  cValue cSimulator::Eval_Expression(cBlock& command, int index) {
    cValue value;
    if ((index < 0) || (index >= command.expressions.Count())) {
      this->Generate_Execution_Error("Expression does not exist at index " + Number_To_Text(index) + ".", command);
    }
    tExpression& expression = command.expressions[index];
    if (expression.size() > 0) {
      value = this->Eval_Operand(expression[0]); // Assign initial value.
      // Evaluate operators.
      int oper_count = expression.size();
      for (int oper_index = 1; oper_index < oper_count; oper_index += 2) {
        sOperand_Operator& oper = expression[oper_index];
        sOperand_Operator& operand = expression[oper_index + 1];
        cValue operand_value = this->Eval_Operand(operand);
        switch (oper.oper_code) {
          case eOPER_ADD: {
            value.Set_Number(value.number + operand_value.number);
            break;
          }
          case eOPER_SUB: {
            value.Set_Number(value.number - operand_value.number);
            break;
          }
          case eOPER_MUL: {
            value.Set_Number(value.number * operand_value.number);
            break;
          }
          case eOPER_DIV: {
            if (operand_value.number == 0) {
              value.Set_Number(value.number);
            }
            else {
              value.Set_Number(value.number / operand_value.number);
            }
            break;
          }
          case eOPER_REM: {
            value.Set_Number(value.number % operand_value.number);
            break;
          }
          case eOPER_RAND: {
            value.Set_Number(this->io->Get_Random_Number(value.number, operand_value.number));
            break;
          }
          case eOPER_COS: {
            value.Set_Number((int)((double)value.number * std::cos((double)operand_value.number * 3.14 / 180.0)));
            break;
          }
          case eOPER_SIN: {
            value.Set_Number((int)((double)value.number * std::sin((double)operand_value.number * 3.14 / 180.0)));
            break;
          }
          case eOPER_CAT: {
            if (value.type == eVALUE_NUMBER) {
              value.Convert_To_String(); // Make sure we have a string.
            }
            if (value.type == eVALUE_NUMBER) {
              operand_value.Convert_To_String();
            }
            value.Set_String(value.string + operand_value.string);
            break;
          }
          default: {
            this->Generate_Execution_Error("Invalid operator " + Number_To_Text(oper.oper_code) + ".", command);
          }
        }
      }
    }
    else {
      this->Generate_Execution_Error("Empty expression.", command);
    }
    return value;
  }

  /**
   * Evaluates a condition.
   * @param command The associated command.
   * @param condition The condition object.
   * @return True if the condition passed, false otherwise.
   * @throws An error if something went wrong.
   */
  bool cSimulator::Eval_Condition(cBlock& command, sCondition_Logic& condition) {
    bool result = false;
    cValue left_val = this->Eval_Expression(command, condition.left_exp);
    cValue right_val = this->Eval_Expression(command, condition.right_exp);
    switch (condition.test) {
      case eTEST_EQUALS: {
        if (left_val.type == eVALUE_NUMBER) {
          result = (left_val.number == right_val.number);
        }
        else if (left_val.type == eVALUE_STRING) {
          result = (left_val.string == right_val.string);
        }
        break;
      }
      case eTEST_NOT: {
        if (left_val.type == eVALUE_NUMBER) {
          result = (left_val.number != right_val.number);
        }
        else if (left_val.type == eVALUE_STRING) {
          result = (left_val.string != right_val.string);
        }
        break;
      }
      case eTEST_LESS: {
        result = (left_val.number < right_val.number);
        break;
      }
      case eTEST_GREATER: {
        result = (left_val.number > right_val.number);
        break;
      }
      case eTEST_LESS_OR_EQUAL: {
        result = (left_val.number <= right_val.number);
        break;
      }
      case eTEST_GREATER_OR_EQUAL: {
        result = (left_val.number >= right_val.number);
        break;
      }
      default: {
        this->Generate_Execution_Error("Invalid test " + Number_To_Text(condition.test) + ".", command);
      }
    }
    return result;
  }

  /**
   * Evaluates a conditional.
   * @param command The associated command.
   * @return A zero or non-zero number representing the result.
   */
  int cSimulator::Eval_Conditional(cBlock& command) {
    int result = 0;
    if (command.conditional.Count() > 0) {
      result = this->Eval_Condition(command, command.conditional[0]);
      // Evaluate logic operators.
      int oper_count = command.conditional.Count();
      for (int oper_index = 1; oper_index < oper_count; oper_index += 2) {
        sCondition_Logic& logic = command.conditional[oper_index];
        sCondition_Logic& condition = command.conditional[oper_index + 1];
        int cond_result = this->Eval_Condition(command, condition);
        switch (logic.logic_code) {
          case eLOGIC_AND: {
            result *= cond_result; // Multiplication is like AND logic gate.
            break;
          }
          case eLOGIC_OR: {
            result += cond_result; // Addition is like OR logic gate.
            break;
          }
          default: {
            this->Generate_Execution_Error("Invalid logic code " + Number_To_Text(logic.logic_code) + ".", command);
          }
        }
      }
    }
    else {
      this->Generate_Execution_Error("No conditional present.", command);
    }
    return result;
  }

  /**
   * Generates an execution error.
   * @param message The error message.
   * @param command The associated command.
   * @throws An error.
   */
  void cSimulator::Generate_Execution_Error(std::string message, cBlock& command) {
    throw cError("Error: " + message + "\nCode: " + Number_To_Text(command.code) + "\nPointer: " + Number_To_Text(this->pointer));
  }

  /**
   * Loads a file into memory. The file consists of objects.
   * @param name The name of the file.
   * @param memory The memory module.
   * @param address The address to load the file at.
   * @return The number of items loaded.
   * @throws An error if the file could not be loaded.
   */
  int cSimulator::Load(std::string name, cMemory* memory, int address) {
    std::ifstream file(name);
    int count = 0;
    if (file) {
      while (!file.eof()) {
        std::string line;
        std::getline(file, line);
        if (file.good()) {
          if (line == "object") {
            (*memory)[address].Clear();
          }
          else if (line == "end") {
            address++; // Go to next object.
            count++;
          }
          else {
            cArray<std::string> pair = Parse_Sausage_Text(line, "=");
            std::string name = pair[0];
            std::string value = pair[1];
            (*memory)[address].fields[name] = cValue();
            try {
              int number = Text_To_Number(value);
              (*memory)[address].fields[name].Set_Number(number);
            }
            catch (cError error) { // A string.
              (*memory)[address].fields[name].Set_String(value);
            }
          }
        }
      }
    }
    else {
      throw cError("Could not load file " + name + ".");
    }
    return count;
  }

  /**
   * Saves a file from a list of blocks.
   * @param name The name of the file.
   * @param memory The memory module.
   * @param address The address where the list starts.
   * @param count The number of objects to save.
   * @throws An error if the file could not be saved.
   */
  void cSimulator::Save(std::string name, cMemory* memory, int address, int count) {
    std::ofstream file(name);
    if (file) {
      for (int block_index = 0; block_index < count; block_index++) {
        cBlock& block = (*memory)[address + block_index];
        file << "object" << std::endl;
        int key_count = block.fields.Count();
        for (int key_index = 0; key_index < key_count; key_index++) {
          file << block.fields.keys[key_index] << "=";
          if (block.fields.values[key_index].type == eVALUE_NUMBER) {
            file << block.fields.values[key_index].number << std::endl;
          }
          else if (block.fields.values[key_index].type == eVALUE_STRING) {
            file << block.fields.values[key_index].string << std::endl;
          }
        }
        file << "end" << std::endl;
      }
    }
    else {
      throw cError("Could not save file " + name + ".");
    }
  }

}