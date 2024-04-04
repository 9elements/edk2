/** @file
  A Setup Menu for configuring boot options defined by bootloader CFR.
  This file parses CFR to produce HII IFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SetupMenu.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CfrHelpersLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/VariablePolicyHelperLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/CfrSetupMenuGuid.h>
#include <Guid/VariableFormat.h>

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

#define CFR_STRING_MAX_SIZE 32
#define EMPTY_STRING_ID 2

typedef struct {
  UINT64 ObjId;
  CFR_OPTION *FoundOption;
} CFR_OPTION_SEARCH;

STATIC
EFI_STATUS
EFIAPI
CfrGetOptionFromObjIdCallback (
    IN CONST CFR_HEADER  *Root,
    IN CONST CFR_HEADER  *Child,
    IN OUT   VOID        *Private
  )
{
  if (Child->Tag == CFR_TAG_FORM) {
    CfrWalkTree ((UINT8 *)Child, CfrGetOptionFromObjIdCallback, NULL);
    return EFI_SUCCESS;
  }

  if (Child->Tag != CFR_TAG_OPTION_BOOL
   && Child->Tag != CFR_TAG_OPTION_NUM_32
   && Child->Tag != CFR_TAG_OPTION_NUM_64
   && Child->Tag != CFR_TAG_OPTION_STRING
   && Child->Tag != CFR_TAG_OPTION_ENUM
   ) {
    return EFI_SUCCESS;
  }

  CFR_OPTION *CfrOption = (CFR_OPTION *)Child;
  CFR_OPTION_SEARCH *CfrSearch = (CFR_OPTION_SEARCH *)Private;

  if (CfrSearch->ObjId == CfrOption->ObjectId) {
    // found option
    CfrSearch->FoundOption = CfrOption;
  }
  return EFI_SUCCESS;
}

STATIC
CFR_OPTION*
GetOptionFromObjId (
  IN UINT64 ObjId
  )
{
  // Get CFR Form
  EFI_HOB_GUID_TYPE *GuidHob = GetFirstGuidHob (&gEfiCfrSetupMenuFormGuid);
  ASSERT(GuidHob != NULL);
  CFR_FORM *CfrForm = GET_GUID_HOB_DATA (GuidHob);
  ASSERT(CfrForm != NULL);

  CFR_OPTION_SEARCH OptionSearch = {
    .ObjId = ObjId,
    .FoundOption = NULL
  };
  CfrWalkTree ((UINT8 *)CfrForm, CfrGetOptionFromObjIdCallback, &OptionSearch);
  ASSERT (OptionSearch.FoundOption != NULL);
  return OptionSearch.FoundOption;
}

STATIC
VOID
EFIAPI
CfrProduceHiiValue (
  IN VOID *StartOpCodeHandle,
  IN BUFFER *StrsBuf,
  IN CFR_TAG Tag, // contains the type which the expression should produce
  IN UINT64 Value,
  IN BOOLEAN Default // always produce default value for given type instead of using 'Value'
  )
{
  UINT8 *TempHiiBuffer;
  switch (Tag) {
  case CFR_TAG_OPTION_BOOL:
    EFI_IFR_TRUE IfrTrueFalse;
    IfrTrueFalse.Header.OpCode = EFI_IFR_FALSE_OP; // Produce default for type
    if (!Default)
      IfrTrueFalse.Header.OpCode = EFI_IFR_TRUE_OP;
    IfrTrueFalse.Header.Length = sizeof(IfrTrueFalse);
    IfrTrueFalse.Header.Scope = 0; // Same scope as above statement
    TempHiiBuffer = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&IfrTrueFalse,
                                   sizeof (IfrTrueFalse)
                                   );
    ASSERT (TempHiiBuffer != NULL);
    break;
  case CFR_TAG_OPTION_NUM_32:
  case CFR_TAG_OPTION_ENUM:
    EFI_IFR_UINT32 IfrNum32;
    IfrNum32.Header.OpCode = EFI_IFR_UINT32_OP;
    IfrNum32.Header.Length = sizeof(IfrNum32);
    IfrNum32.Header.Scope = 0; // Same scope as above statement
    IfrNum32.Value = 0;
    if (!Default)
      IfrNum32.Value = (UINT32)Value;
    TempHiiBuffer = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&IfrNum32,
                                   sizeof (IfrNum32)
                                   );
    ASSERT (TempHiiBuffer != NULL);
    break;
  case CFR_TAG_OPTION_NUM_64:
    EFI_IFR_UINT64 IfrNum64;
    IfrNum64.Header.OpCode = EFI_IFR_UINT64_OP;
    IfrNum64.Header.Length = sizeof(IfrNum64);
    IfrNum64.Header.Scope = 0; // Same scope as above statement
    IfrNum64.Value = 0;
    if (!Default)
      IfrNum64.Value = Value;
    TempHiiBuffer = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&IfrNum64,
                                   sizeof (IfrNum64)
                                   );
    ASSERT (TempHiiBuffer != NULL);
    break;
  case CFR_TAG_OPTION_STRING:
    EFI_IFR_STRING_REF1 IfrStr;
    IfrStr.Header.OpCode = EFI_IFR_STRING_REF1_OP;
    IfrStr.Header.Length = sizeof(IfrStr);
    IfrStr.Header.Scope = 0; // Same scope as above statement
    IfrStr.StringId = EMPTY_STRING_ID;
    if (!Default) {
      CONST CHAR8 *StrValue = CfrExtractString(Value);
      EFI_STRING_ID HiiStringId = AddHiiStringBlock(StrsBuf, EFI_HII_SIBT_STRING_SCSU, StrValue);;
      IfrStr.StringId = HiiStringId;
    }
    TempHiiBuffer = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&IfrStr,
                                   sizeof (IfrStr)
                                   );
    ASSERT (TempHiiBuffer != NULL);
    break;
  default:
    DEBUG ((DEBUG_INFO, "CFR: %a() %d Unknown Tag: %d\n", __func__, __LINE__, Tag));
    ASSERT(0);
  }
}

STATIC
VOID
EFIAPI
CfrProduceHiiExpr (
  IN VOID *StartOpCodeHandle,
  IN BUFFER *StrsBuf,
  IN CFR_EXPR *CfrExpr,
  IN UINTN Size,
  IN CFR_TAG Tag, // contains the type which the expression should produce
  IN UINT8 Scope // 1 if expression is the start of a StatementExpression
  )
{
  UINT8 *TempHiiBuffer;
  UINTN ExprSize;
  if (!Size) {
    // we traversed all cfr expr
    return;
  }
  ExprSize = sizeof(CFR_EXPR);

  switch (CfrExpr->Type) {
  case CFR_EXPR_TYPE_CONST_NUM:
    UINT64 NumValue = CfrExpr->Value;
    EFI_IFR_UINT64 IfrNum;
    IfrNum.Header.OpCode = EFI_IFR_UINT64_OP;
    IfrNum.Header.Length = sizeof(IfrNum);
    IfrNum.Header.Scope = Scope;
    IfrNum.Value = NumValue;
    DEBUG ((DEBUG_INFO, "CFR: %a() %d CONST_UINT64\n", __func__, __LINE__));
    TempHiiBuffer = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&IfrNum,
                                   sizeof (IfrNum)
                                   );
    ASSERT (TempHiiBuffer != NULL);
    break;
  case CFR_EXPR_TYPE_CONST_BOOL:
    UINT64 BoolValue = CfrExpr->Value;
    EFI_IFR_TRUE IfrTrueFalse;
    IfrTrueFalse.Header.OpCode = BoolValue ? EFI_IFR_TRUE_OP : EFI_IFR_FALSE_OP;
    IfrTrueFalse.Header.Length = sizeof(IfrTrueFalse);
    IfrTrueFalse.Header.Scope = Scope;
    DEBUG ((DEBUG_INFO, "CFR: %a() %d CONST_BOOL\n", __func__, __LINE__));
    TempHiiBuffer = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&IfrTrueFalse,
                                   sizeof (IfrTrueFalse)
                                   );
    ASSERT (TempHiiBuffer != NULL);
    break;
  case CFR_EXPR_TYPE_CONST_STRING:
    CONST CHAR8 *StrValue = CfrExtractString(CfrExpr->Value);
    EFI_STRING_ID HiiStringId = AddHiiStringBlock(StrsBuf, EFI_HII_SIBT_STRING_SCSU, StrValue);;

    EFI_IFR_STRING_REF1 IfrString;
    IfrString.Header.OpCode = EFI_IFR_STRING_REF1_OP;
    IfrString.Header.Length = sizeof(IfrString);
    IfrString.Header.Scope = Scope;
    IfrString.StringId = HiiStringId;
    DEBUG ((DEBUG_INFO, "CFR: %a() %d CONST_STRING\n", __func__, __LINE__));
    TempHiiBuffer = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&IfrString,
                                   sizeof (IfrString)
                                   );
    ASSERT (TempHiiBuffer != NULL);
    break;
  case CFR_EXPR_TYPE_SYMBOL:
    UINT64 QuestionId = CfrExpr->Value;
    //TODO If used under default node, both EFI_IFR_QUESTION_REF1 and EFI_IFR_EQ_ID_VAL only apply the default at load time. Ideally the value of this Question would change depending on if the user selected the Question we are depending on (like how it works in Kconfig menuconfig).
    EFI_IFR_QUESTION_REF1 IfrQuestRef;
    //EFI_IFR_EQ_ID_VAL IfrQuestRef;
    //IfrQuestRef.Header.OpCode = EFI_IFR_EQ_ID_VAL_OP;
    IfrQuestRef.Header.OpCode = EFI_IFR_QUESTION_REF1_OP;
    IfrQuestRef.Header.Length = sizeof(IfrQuestRef);
    IfrQuestRef.Header.Scope = Scope;
    IfrQuestRef.QuestionId = QuestionId;
    //IfrQuestRef.Value = 1;
    TempHiiBuffer = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&IfrQuestRef,
                                   sizeof (IfrQuestRef)
                                   );
    ASSERT (TempHiiBuffer != NULL);
    break;
  case CFR_EXPR_TYPE_OPERATOR:
    switch (CfrExpr->Value) {
      case CFR_OPERATOR_NOT:
        EFI_IFR_NOT IfrNot;
        IfrNot.Header.OpCode = EFI_IFR_NOT_OP;
        IfrNot.Header.Length = sizeof(IfrNot);
        IfrNot.Header.Scope = Scope;
        DEBUG ((DEBUG_INFO, "CFR: %a() %d NOT\n", __func__, __LINE__));
        TempHiiBuffer = HiiCreateRawOpCodes (
                                       StartOpCodeHandle,
                                       (UINT8 *)&IfrNot,
                                       sizeof (IfrNot)
                                       );
        ASSERT (TempHiiBuffer != NULL);
        break;
      case CFR_OPERATOR_AND:
        EFI_IFR_AND IfrAnd;
        IfrAnd.Header.OpCode = EFI_IFR_AND_OP;
        IfrAnd.Header.Length = sizeof(IfrAnd);
        IfrAnd.Header.Scope = Scope;
        DEBUG ((DEBUG_INFO, "CFR: %a() %d AND\n", __func__, __LINE__));
        TempHiiBuffer = HiiCreateRawOpCodes (
                                       StartOpCodeHandle,
                                       (UINT8 *)&IfrAnd,
                                       sizeof (IfrAnd)
                                       );
        ASSERT (TempHiiBuffer != NULL);
        break;
      case CFR_OPERATOR_OR:
        EFI_IFR_OR IfrOr;
        IfrOr.Header.OpCode = EFI_IFR_OR_OP;
        IfrOr.Header.Length = sizeof(IfrOr);
        IfrOr.Header.Scope = Scope;
        DEBUG ((DEBUG_INFO, "CFR: %a() %d OR\n", __func__, __LINE__));
        TempHiiBuffer = HiiCreateRawOpCodes (
                                       StartOpCodeHandle,
                                       (UINT8 *)&IfrOr,
                                       sizeof (IfrOr)
                                       );
        ASSERT (TempHiiBuffer != NULL);
        break;
      case CFR_OPERATOR_EQUAL:
        EFI_IFR_EQUAL IfrEqual;
        IfrEqual.Header.OpCode = EFI_IFR_EQUAL_OP;
        IfrEqual.Header.Length = sizeof(IfrEqual);
        IfrEqual.Header.Scope = Scope;
        DEBUG ((DEBUG_INFO, "CFR: %a() %d OR\n", __func__, __LINE__));
        TempHiiBuffer = HiiCreateRawOpCodes (
                                       StartOpCodeHandle,
                                       (UINT8 *)&IfrEqual,
                                       sizeof (IfrEqual)
                                       );
        ASSERT (TempHiiBuffer != NULL);
        break;
      default:
        break;
    }
    break;
  default:
    ASSERT(0);
  }
  ASSERT(ExprSize <= Size);
  Size -= ExprSize;
  CfrProduceHiiExpr(StartOpCodeHandle, StrsBuf, (CFR_EXPR *)((UINT8 *)CfrExpr + ExprSize), Size, Tag, 0);
}
STATIC
VOID
EFIAPI
CfrProduceHiiDefault (
  IN VOID *StartOpCodeHandle,
  IN BUFFER *StrsBuf,
  CFR_HEADER *Option,
  CFR_DEFAULT *Default
  )
{
  UINT8 *TempHiiBuffer;

  //EFI_IFR_DEFAULT EfiIfrDefault;
  EFI_IFR_DEFAULT_2 EfiIfrDefault;
  ZeroMem (&EfiIfrDefault, sizeof (EfiIfrDefault));
  EfiIfrDefault.Header.OpCode = EFI_IFR_DEFAULT_OP;
  //EfiIfrDefault.Header.Scope = 0; // new scope
  EfiIfrDefault.Header.Scope = 1; // new scope
  EfiIfrDefault.Header.Length = sizeof(EfiIfrDefault);
  // If Type is EFI_IFR_TYPE_OTHER, then the default value is provided by a nested EFI_IFR_VALUE
  //EfiIfrDefault.Type = EFI_IFR_TYPE_NUM_SIZE_64;
  EfiIfrDefault.Type = EFI_IFR_TYPE_OTHER;
  EfiIfrDefault.DefaultId = EFI_HII_DEFAULT_CLASS_STANDARD;
  //EfiIfrDefault.Value.u64 = 42;

  TempHiiBuffer = HiiCreateRawOpCodes (
                      StartOpCodeHandle,
                      (UINT8 *)&EfiIfrDefault,
                      sizeof (EfiIfrDefault)
                      );
  ASSERT(TempHiiBuffer != NULL);

  EFI_IFR_VALUE EfiIfrValue;
  EfiIfrValue.Header.OpCode = EFI_IFR_VALUE_OP;
  EfiIfrValue.Header.Scope = 1;
  EfiIfrValue.Header.Length = sizeof (EfiIfrValue);

  TempHiiBuffer = HiiCreateRawOpCodes (
                      StartOpCodeHandle,
                      (UINT8 *)&EfiIfrValue,
                      sizeof (EfiIfrValue)
                      );
  ASSERT (TempHiiBuffer != NULL);

  // 1. First write the conditional opcode
  // 2. write opcode (expression) that will be used as default if the visible condition is true
  // 3. write opcode (expression) that will be used as default if the visible condition is false
  // 4. write opcode visible expression
  CFR_EXPRESSION *CfrVisibleExpression; // expression must resolve to a boolean value
  EFI_STATUS FoundVisibleExpressionStatus = CfrFindTag ((UINT8 *)Default, CFR_TAG_DEFAULT_VISIBLE, (CONST VOID **)&CfrVisibleExpression);

  if (FoundVisibleExpressionStatus == EFI_SUCCESS && CfrVisibleExpression) {
    DEBUG ((DEBUG_INFO, "CFR: %a() %d Visible Default Expression\n", __func__, __LINE__));
    // 4. write opcode visible expression
    CFR_EXPR *CfrVisibleExpr = (CFR_EXPR *)((UINT8 *)CfrVisibleExpression + sizeof(CFR_EXPRESSION));
    CfrProduceHiiExpr (StartOpCodeHandle, StrsBuf, CfrVisibleExpr, CfrVisibleExpression->Size - sizeof(CFR_EXPRESSION), CFR_TAG_OPTION_BOOL, 1);

    // 3. write opcode (expression) that will be used as default if the visible condition is false
    // TODO currently no support in CFR for multiple defaults, therefore just fall back to default value of Option->Tag type
    CfrProduceHiiValue (StartOpCodeHandle, StrsBuf, Option->Tag, 0, TRUE);
  }

  // 2. write opcode (expression) that will be used as default if the visible condition is true
  CfrProduceHiiValue(StartOpCodeHandle, StrsBuf, Option->Tag, Default->Value, FALSE);

  if (FoundVisibleExpressionStatus == EFI_SUCCESS && CfrVisibleExpression) {
    // 1. First write the conditional opcode (here)
    EFI_IFR_CONDITIONAL  ConditionalOp;

    ConditionalOp.Header.OpCode = EFI_IFR_CONDITIONAL_OP;
    ConditionalOp.Header.Length = sizeof (EFI_IFR_CONDITIONAL);
    ConditionalOp.Header.Scope = 0;

    DEBUG ((DEBUG_INFO, "CFR: %a() %d Visible Default Expression\n", __func__, __LINE__));
    TempHiiBuffer = HiiCreateRawOpCodes (
                      StartOpCodeHandle,
                      (UINT8 *)&ConditionalOp,
                      sizeof (EFI_IFR_OP_HEADER)
                      );
    ASSERT (TempHiiBuffer != NULL);

    // close StatementExpression scope (or conditional scope?)
    TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
    ASSERT (TempHiiBuffer != NULL);
  }

  // close VALUE_OP scope
  TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
  ASSERT (TempHiiBuffer != NULL);

  // close DEFAULT_OP scope
  TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
  ASSERT (TempHiiBuffer != NULL);
}

STATIC
VOID
EFIAPI
CfrProduceHiiIf (
  IN VOID           *StartOpCodeHandle,
  IN BUFFER         *StrsBuf,
  IN UINT8           OpCode,
  IN CFR_EXPRESSION *CfrExpression
  )
{
  UINT8              *TempHiiBuffer;
  EFI_IFR_OP_HEADER  IfOpHeader;

  IfOpHeader.OpCode = OpCode;
  IfOpHeader.Length = sizeof (EFI_IFR_OP_HEADER);
  IfOpHeader.Scope = 1; // `if` statements are new scopes

  TempHiiBuffer = HiiCreateRawOpCodes (
                    StartOpCodeHandle,
                    (UINT8 *)&IfOpHeader,
                    sizeof (EFI_IFR_OP_HEADER)
                    );
  ASSERT (TempHiiBuffer != NULL);

  if (!CfrExpression) {
    // If there is no expression we assume it is always true
    EFI_IFR_TRUE IfrTrue;
    IfrTrue.Header.OpCode = EFI_IFR_TRUE_OP;
    IfrTrue.Header.Length = sizeof(IfrTrue);
    IfrTrue.Header.Scope = 0; // Same scope as above statement
    TempHiiBuffer = HiiCreateRawOpCodes (StartOpCodeHandle, (UINT8 *)&IfrTrue, sizeof (IfrTrue));
    ASSERT (TempHiiBuffer != NULL);
  } else {
    CFR_EXPR *CfrExpr = (CFR_EXPR *)((UINT8 *)CfrExpression + sizeof(CFR_EXPRESSION));
    CfrProduceHiiExpr (StartOpCodeHandle, StrsBuf, CfrExpr, CfrExpression->Size - sizeof(CFR_EXPRESSION), CFR_TAG_OPTION_BOOL, 1);

    // Add a NOT for suppressif and grayoutif
    EFI_IFR_NOT IfrNot;
    IfrNot.Header.OpCode = EFI_IFR_NOT_OP;
    IfrNot.Header.Length = sizeof(IfrNot);
    IfrNot.Header.Scope = 0; // Same scope as above statement
    TempHiiBuffer = HiiCreateRawOpCodes (StartOpCodeHandle, (UINT8 *)&IfrNot, sizeof (IfrNot));
    ASSERT (TempHiiBuffer != NULL);

    // close statement produced by CfrProduceHiiExpr
    TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
    ASSERT (TempHiiBuffer != NULL);
  }
}

STATIC
EFI_STATUS
OptionEvaluateStringValue(
  IN  CFR_OPTION  *Option,
  OUT UINTN       *DataSize,
  OUT CHAR16     **Value
  );

STATIC
EFI_STATUS
OptionEvaluateUint64Value(
  IN  CFR_OPTION  *Option,
  OUT UINT64      *Value
  );


STATIC
VOID
ExprGetValue (
    IN OUT CFR_EXPR *Expr,
    OUT UINT64      *Num,
    OUT CHAR16     **Str
    )
{
  UINTN DataSize;
  if (Expr->Type == CFR_EXPR_TYPE_SYMBOL) {
    CFR_OPTION *Option = GetOptionFromObjId(Expr->Value);
    if (Option->Tag == CFR_TAG_OPTION_STRING) {
      ASSERT(Str != NULL);
      OptionEvaluateStringValue(Option, &DataSize, Str);
      ASSERT (DataSize == StrSize(*Str));
      Expr->Type = CFR_EXPR_TYPE_CONST_STRING;
    } else if (Option->Tag == CFR_TAG_OPTION_NUM_32
           ||  Option->Tag == CFR_TAG_OPTION_NUM_64
           ||  Option->Tag == CFR_TAG_OPTION_ENUM) {
      OptionEvaluateUint64Value(Option, Num);
      Expr->Type = CFR_EXPR_TYPE_CONST_NUM;
    } else if (Option->Tag == CFR_TAG_OPTION_BOOL) {
      OptionEvaluateUint64Value(Option, Num);
      Expr->Type = CFR_EXPR_TYPE_CONST_BOOL;
    }
  } else {
    if (Expr->Type == CFR_EXPR_TYPE_CONST_STRING) {
      ASSERT(Str != NULL);
      CONST CHAR8 *CfrStr = CfrExtractString(Expr->Value);
      UINTN CfrStrSize = AsciiStrSize(CfrStr);
      *Str = AllocatePool(CfrStrSize * sizeof(CHAR16));
      AsciiStrToUnicodeStrS(CfrStr, *Str, CfrStrSize);
    } else if (Expr->Type == CFR_EXPR_TYPE_CONST_NUM) {
      *Num = Expr->Value;
    } else if (Expr->Type == CFR_EXPR_TYPE_CONST_BOOL) {
      *Num = !!Expr->Value;
    }
  }
}


STATIC
BOOLEAN
EvaluateVisibleExpression (
  IN CFR_OPTION     *Option,
  IN CFR_EXPRESSION *Expression
  )
{
  UINT32 ExprElements = (Expression->Size - sizeof(CFR_EXPRESSION)) / sizeof(CFR_EXPR);
  CFR_EXPR ValueStack[ExprElements];
  UINTN j = 0;

#define POP_VALUE_STACK() ValueStack[--j]
#define PUSH_VALUE_STACK(Value) ValueStack[j++] = Value

  for (UINTN i = 0; i < ExprElements; i++) {
    CFR_EXPR *Expr = &Expression->Exprs[i];
    switch (Expr->Type) {
      case CFR_EXPR_TYPE_CONST_BOOL:
      case CFR_EXPR_TYPE_CONST_NUM:
      case CFR_EXPR_TYPE_CONST_STRING:
      case CFR_EXPR_TYPE_SYMBOL:
        PUSH_VALUE_STACK(*Expr);
        break;
      case CFR_EXPR_TYPE_OPERATOR:
        switch (Expr->Value) {
          case CFR_OPERATOR_NOT:
            CFR_EXPR NotValue = POP_VALUE_STACK();
            if (NotValue.Type == CFR_EXPR_TYPE_SYMBOL) {
              CFR_OPTION *Option1 = GetOptionFromObjId(NotValue.Value);
              ASSERT (Option1->Tag == CFR_TAG_OPTION_BOOL); // You can only NOT const bool expressions
              OptionEvaluateUint64Value(Option1, &NotValue.Value);
            } else {
              ASSERT(NotValue.Type == CFR_EXPR_TYPE_CONST_BOOL); // You can only NOT const bool expressions
            }
            NotValue.Value = !NotValue.Value;
            PUSH_VALUE_STACK(NotValue);
            break;
          case CFR_OPERATOR_OR:
          case CFR_OPERATOR_AND:
            CFR_EXPR AndOrValue1 = POP_VALUE_STACK();
            if (AndOrValue1.Type == CFR_EXPR_TYPE_SYMBOL) {
              CFR_OPTION *Option1 = GetOptionFromObjId(AndOrValue1.Value);
              ASSERT (Option1->Tag == CFR_TAG_OPTION_BOOL); // You can only AND bool expressions
              OptionEvaluateUint64Value(Option1, &AndOrValue1.Value);
            } else {
              ASSERT(AndOrValue1.Type == CFR_EXPR_TYPE_CONST_BOOL); // You can only AND bool expressions
            }
            CFR_EXPR AndOrValue2 = POP_VALUE_STACK();
            if (AndOrValue2.Type == CFR_EXPR_TYPE_SYMBOL) {
              CFR_OPTION *Option2 = GetOptionFromObjId(AndOrValue2.Value);
              ASSERT (Option2->Tag == CFR_TAG_OPTION_BOOL); // You can only AND bool expressions
              OptionEvaluateUint64Value(Option2, &AndOrValue2.Value);
            } else {
              ASSERT(AndOrValue2.Type == CFR_EXPR_TYPE_CONST_BOOL); // You can only AND bool expressions
            }

            CFR_EXPR AndOrResultValue;
            AndOrResultValue.Type = CFR_EXPR_TYPE_CONST_BOOL;
            if (Expr->Value == CFR_OPERATOR_OR)
                AndOrResultValue.Value = !!(AndOrValue1.Value || AndOrValue2.Value);
            if (Expr->Value == CFR_OPERATOR_AND)
                AndOrResultValue.Value = !!(AndOrValue1.Value && AndOrValue2.Value);
            PUSH_VALUE_STACK(AndOrResultValue);
            break;
          case CFR_OPERATOR_EQUAL:
            UINT64 Num1, Num2;
            CHAR16 *Str1, *Str2;
            CFR_EXPR EqualValue1 = POP_VALUE_STACK();
            CFR_EXPR EqualValue2 = POP_VALUE_STACK();

            ExprGetValue(&EqualValue1, &Num1, &Str1);
            ExprGetValue(&EqualValue2, &Num2, &Str2);
            ASSERT (EqualValue1.Type == EqualValue2.Type); // type safety
            
            CFR_EXPR EqualResultValue;
            EqualResultValue.Type = CFR_EXPR_TYPE_CONST_BOOL;
            switch (EqualValue1.Type) {
              case CFR_EXPR_TYPE_CONST_BOOL:
                EqualResultValue.Value = (!!Num1) == (!!Num2);
                break;
              case CFR_EXPR_TYPE_CONST_NUM:
                EqualResultValue.Value = Num1 == Num2;
                break;
              case CFR_EXPR_TYPE_CONST_STRING:
                EqualResultValue.Value = !StrCmp(Str1, Str2);
                FreePool(Str1);
                FreePool(Str2);
                break;
            }
            PUSH_VALUE_STACK(EqualResultValue);
            break;
          default:
            ASSERT(0);
            break;
        }
        break;
      default:
        ASSERT(0);
    }
  }
  ASSERT (j == 1); // There should only be one Value left on the ValueStack
  CFR_EXPR FinalExpr = POP_VALUE_STACK();
  UINT64 ExprResult;
  ExprGetValue(&FinalExpr, &ExprResult, NULL);
  ASSERT (FinalExpr.Type == CFR_EXPR_TYPE_CONST_BOOL); // Visible expression MUST evaluate to a constant bool
  return !!FinalExpr.Value;
}

/*
 * return EFI_NOT_FOUND if the GetVariable Returns EFI_NOT_FOUND. In this case the default for this option is evaluated and returned. Returns EFI_SUCCESS if the underlying storage is already set with a value. The value is then returned.
 */
