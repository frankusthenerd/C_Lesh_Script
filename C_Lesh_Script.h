// ============================================================================
// C-Lesh Script (Definitions)
// Programmed by Francois Lamini
// ============================================================================

#include "..\Code_Helper\Codeloader.hpp"
#include "..\Code_Helper\Allegro.hpp"

namespace Codeloader {

  enum eOperator {
    eOPER_ADD,
    eOPER_SUB,
    eOPER_MUL,
    eOPER_DIV,
    eOPER_REM,
    eOPER_RAND,
    eOPER_COS,
    eOPER_SIN,
    eOPER_CAT
  };

  enum eAddress {
    eADDR_VAL_NUMBER,
    eADDR_VAL_STRING,
    eADDR_IMMEDIATE,
    eADDR_POINTER
  };

  enum eCommand {
    eCMD_NONE,
    eCMD_STORE,
    eCMD_SET,
    eCMD_TEST,
    eCMD_CALL,
    eCMD_RETURN,
    eCMD_STOP,
    eCMD_OUTPUT,
    eCMD_DRAW,
    eCMD_REFRESH,
    eCMD_SOUND,
    eCMD_MUSIC,
    eCMD_SILENCE,
    eCMD_INPUT,
    eCMD_TIMEOUT,
    eCMD_COLOR,
    eCMD_LOAD,
    eCMD_SAVE,
    eCMD_PUSH,
    eCMD_POP,
    eCMD_REPEAT,
    eCMD_GET_OBJECT,
    eCMD_GET_LIST
  };

  enum eTest {
    eTEST_EQUALS,
    eTEST_NOT,
    eTEST_LESS,
    eTEST_GREATER,
    eTEST_LESS_OR_EQUAL,
    eTEST_GREATER_OR_EQUAL
  };

  enum eLogic {
    eLOGIC_AND,
    eLOGIC_OR
  };

  typedef cHash<std::string, cValue> tObject;

  struct sOperand_Operator {
    int oper_code;
    int addr_mode;
    cValue value;
    std::string field;
    std::string placeholder;
  };

  typedef std::vector<sOperand_Operator> tExpression;

  struct sCondition_Logic {
    int logic_code;
    int left_exp;
    int test;
    int right_exp;
  };

  class cBlock {

    public:
      int code;
      cArray<tExpression> expressions;
      cArray<sCondition_Logic> conditional;
      tObject fields;
      cValue value;

      cBlock();
      void Clear();

  };

  class cMemory {

    public:
      int count;
      cBlock* memory;

      cMemory(int size);
      ~cMemory();
      cBlock& operator[] (int address);
      void Clear();

  };

  class cCompiler {

    public:
      cHash<std::string, int> symtab;
      cMemory* memory;
      int pointer;
      cArray<sToken> tokens;

      cCompiler(std::string source, cMemory* memory);
      void Parse_Tokens(std::string source);
      sToken Parse_Token();
      sToken Peek_Token();
      void Parse_Keyword(std::string keyword);
      void Generate_Parse_Error(std::string message, sToken token);
      int Parse_Expression(cBlock& command);
      sOperand_Operator Parse_Operand();
      sOperand_Operator Parse_Operator();
      bool Is_Operator();
      void Parse_Address(std::string address, sOperand_Operator& operand);
      void Parse_Conditional(cBlock& command);
      sCondition_Logic Parse_Condition(cBlock& block);
      sCondition_Logic Parse_Logic();
      bool Is_Logic();
      void Parse_Statements();
      void Replace_Placeholders();
      void Preprocess();

  };

  class cSimulator {

    public:
      cMemory* memory;
      int pointer;
      cArray<int> stack;
      cIO_Control* io;
      int status;

      cSimulator(cMemory* memory, cIO_Control* io, int program);
      void Run(int timeout);
      void Command_Processor(cBlock& command);
      cValue Eval_Operand(sOperand_Operator& operand);
      cValue Eval_Expression(cBlock& command, int index);
      bool Eval_Condition(cBlock& command, sCondition_Logic& condition);
      int Eval_Conditional(cBlock& command);
      void Generate_Execution_Error(std::string message, cBlock& command);
      int Load(std::string name, cMemory* memory, int address);
      void Save(std::string name, cMemory* memory, int address, int count);

  };

}