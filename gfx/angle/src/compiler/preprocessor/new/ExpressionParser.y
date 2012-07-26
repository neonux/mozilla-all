/*
//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

This file contains the Yacc grammar for GLSL ES preprocessor expression.

IF YOU MODIFY THIS FILE YOU ALSO NEED TO RUN generate_parser.sh,
WHICH GENERATES THE GLSL ES preprocessor expression parser.
*/

%{
//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// This file is auto-generated by generate_parser.sh. DO NOT EDIT!

#if defined(__GNUC__)
// Triggered by the auto-generated pplval variable.
#pragma GCC diagnostic ignored "-Wuninitialized"
#elif defined(_MSC_VER)
#pragma warning(disable: 4065 4701)
#endif

#include "ExpressionParser.h"

#include <cassert>
#include <sstream>

#include "Diagnostics.h"
#include "Lexer.h"
#include "Token.h"

#if defined(_MSC_VER)
typedef __int64 YYSTYPE;
#else
#include <stdint.h>
typedef intmax_t YYSTYPE;
#endif  // _MSC_VER
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1

namespace {
struct Context
{
    pp::Diagnostics* diagnostics;
    pp::Lexer* lexer;
    pp::Token* token;
    int* result;
};
}  // namespace
%}

%pure-parser
%name-prefix="pp"
%parse-param {Context *context}
%lex-param {Context *context}

%{
static int yylex(YYSTYPE* lvalp, Context* context);
static void yyerror(Context* context, const char* reason);
%}

%token TOK_CONST_INT
%left TOK_OP_OR
%left TOK_OP_AND
%left '|'
%left '^'
%left '&'
%left TOK_OP_EQ TOK_OP_NE
%left '<' '>' TOK_OP_LE TOK_OP_GE
%left TOK_OP_LEFT TOK_OP_RIGHT
%left '+' '-'
%left '*' '/' '%'
%right TOK_UNARY

%%

input
    : expression {
        *(context->result) = static_cast<int>($1);
        YYACCEPT;
    }
;

expression
    : TOK_CONST_INT
    | expression TOK_OP_OR expression {
        $$ = $1 || $3;
    }
    | expression TOK_OP_AND expression {
        $$ = $1 && $3;
    }
    | expression '|' expression {
        $$ = $1 | $3;
    }
    | expression '^' expression {
        $$ = $1 ^ $3;
    }
    | expression '&' expression {
        $$ = $1 & $3;
    }
    | expression TOK_OP_NE expression {
        $$ = $1 != $3;
    }
    | expression TOK_OP_EQ expression {
        $$ = $1 == $3;
    }
    | expression TOK_OP_GE expression {
        $$ = $1 >= $3;
    }
    | expression TOK_OP_LE expression {
        $$ = $1 <= $3;
    }
    | expression '>' expression {
        $$ = $1 > $3;
    }
    | expression '<' expression {
        $$ = $1 < $3;
    }
    | expression TOK_OP_RIGHT expression {
        $$ = $1 >> $3;
    }
    | expression TOK_OP_LEFT expression {
        $$ = $1 << $3;
    }
    | expression '-' expression {
        $$ = $1 - $3;
    }
    | expression '+' expression {
        $$ = $1 + $3;
    }
    | expression '%' expression {
        if ($3 == 0) {
            std::ostringstream stream;
            stream << $1 << " % " << $3;
            std::string text = stream.str();
            context->diagnostics->report(pp::Diagnostics::DIVISION_BY_ZERO,
                                         context->token->location,
                                         text.c_str());
            YYABORT;
        } else {
            $$ = $1 % $3;
        }
    }
    | expression '/' expression {
        if ($3 == 0) {
            std::ostringstream stream;
            stream << $1 << " / " << $3;
            std::string text = stream.str();
            context->diagnostics->report(pp::Diagnostics::DIVISION_BY_ZERO,
                                         context->token->location,
                                         text.c_str());
            YYABORT;
        } else {
            $$ = $1 / $3;
        }
    }
    | expression '*' expression {
        $$ = $1 * $3;
    }
    | '!' expression %prec TOK_UNARY {
        $$ = ! $2;
    }
    | '~' expression %prec TOK_UNARY {
        $$ = ~ $2;
    }
    | '-' expression %prec TOK_UNARY {
        $$ = - $2;
    }
    | '+' expression %prec TOK_UNARY {
        $$ = + $2;
    }
    | '(' expression ')' {
        $$ = $2;
    }
;

%%

int yylex(YYSTYPE* lvalp, Context* context)
{
    int type = 0;

    pp::Token* token = context->token;
    switch (token->type)
    {
      case pp::Token::CONST_INT:
      {
        unsigned int val = 0;
        if (!token->uValue(&val))
        {
            context->diagnostics->report(pp::Diagnostics::INTEGER_OVERFLOW,
                                         token->location, token->text);
        }
        *lvalp = static_cast<YYSTYPE>(val);
        type = TOK_CONST_INT;
        break;
      }
      case pp::Token::OP_OR: type = TOK_OP_OR; break;
      case pp::Token::OP_AND: type = TOK_OP_AND; break;
      case pp::Token::OP_NE: type = TOK_OP_NE; break;
      case pp::Token::OP_EQ: type = TOK_OP_EQ; break;
      case pp::Token::OP_GE: type = TOK_OP_GE; break;
      case pp::Token::OP_LE: type = TOK_OP_LE; break;
      case pp::Token::OP_RIGHT: type = TOK_OP_RIGHT; break;
      case pp::Token::OP_LEFT: type = TOK_OP_LEFT; break;
      case '|': type = '|'; break;
      case '^': type = '^'; break;
      case '&': type = '&'; break;
      case '>': type = '>'; break;
      case '<': type = '<'; break;
      case '-': type = '-'; break;
      case '+': type = '+'; break;
      case '%': type = '%'; break;
      case '/': type = '/'; break;
      case '*': type = '*'; break;
      case '!': type = '!'; break;
      case '~': type = '~'; break;
      case '(': type = '('; break;
      case ')': type = ')'; break;

      default: break;
    }

    // Advance to the next token if the current one is valid.
    if (type != 0) context->lexer->lex(token);

    return type;
}

void yyerror(Context* context, const char* reason)
{
    context->diagnostics->report(pp::Diagnostics::INVALID_EXPRESSION,
                                 context->token->location,
                                 reason);
}

namespace pp {

ExpressionParser::ExpressionParser(Lexer* lexer, Diagnostics* diagnostics) :
    mLexer(lexer),
    mDiagnostics(diagnostics)
{
}

bool ExpressionParser::parse(Token* token, int* result)
{
    Context context;
    context.diagnostics = mDiagnostics;
    context.lexer = mLexer;
    context.token = token;
    context.result = result;
    int ret = yyparse(&context);
    switch (ret)
    {
      case 0:
      case 1:
        break;

      case 2:
        mDiagnostics->report(Diagnostics::OUT_OF_MEMORY, token->location, "");
        break;

      default:
        assert(false);
        mDiagnostics->report(Diagnostics::INTERNAL_ERROR, token->location, "");
        break;
    }

    return ret == 0;
}

}  // namespace pp