STATIC
EFI_STATUS
OptionEvaluateUint64Value (
  IN  CFR_OPTION  *Option,
  UINT64          *Value
  )
{
  EFI_STATUS Status;

  // get option name
  CONST CHAR8 *OptionName = CfrExtractString (Option->Name);
  ASSERT (OptionName != NULL);
  DEBUG ((DEBUG_INFO, "CFR: Option Name: %a\n", OptionName));
  // convert to UTF-16
  UINTN CfrStringSize = AsciiStrSize(OptionName);
  CHAR16 *VariableName = AllocatePool (CfrStringSize * sizeof (CHAR16));
  ASSERT (VariableName != NULL);
  Status = AsciiStrToUnicodeStrS (OptionName, VariableName, CfrStringSize);
  ASSERT (Status == EFI_SUCCESS);

  UINTN DataSize = 4;
  if (Option->Tag == CFR_TAG_OPTION_NUM_64)
    DataSize = 8;

  Status = gRT->GetVariable (VariableName, &gEfiCfrNvDataGuid, NULL, &DataSize, Value);
  ASSERT (Status != EFI_BUFFER_TOO_SMALL);
  if (Status == EFI_SUCCESS) {
    ASSERT (DataSize == 8 || DataSize == 4);
    FreePool(VariableName);
    return EFI_SUCCESS;
  }
  if (Status == EFI_NOT_FOUND) {
    // the option does not have an underlying storage variable set. Evaluate the Default value and return it

    CFR_DEFAULT *Default = NULL;
    CfrFindTag ((UINT8 *)Option, CFR_TAG_DEFAULT, (CONST VOID **)&Default);
    ASSERT(Default != NULL); //TODO return the default for the given option type instead
    CFR_EXPRESSION *CfrVisibleExpression; // expression must resolve to a boolean value
    EFI_STATUS FoundVisibleExpressionStatus = CfrFindTag ((UINT8 *)Default, CFR_TAG_DEFAULT_VISIBLE, (CONST VOID **)&CfrVisibleExpression);
    if (FoundVisibleExpressionStatus != EFI_SUCCESS) {
      // No visible expression found. We can return the default value
      *Value = Default->Value;
      FreePool(VariableName);
      return EFI_NOT_FOUND;
    }
    ASSERT (CfrVisibleExpression != NULL);

    BOOLEAN Visible = EvaluateVisibleExpression(Option, CfrVisibleExpression);
    if (Visible) {
      // Default is visible. We can set and return the default value
      *Value = Default->Value;
      FreePool(VariableName);
      return EFI_NOT_FOUND;
    }
    // Default is not visible. return the default of 0
    *Value = 0;
    FreePool(VariableName);
    return EFI_NOT_FOUND;
  }
  FreePool(VariableName);
  ASSERT_EFI_ERROR(Status);
  return Status;
}

/*
 * return EFI_NOT_FOUND if the GetVariable Returns EFI_NOT_FOUND. In this case the default for this option is evaluated and returned. Returns EFI_SUCCESS if the underlying storage is already set with a value. The value is then returned.
 */
STATIC
EFI_STATUS
OptionEvaluateStringValue (
  IN  CFR_OPTION  *Option,
  OUT UINTN       *DataSize,
  OUT CHAR16     **Value
  )
{
  EFI_STATUS Status;

  // get option name
  CONST CHAR8 *OptionName = CfrExtractString (Option->Name);
  ASSERT (OptionName != NULL);
  // convert to UTF-16
  UINTN CfrStringSize = AsciiStrSize(OptionName);
  CHAR16 *VariableName = AllocatePool (CfrStringSize * sizeof (CHAR16));
  ASSERT (VariableName != NULL);
  Status = AsciiStrToUnicodeStrS (OptionName, VariableName, CfrStringSize);
  ASSERT (Status == EFI_SUCCESS);

  Status = gRT->GetVariable (VariableName, &gEfiCfrNvDataGuid, NULL, DataSize, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    *Value = AllocatePool(*DataSize);
    // If the variable has already a value set we can simply return it
    Status = gRT->GetVariable (VariableName, &gEfiCfrNvDataGuid, NULL, DataSize, *Value);
    ASSERT (Status == EFI_SUCCESS);
    FreePool(VariableName);
    return EFI_SUCCESS;
  }
  ASSERT(Status == EFI_NOT_FOUND);

  // the option does not have an underlying storage variable set. Evaluate the Default value and return it

  CFR_DEFAULT *Default = NULL;
  CfrFindTag ((UINT8 *)Option, CFR_TAG_DEFAULT, (CONST VOID **)&Default);
  ASSERT(Default != NULL); //TODO return default for the given type of option instead
  CFR_EXPRESSION *CfrVisibleExpression; // expression must resolve to a boolean value
  EFI_STATUS FoundVisibleExpressionStatus = CfrFindTag ((UINT8 *)Default, CFR_TAG_DEFAULT_VISIBLE, (CONST VOID **)&CfrVisibleExpression);
  if (FoundVisibleExpressionStatus != EFI_SUCCESS
   || EvaluateVisibleExpression(Option, CfrVisibleExpression) ) {
    // No visible expression found or default is visible
    // We can return the default value
    CONST CHAR8 *DefStr = CfrExtractString(Default->Value);
    UINTN DefStrSize = AsciiStrLen(DefStr) + 1;
    *DataSize = DefStrSize * sizeof(CHAR16);
    *Value = AllocatePool(*DataSize);
    ASSERT (*Value != NULL);
    Status = AsciiStrToUnicodeStrS (DefStr, *Value, DefStrSize);
    ASSERT_EFI_ERROR (Status);
  } else {
    // Default is not visible. Just return the default for the given type
    *Value = AllocatePool(1);
    ASSERT_EFI_ERROR (*Value);
    ((CHAR8 *)*Value)[0] = '\0';
  }

  FreePool(VariableName);
  return EFI_NOT_FOUND;
}

//
// Produce IFR VARSTORE for option and register variable policies.
//
STATIC
EFI_STATUS
EFIAPI
CfrProduceStorageForOption (
  IN CONST CHAR8  *AsciiVariableName,
  IN  CFR_OPTION  *CfrOption,
  IN        VOID  *StartOpCodeHandle,
  IN       UINTN   QuestionIdVarStoreId,
  IN     CFR_TAG   Tag
  )
{
  CHAR16           *VariableName;
  UINTN             VariableNameLen;
  UINTN             AsciiVariableNameLen;
  EFI_STATUS        Status;
  UINTN             VarStoreStructSize;
  EFI_IFR_VARSTORE_EFI *VarStore;
  UINT8            *TempHiiBuffer;

  if ((AsciiVariableName == NULL) || (StartOpCodeHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  AsciiVariableNameLen = AsciiStrLen (AsciiVariableName) + 1;
  VariableNameLen = AsciiVariableNameLen * sizeof(CHAR16);
  VariableName = AllocatePool(VariableNameLen);
  AsciiStrToUnicodeStrS(AsciiVariableName, VariableName, VariableNameLen);


  EDKII_VARIABLE_POLICY_PROTOCOL *mVariablePolicy = NULL;
  Status = gBS->LocateProtocol (&gEdkiiVariablePolicyProtocolGuid, NULL, (VOID **)&mVariablePolicy);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "CFR: Unable to lock variables!\n"));
  }

  // currently all read only options are volatile, so we don't need to lock them down.
  // TODO need to test things with authenticated variables.
  if (mVariablePolicy != NULL) {
    Status = RegisterBasicVariablePolicy (
               mVariablePolicy,
               &mSetupMenuHiiGuid,
               VariableName,
               VARIABLE_POLICY_NO_MIN_SIZE,
               VARIABLE_POLICY_NO_MAX_SIZE,
               VARIABLE_POLICY_NO_MUST_ATTR,
               VARIABLE_POLICY_NO_CANT_ATTR,
               VARIABLE_POLICY_TYPE_NO_LOCK
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "CFR: Failed to lock variable \"%s\"!\n", VariableName));
    }
  }

  //
  // Create IFR Varstore opcode
  //

  // Struct contains space for terminator only, allocate with name too
  VarStoreStructSize = sizeof (EFI_IFR_VARSTORE_EFI) - 1 + AsciiVariableNameLen;
  ASSERT (VarStoreStructSize <= 0x7F);
  if (VarStoreStructSize > 0x7F) {
    DEBUG ((DEBUG_ERROR, "CFR: Option name length 0x%x is too long!\n", AsciiVariableNameLen));
    FreePool (VariableName);
    return EFI_OUT_OF_RESOURCES;
  }

  VarStore = AllocateZeroPool (VarStoreStructSize);
  ASSERT (VarStore != NULL);
  if (VarStore == NULL) {
    DEBUG ((DEBUG_ERROR, "CFR: Failed to allocate memory for varstore!\n"));
    FreePool (VariableName);
    return EFI_OUT_OF_RESOURCES;
  }

  VarStore->Header.OpCode = EFI_IFR_VARSTORE_EFI_OP;
  VarStore->Header.Length = VarStoreStructSize;

  // Each option gets its own varstore.
  // We can therefore create a direct mapping between QuestionId and VarstoreId.
  VarStore->VarStoreId = QuestionIdVarStoreId;
  VarStore->Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS;
  if (!(CfrOption->Flags & CFR_OPTFLAG_VOLATILE)) {
    VarStore->Attributes |= EFI_VARIABLE_NON_VOLATILE;
  }
  switch (Tag) {
    case CFR_TAG_OPTION_BOOL:
      VarStore->Size = 4;
    break;
    case CFR_TAG_OPTION_ENUM:
      VarStore->Size = 4;
    break;
    case CFR_TAG_OPTION_NUM_32:
      VarStore->Size = 4;
    break;
    case CFR_TAG_OPTION_NUM_64:
      VarStore->Size = 8;
    break;
    case CFR_TAG_OPTION_STRING:
      VarStore->Size = CFR_STRING_MAX_SIZE;
    break;
    default:
      ASSERT(0);
    break;
  }

  CopyMem (&VarStore->Guid, &gEfiCfrNvDataGuid, sizeof (EFI_GUID));
  CopyMem (VarStore->Name, AsciiVariableName, AsciiVariableNameLen); // apparently the VarStore name is always ASCII

  TempHiiBuffer = HiiCreateRawOpCodes (StartOpCodeHandle, (UINT8 *)VarStore, VarStoreStructSize);
  ASSERT (TempHiiBuffer != NULL);
  FreePool (VarStore);
  FreePool (VariableName);

  return EFI_SUCCESS;
}

//
// Creates a Goto Opcode to a Form if the Form has a visible prompt (aka configurable)
//
STATIC
EFI_STATUS
EFIAPI
CfrProduceHiiGotoForm (
  IN  CFR_FORM  *Option,
  IN  VOID      *StartOpCodeHandle,
  IN  BUFFER    *StrsBuf,
  IN  UINT16     FormId
  )
{
  UINT8 *TempHiiBuffer;

  //
  // Check if Prompt is visible (aka Form is configurable)
  //
  CFR_PROMPT *PromptOption;
  EFI_STATUS PromptStatus = CfrFindTag ((UINT8 *)Option, CFR_TAG_PROMPT, (CONST VOID **)&PromptOption);
  ASSERT(PromptStatus == EFI_SUCCESS); // prompt is required for a form
  CONST CHAR8 *Prompt = CfrExtractString (PromptOption->Prompt);
  ASSERT (Prompt != NULL);
  CFR_EXPRESSION *PromptVisibleExpression = NULL;
  EFI_STATUS PromptVisibleStatus = CfrFindTag ((UINT8 *)PromptOption, CFR_TAG_PROMPT_VISIBLE, (CONST VOID **)&PromptVisibleExpression);
  if (PromptVisibleStatus == EFI_SUCCESS && PromptVisibleExpression) {
    CfrProduceHiiIf(StartOpCodeHandle, StrsBuf, EFI_IFR_SUPPRESS_IF_OP, PromptVisibleExpression);
  }
  EFI_STRING_ID PromptStringId = AddHiiStringBlock(StrsBuf, EFI_HII_SIBT_STRING_SCSU, Prompt);

  DEBUG ((
    DEBUG_INFO,
    "CFR: Process form \"%a\" of size 0x%x, goto formid: %d\n",
    Prompt,
    Option->Size,
    FormId
    ));

  // if we are traversing child nodes of a FORM we just need to add a goto to the child form.
  // The IFR opcodes for the form itself will be created later.
  HiiCreateGotoOpCode (
    StartOpCodeHandle,
    FormId,
    PromptStringId,
    EMPTY_STRING_ID,
    EFI_IFR_FLAG_CALLBACK,
    FormId
    );

  if (PromptVisibleExpression) {
    // close EFI_IFR_SUPRESS_IF_OP scope
    TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
    ASSERT (TempHiiBuffer != NULL);
  }
  return EFI_SUCCESS;
}

typedef struct {
  VOID    *OptionOpCodeHandle;
  VOID    *FormSetOpCodeHandle;
  VOID    *FormOpCodeHandle;
  UINT16   FormId;
  BUFFER  *StrsBuf;
} FORM_STATE;

//
// Iterate over CFR_TAG_ENUM_VALUE fields and invoke HiiCreateOneOfOptionOpCode for each.
//
STATIC
EFI_STATUS
EFIAPI
CfrEnumCallback  (
  IN CONST CFR_HEADER  *Root,
  IN CONST CFR_HEADER  *Child,
  IN OUT   VOID        *Private
  )
{
  CFR_ENUM_VALUE *CfrEnumValue;
  UINT8          *TempHiiBuffer;

  if ((Root == NULL) || (Child == NULL) || (Private == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Child->Tag != CFR_TAG_ENUM_VALUE) {
    return EFI_SUCCESS;
  }

  CfrEnumValue      = (CFR_ENUM_VALUE *)Child;
  FORM_STATE *State = Private;

  ASSERT (State->OptionOpCodeHandle != NULL);
  CONST CHAR8 *Prompt = CfrExtractString(CfrEnumValue->Prompt);
  EFI_STRING_ID PromptId; 
  if (Prompt) {
    PromptId = AddHiiStringBlock(State->StrsBuf, EFI_HII_SIBT_STRING_SCSU, Prompt);
  } else {
    PromptId = EMPTY_STRING_ID;
  }

  TempHiiBuffer = HiiCreateOneOfOptionOpCode (
                    State->OptionOpCodeHandle,
                    PromptId,
                    0,
                    EFI_IFR_TYPE_NUM_SIZE_32,
                    (UINT32)CfrEnumValue->Value
                    );
  ASSERT (TempHiiBuffer != NULL);
  return EFI_SUCCESS;
}

/**
  Process one CFR numeric option and create HII component.

**/
STATIC
EFI_STATUS
EFIAPI
CfrProcessOption (
  IN CFR_OPTION *Option,
  IN VOID       *FormSetOpCodeHandle,
  IN VOID       *FormOpCodeHandle,
  IN BUFFER     *StrsBuf
  )
{
  UINTN       QuestionIdVarStoreId;
  UINT8       QuestionFlags;
  VOID        *DefaultOpCodeHandle;
  UINT8       *TempHiiBuffer;
  VOID        *OptionOpCodeHandle;
  EFI_STATUS  Status;

  DEBUG ((
    DEBUG_INFO,
    "CFR: Process option[%llx] of size 0x%x\n",
    Option->ObjectId,
    Option->Size
    ));

  //
  // Processing start
  //
  
  // option name
  CONST CHAR8 *OptionName = CfrExtractString (Option->Name);
  ASSERT (OptionName != NULL);
  //
  // prompt
  //
  BOOLEAN Condition = FALSE;
  CFR_EXPRESSION *PromptVisibleExpression = NULL;
  EFI_STRING_ID PromptStringId = EMPTY_STRING_ID;
  CFR_PROMPT *PromptOption = NULL;
  EFI_STATUS PromptStatus = CfrFindTag ((UINT8 *)Option, CFR_TAG_PROMPT, (CONST VOID **)&PromptOption);
  if (PromptStatus == EFI_SUCCESS && PromptOption) {
    // option has a prompt
    CONST CHAR8 *Prompt = CfrExtractString (PromptOption->Prompt);
    ASSERT (Prompt != NULL);
    EFI_STATUS PromptVisibleStatus = CfrFindTag ((UINT8 *)PromptOption, CFR_TAG_PROMPT_VISIBLE, (CONST VOID **)&PromptVisibleExpression);
    if (PromptVisibleStatus == EFI_SUCCESS && PromptVisibleExpression) {
      // options prompt is only visible depending on a visible expression
      CfrProduceHiiIf(FormOpCodeHandle, StrsBuf, EFI_IFR_GRAY_OUT_IF_OP, PromptVisibleExpression);
      Condition = TRUE;
    } else if (Option->Flags & CFR_OPTFLAG_VOLATILE) {
      // option is always visible, but read only since it is volatile. Always show a grayout version of the option
      CfrProduceHiiIf(FormOpCodeHandle, StrsBuf, EFI_IFR_GRAY_OUT_IF_OP, NULL);
      Condition = TRUE;
    }
    PromptStringId = AddHiiStringBlock(StrsBuf, EFI_HII_SIBT_STRING_SCSU, Prompt);;
  } else {
    // suppress always if there is no prompt (no sense in showing an empty prompt)
    CfrProduceHiiIf(FormOpCodeHandle, StrsBuf, EFI_IFR_SUPPRESS_IF_OP, NULL);
    Condition = TRUE;
  }

  //
  // storage
  //
  QuestionIdVarStoreId = Option->ObjectId;
  Status = CfrProduceStorageForOption (OptionName, Option, FormSetOpCodeHandle, QuestionIdVarStoreId, Option->Tag);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // helptext
  //
  CONST CHAR8 *HelpText = CfrExtractString(Option->Help);
  EFI_STRING_ID HiiHelpTextId; 
  if (HelpText) {
    HiiHelpTextId = AddHiiStringBlock(StrsBuf, EFI_HII_SIBT_STRING_SCSU, HelpText);
  } else {
    HiiHelpTextId = EMPTY_STRING_ID;
  }

  QuestionFlags = EFI_IFR_FLAG_RESET_REQUIRED;
  if (Option->Flags & CFR_OPTFLAG_VOLATILE) {
    QuestionFlags |= EFI_IFR_FLAG_READ_ONLY;
  }

  //
  // create IFR DEFAULT opcodes
  //
  DefaultOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (DefaultOpCodeHandle != NULL);
  CFR_DEFAULT *Default;
  EFI_STATUS DefaultStatus = CfrFindTag ((UINT8 *)Option, CFR_TAG_DEFAULT, (CONST VOID **)&Default);
  if (DefaultStatus == EFI_SUCCESS) {
    CfrProduceHiiDefault(DefaultOpCodeHandle, StrsBuf, (CFR_HEADER *)Option, Default);
  }

  //
  // Create final HII opcode for given option
  //
  OptionOpCodeHandle = NULL;
  if (Option->Tag == CFR_TAG_OPTION_ENUM) {

    OptionOpCodeHandle = HiiAllocateOpCodeHandle ();
    ASSERT (OptionOpCodeHandle != NULL);

    FORM_STATE State = {
      .OptionOpCodeHandle = OptionOpCodeHandle,
      .FormId = 0,
      .StrsBuf = StrsBuf,
    };
    Status = CfrWalkTree ((UINT8 *)Option, CfrEnumCallback, &State);
    if (EFI_ERROR (Status)) {
      HiiFreeOpCodeHandle (OptionOpCodeHandle);
      return Status;
    }

    TempHiiBuffer = HiiCreateOneOfOpCode (
                      FormOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      PromptStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      EFI_IFR_NUMERIC_SIZE_4,
                      OptionOpCodeHandle,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else if (Option->Tag == CFR_TAG_OPTION_NUM_32) {
    TempHiiBuffer = HiiCreateNumericOpCode (
                      FormOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      PromptStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      EFI_IFR_NUMERIC_SIZE_4 | EFI_IFR_DISPLAY_UINT_DEC,
                      0x0000000000000000,
                      0xFFFFFFFFFFFFFFFF,
                      0,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else if (Option->Tag == CFR_TAG_OPTION_NUM_64) {
    TempHiiBuffer = HiiCreateNumericOpCode (
                      FormOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      PromptStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      EFI_IFR_NUMERIC_SIZE_8 | EFI_IFR_DISPLAY_UINT_DEC,
                      0x00000000,
                      0xFFFFFFFF,
                      0,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else if (Option->Tag == CFR_TAG_OPTION_BOOL) {
    TempHiiBuffer = HiiCreateCheckBoxOpCode (
                      FormOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      PromptStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      0,
                      DefaultOpCodeHandle 
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else if (Option->Tag == CFR_TAG_OPTION_STRING) {
    TempHiiBuffer = HiiCreateStringOpCode (
                      FormOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      PromptStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      0,
                      0,
                      CFR_STRING_MAX_SIZE,
                      DefaultOpCodeHandle
                      );
  }

  if (Condition == TRUE) {
    // close if condition scope (EFI_IFR_SUPRESS_IF_OP or EFI_IFR_GRAYOUT_IF_OP)
    TempHiiBuffer = HiiCreateEndOpCode (FormOpCodeHandle);
    ASSERT (TempHiiBuffer != NULL);
  }

  if (OptionOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (OptionOpCodeHandle);
  }

  if (DefaultStatus == EFI_SUCCESS) {
    HiiFreeOpCodeHandle (DefaultOpCodeHandle);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
CfrProcessComment (
  IN CFR_COMMENT *Option,
  IN VOID        *StartOpCodeHandle,
  IN BUFFER      *StrsBuf
  )
{
  //
  // Check if the comments prompt is visible
  //
  CFR_PROMPT *PromptOption;
  EFI_STATUS PromptStatus = CfrFindTag ((UINT8 *)Option, CFR_TAG_PROMPT, (CONST VOID **)&PromptOption);
  ASSERT(PromptStatus == EFI_SUCCESS); // prompt is required for a comment
  CONST CHAR8 *Prompt = CfrExtractString (PromptOption->Prompt);
  ASSERT (Prompt != NULL);
  CFR_EXPRESSION *PromptVisibleExpression = NULL;
  EFI_STATUS PromptVisibleStatus = CfrFindTag ((UINT8 *)PromptOption, CFR_TAG_PROMPT_VISIBLE, (CONST VOID **)&PromptVisibleExpression);
  if (PromptVisibleStatus == EFI_SUCCESS && PromptVisibleExpression) {
    CfrProduceHiiIf(StartOpCodeHandle, StrsBuf, EFI_IFR_SUPPRESS_IF_OP, PromptVisibleExpression);
  }
  EFI_STRING_ID PromptStringId = AddHiiStringBlock(StrsBuf, EFI_HII_SIBT_STRING_SCSU, Prompt);

  //
  // Create HII opcodes, processing complete.
  //
  UINT8 *TempHiiBuffer = HiiCreateTextOpCode (
                           StartOpCodeHandle,
                           PromptStringId,
                           EMPTY_STRING_ID,
                           EMPTY_STRING_ID
                           );
  ASSERT (TempHiiBuffer != NULL);

  if (PromptVisibleExpression) {
    // close EFI_IFR_SUPRESS_IF_OP scope
    TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
    ASSERT (TempHiiBuffer != NULL);
  }

  return EFI_SUCCESS;
}

//
// Iterate over CFR_FORM fields and generate Hii data for each child.
//
STATIC
EFI_STATUS
EFIAPI
CfrFormCallback  (
  IN CONST CFR_HEADER  *Root,
  IN CONST CFR_HEADER  *Child,
  IN OUT   VOID        *Private
  )
{
  EFI_STATUS     Status;
  UINT16         FormId;

  if ((Root == NULL) || (Child == NULL) || (Private == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  FORM_STATE *State = Private;
  VOID *FormSetOpCodeHandle = State->FormSetOpCodeHandle;
  VOID *FormOpCodeHandle = State->FormOpCodeHandle;

  if (Child->Tag == CFR_TAG_FORM) {
    State->FormId++; // increase FormId for the caller
    FormId = State->FormId;
    return CfrProduceHiiGotoForm ((CFR_FORM *)Child, FormOpCodeHandle, State->StrsBuf, FormId);
  }

  if (Child->Tag == CFR_TAG_PROMPT) {
    // Skip Form attributes (we only care about the childrens of the Form here)
    return EFI_SUCCESS;
  }

  switch (Child->Tag) {
    case CFR_TAG_OPTION_BOOL:
    case CFR_TAG_OPTION_ENUM:
    case CFR_TAG_OPTION_NUM_32:
    case CFR_TAG_OPTION_NUM_64:
    case CFR_TAG_OPTION_STRING:
      Status = CfrProcessOption ( (CFR_OPTION *)Child, FormSetOpCodeHandle, FormOpCodeHandle, State->StrsBuf);
      break;
    case CFR_TAG_COMMENT:
      Status = CfrProcessComment ((CFR_COMMENT *)Child, FormOpCodeHandle, State->StrsBuf);
      break;
    default:
      DEBUG ((
        DEBUG_ERROR,
        "CFR: Offset 0x%llx - Unexpected entry 0x%x (size 0x%x)!\n",
        (UINTN)((UINT8 *)Child - (UINT8 *)Root),
        Child->Tag,
        Child->Size
        ));
      break;
  }

  ASSERT(Status == EFI_SUCCESS);

  return EFI_SUCCESS;
}

VOID
ProcessFormsRecursive (
  IN CFR_FORM *Form,
  IN UINT16 FormId,
  IN VOID *FormSetOpCodeHandle,
  IN BUFFER *StrsBuf
  )
{
  //
  // Get Prompt of Form from CFR and hide/suppress Form if it has no prompt (aka not configurable)
  // TODO it may be enough to suppress the Goto Opcode only.
  //
  CFR_PROMPT *PromptOption;
  EFI_STATUS PromptStatus = CfrFindTag ((UINT8 *)Form, CFR_TAG_PROMPT, (CONST VOID **)&PromptOption);
  ASSERT(PromptStatus == EFI_SUCCESS); // prompt is required for a form
  CONST CHAR8 *Prompt = CfrExtractString (PromptOption->Prompt);
  ASSERT (Prompt != NULL);
  CFR_EXPRESSION *PromptVisibleExpression = NULL;
  EFI_STATUS PromptVisibleStatus = CfrFindTag ((UINT8 *)PromptOption, CFR_TAG_PROMPT_VISIBLE, (CONST VOID **)&PromptVisibleExpression);
  if (PromptVisibleStatus == EFI_SUCCESS && PromptVisibleExpression) {
    CfrProduceHiiIf(FormSetOpCodeHandle, StrsBuf, EFI_IFR_SUPPRESS_IF_OP, PromptVisibleExpression);
  }
  EFI_STRING_ID PromptStringId = AddHiiStringBlock(StrsBuf, EFI_HII_SIBT_STRING_SCSU, Prompt);;

  //
  // Create IFR Form
  //
  VOID *FormOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (FormOpCodeHandle != NULL);
  EFI_IFR_FORM IfrForm;
  IfrForm.Header.OpCode = EFI_IFR_FORM_OP;
  IfrForm.Header.Length = sizeof(IfrForm);
  IfrForm.Header.Scope = 1;
  IfrForm.FormId = FormId; // Produce default for type
  IfrForm.FormTitle = PromptStringId;
  UINT8 *TempHiiBuffer = HiiCreateRawOpCodes (
                                 FormOpCodeHandle,
                                 (UINT8 *)&IfrForm,
                                 sizeof (IfrForm)
                                 );
  ASSERT (TempHiiBuffer != NULL);

  FORM_STATE Private = {
      .FormSetOpCodeHandle = FormSetOpCodeHandle,
      .FormOpCodeHandle = FormOpCodeHandle,
      .FormId = FormId,
      .StrsBuf = StrsBuf
  };
  EFI_STATUS Status = CfrWalkTree ((UINT8 *)Form, CfrFormCallback, &Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CFR: Error walking form: %r\n", Status));
  }

  // close Form scope
  TempHiiBuffer = HiiCreateEndOpCode (FormOpCodeHandle);
  ASSERT (TempHiiBuffer != NULL);

  //
  // append all opcodes that are part of the Form to the FormSet opcode
  //
  HII_LIB_OPCODE_BUFFER *RawFormOpCodeBuffer = (HII_LIB_OPCODE_BUFFER *)FormOpCodeHandle;
  TempHiiBuffer = HiiCreateRawOpCodes (FormSetOpCodeHandle, RawFormOpCodeBuffer->Buffer, RawFormOpCodeBuffer->Position);
  ASSERT (TempHiiBuffer != NULL);
  HiiFreeOpCodeHandle (FormOpCodeHandle);
  UINTN processed = sizeof(CFR_FORM);
  while (processed < Form->Size) {
    CFR_HEADER *Child = (CFR_HEADER *)((UINTN)Form + processed);
    if (Child->Tag == CFR_TAG_FORM) {
      FormId++;
      ProcessFormsRecursive((CFR_FORM *)Child, FormId, FormSetOpCodeHandle, StrsBuf);
    }
    processed += Child->Size;
  }
}

VOID
ProcessFormSet (
  IN VOID *FormSetOpCodeHandle,
  IN CFR_FORM *Form,
  IN BUFFER *StrsBuf
  )
{
  // Get Prompt of first/root Form from CFR TODO not sure if there is a use case to hide the root form
  CFR_PROMPT *PromptOption;
  EFI_STATUS PromptStatus = CfrFindTag ((UINT8 *)Form, CFR_TAG_PROMPT, (CONST VOID **)&PromptOption);
  ASSERT(PromptStatus == EFI_SUCCESS); // prompt is required for a form
  CONST CHAR8 *Prompt = CfrExtractString (PromptOption->Prompt);
  ASSERT (Prompt != NULL);
  CFR_EXPRESSION *PromptVisibleExpression = NULL;
  EFI_STATUS PromptVisibleStatus = CfrFindTag ((UINT8 *)PromptOption, CFR_TAG_PROMPT_VISIBLE, (CONST VOID **)&PromptVisibleExpression);
  if (PromptVisibleStatus == EFI_SUCCESS && PromptVisibleExpression) {
    CfrProduceHiiIf(FormSetOpCodeHandle, StrsBuf, EFI_IFR_SUPPRESS_IF_OP, PromptVisibleExpression);
  }
  EFI_STRING_ID PromptStringId = AddHiiStringBlock(StrsBuf, EFI_HII_SIBT_STRING_SCSU, Prompt);
  
  //
  // Create IFR Formset
  //
  UINT8 IfrFormSetBuf[sizeof(EFI_IFR_FORM_SET) + sizeof(EFI_GUID)];
  EFI_IFR_FORM_SET *IfrFormSet = (EFI_IFR_FORM_SET *)IfrFormSetBuf;
  IfrFormSet->Header.OpCode = EFI_IFR_FORM_SET_OP;
  IfrFormSet->Header.Length = sizeof(*IfrFormSet) + sizeof(EFI_GUID);
  IfrFormSet->Header.Scope = 1;
  IfrFormSet->Guid = mSetupMenuHiiGuid;
  IfrFormSet->FormSetTitle = PromptStringId;
  IfrFormSet->Help = EMPTY_STRING_ID;
  IfrFormSet->Flags = 1; //TODO
  EFI_GUID *ClassGuid = (EFI_GUID *)&IfrFormSetBuf[sizeof(EFI_IFR_FORM_SET)];
  CopyGuid(ClassGuid, &gEfiHiiPlatformSetupFormsetGuid);
  UINT8 *TempHiiBuffer = HiiCreateRawOpCodes (
                                 FormSetOpCodeHandle,
                                 IfrFormSetBuf,
                                 sizeof (*IfrFormSet)  + sizeof(EFI_GUID)
                                 );
  ASSERT (TempHiiBuffer != NULL);
  
  //
  // create DEFAULTSTORE opcode
  //
  EFI_IFR_DEFAULTSTORE IfrDefaultStore;
  IfrDefaultStore.Header.OpCode = EFI_IFR_DEFAULTSTORE_OP;
  IfrDefaultStore.Header.Length = sizeof(IfrDefaultStore);
  IfrDefaultStore.Header.Scope = 1;
  IfrDefaultStore.DefaultName = 0;
  IfrDefaultStore.DefaultId = EFI_HII_DEFAULT_CLASS_STANDARD;
  TempHiiBuffer = HiiCreateRawOpCodes (
                                 FormSetOpCodeHandle,
                                 (UINT8 *)&IfrDefaultStore,
                                 sizeof (IfrDefaultStore)
                                 );
  ASSERT (TempHiiBuffer != NULL);

  ProcessFormsRecursive(Form, 0, FormSetOpCodeHandle, StrsBuf);

  // close FormSet scope
  TempHiiBuffer = HiiCreateEndOpCode (FormSetOpCodeHandle);
  ASSERT (TempHiiBuffer != NULL);
}

//
// Takes an ASCII string 'str' and concatenates it into the StrsBuf,
// which is later used to create the HII strings package.
//
EFI_STRING_ID
AddHiiStringBlock (
  OUT       BUFFER *StrsBuf,
  IN        UINT8   BlockType,
  IN  CONST CHAR8  *Str
  )
{
  UINTN StrSize = 0;
  if (Str)
    StrSize = AsciiStrLen(Str) + 1;
  StrsBuf->Data = ReallocatePool (
                    StrsBuf->Size,
                    StrsBuf->Size + sizeof(EFI_HII_STRING_BLOCK) + StrSize,
                    StrsBuf->Data
                    );
  EFI_HII_STRING_BLOCK *Block = (EFI_HII_STRING_BLOCK *)(StrsBuf->Data + StrsBuf->Size);
  Block->BlockType = BlockType;
  StrsBuf->Size += sizeof(EFI_HII_STRING_BLOCK);
  if (Str) {
    AsciiStrCpyS(StrsBuf->Data + StrsBuf->Size, StrSize, Str);
    StrsBuf->Size += StrSize;
  }
  StrsBuf->CountStrs++;
  return StrsBuf->CountStrs;
}

/*
 * Tries to fetch the value of a given option (GetVariable).
 * If the option is not found it will set the underlying storage (SetVariable),
 * to the default value given by CFR.
 */
STATIC
EFI_STATUS
OptionSetStorage (
  IN  CFR_OPTION  *Option
  )
{
  UINT64 Num64;
  UINT32 Num32;
  UINTN DataSize;
  VOID *Value = NULL;
  EFI_STATUS Status;
  switch (Option->Tag) {
    case CFR_TAG_OPTION_NUM_64:
      Status = OptionEvaluateUint64Value(Option, &Num64);
      DataSize = 8;
      Value = &Num64;
      break;
    case CFR_TAG_OPTION_BOOL:
    case CFR_TAG_OPTION_ENUM:
    case CFR_TAG_OPTION_NUM_32:
      Status = OptionEvaluateUint64Value(Option, &Num64);
      DataSize = 4;
      Num32 = (UINT32)Num64;
      Value = &Num32;
      break;
    case CFR_TAG_OPTION_STRING:
      Status = OptionEvaluateStringValue(Option, &DataSize, (CHAR16 **)&Value);
      break;
  }
  if (Status == EFI_NOT_FOUND) {
    // 
    // The value was not found in the underlying storage (e.g. SMMSTORE)
    // 'Value' is set to the default value specified by CFR.
    // Now we just need to set the underlying storage value to the default.
    //

    // get option name
    CONST CHAR8 *OptionName = CfrExtractString (Option->Name);
    ASSERT (OptionName != NULL);
    // convert to UTF-16
    UINTN CfrStringSize = AsciiStrSize(OptionName);
    CHAR16 *VariableName = AllocatePool (CfrStringSize * sizeof (CHAR16));
    ASSERT (VariableName != NULL);
    Status = AsciiStrToUnicodeStrS (OptionName, VariableName, CfrStringSize);
    ASSERT (Status == EFI_SUCCESS);

    UINT32 VariableAttributes = EFI_VARIABLE_BOOTSERVICE_ACCESS;
    if (!(Option->Flags & CFR_OPTFLAG_VOLATILE)) {
      VariableAttributes |= EFI_VARIABLE_NON_VOLATILE;
    }

    Status = gRT->SetVariable (VariableName, &gEfiCfrNvDataGuid, VariableAttributes, DataSize, Value);

    FreePool(VariableName);
  }
  if (Option->Tag == CFR_TAG_OPTION_STRING)
    FreePool(Value);

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
CfrSetDefaultOptionCallback (
    IN CONST CFR_HEADER  *Root,
    IN CONST CFR_HEADER  *Child,
    IN OUT   VOID        *Private
  )
{
  if (Child->Tag == CFR_TAG_FORM) {
    CfrWalkTree ((UINT8 *)Child, CfrSetDefaultOptionCallback, NULL);
    return EFI_SUCCESS;
  }

  if (Child->Tag != CFR_TAG_OPTION_BOOL
   && Child->Tag != CFR_TAG_OPTION_NUM_32
   && Child->Tag != CFR_TAG_OPTION_NUM_64
   && Child->Tag != CFR_TAG_OPTION_STRING
   && Child->Tag != CFR_TAG_OPTION_ENUM
   ) {
    return EFI_SUCCESS;
  }

  CFR_OPTION *CfrOption = (CFR_OPTION *)Child;
  
  OptionSetStorage(CfrOption);

  return EFI_SUCCESS;
}

VOID
CfrSetDefaultOptions (
    IN CFR_FORM *CfrForm
  )
{
  CfrWalkTree ((UINT8 *)CfrForm, CfrSetDefaultOptionCallback, NULL);
}
