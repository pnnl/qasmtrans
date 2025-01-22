#pragma once

#include <iostream>
#include <stack>
#include <queue>
#include <cmath>
#include "lexer.hpp"
#include "../QASMTransPrimitives.hpp"

namespace QASMTrans {

using namespace std;
using namespace lexertk;

/**
 * @brief Returns an integer precedence level for each operator.
 * Higher number => higher precedence.
 */
inline int get_precedence(token::token_type op)
{
    switch (op)
    {
        case token::e_add:
        case token::e_sub:
            return 1;
        case token::e_mul:
        case token::e_div:
            return 2;
        case token::e_pow:
            return 3; // highest precedence
        case token::e_negative:
            return 4; // unary minus => even higher
        default:
            return -1; // not an operator
    }
}

/**
 * @brief Compare the precedence of two operators.
 * Returns:
 *   1 if op1 has higher precedence than op2,
 *  -1 if op1 has lower precedence than op2,
 *   0 if equal precedence.
 */
inline int compare_operators(token::token_type op1, token::token_type op2)
{
    int p1 = get_precedence(op1);
    int p2 = get_precedence(op2);

    if (p1 == -1 || p2 == -1)
    {
        throw std::runtime_error("compare_operators: Unrecognized operator");
    }
    if (p1 > p2)
        return 1;
    if (p1 < p2)
        return -1;
    return 0;
}

/**
 * @brief Check if a token is considered an operator in this parser.
 */
inline bool is_operator(token::token_type t)
{
    return (t == token::e_pow) ||
           (t == token::e_mul) ||
           (t == token::e_div) ||
           (t == token::e_add) ||
           (t == token::e_sub) ||
           (t == token::e_negative);
}

/**
 * @brief Parse a subset of tokens [start, end) using a Shunting Yard algorithm
 * and evaluate them to a double. 
 *
 * The tokens can include:
 *  - numbers (e_number)
 *  - pi constant (e_pi)
 *  - parentheses (e_lbracket, e_rbracket)
 *  - operators (+, -, *, /, ^, unary minus)
 *  - some functions (sin, cos, etc.)
 *
 * @throw std::runtime_error for malformed expressions or unrecognized operators.
 * @param tokens The full token array
 * @param start  Index of the first token to parse
 * @param end    One past the last token to parse
 * @return The numerical result
 */
inline double parse_expr(const std::vector<token> &tokens, int start, int end)
{
    if (start < 0 || end > static_cast<int>(tokens.size()) || start >= end)
    {
        throw std::runtime_error("parse_expr: invalid range [" +
                                 std::to_string(start) + "," +
                                 std::to_string(end) + ")");
    }

    std::stack<token> op_stack;
    std::queue<token> output_queue;

    // Convert to Reverse Polish Notation (RPN) via Shunting Yard
    for (int i = start; i < end; i++)
    {
        token t = tokens[i];

        switch (t.type)
        {
            case token::e_number:
            case token::e_pi:
                // Numbers and pi go directly to output
                output_queue.push(t);
                break;

            case token::e_func:
                // Function names are pushed to the op_stack
                op_stack.push(t);
                break;

            case token::e_lbracket:
                // Left bracket always pushed
                op_stack.push(t);
                break;

            case token::e_rbracket:
                // Pop from the stack to output until we find the matching left bracket
                while (!op_stack.empty() && op_stack.top().type != token::e_lbracket)
                {
                    output_queue.push(op_stack.top());
                    op_stack.pop();
                }
                if (op_stack.empty())
                {
                    throw std::runtime_error("parse_expr: mismatched parentheses");
                }
                // pop the left bracket
                op_stack.pop();

                // If top of stack is a function, pop it as well
                if (!op_stack.empty() && op_stack.top().type == token::e_func)
                {
                    output_queue.push(op_stack.top());
                    op_stack.pop();
                }
                break;

            case token::e_sub:
            {
                // Check if it's unary minus (e_negative)
                // Condition: This is unary if it's the first token OR
                // the previous token is not a number/pi/rbracket
                bool unary = (i == 0) ||
                             (tokens[i - 1].type != token::e_number &&
                              tokens[i - 1].type != token::e_pi &&
                              tokens[i - 1].type != token::e_rbracket);

                if (unary)
                {
                    // Convert to e_negative
                    t.type  = token::e_negative;
                    t.value = "NEG";
                    op_stack.push(t);
                    break; // go to next token
                }
                // else it's a normal minus => handle like +,*, etc.
            }
            // fallthrough
            case token::e_add:
            case token::e_mul:
            case token::e_div:
            case token::e_pow:
            {
                // pop operators with higher or equal precedence
                while (!op_stack.empty() && is_operator(op_stack.top().type))
                {
                    // If current op <= top of stack in precedence, pop
                    if (compare_operators(t.type, op_stack.top().type) <= 0)
                    {
                        output_queue.push(op_stack.top());
                        op_stack.pop();
                    }
                    else
                    {
                        break;
                    }
                }
                op_stack.push(t);
                break;
            }

            default:
                // Possibly a token::e_end, token::e_unknown, etc.
                cerr << "parse_expr: Unknown token type: "
                     << token::to_str(t.type) << " '" << t.value << "'\n";
                break;
        }
    }

    // Move any remaining ops to the output queue
    while (!op_stack.empty())
    {
        if (op_stack.top().type == token::e_lbracket ||
            op_stack.top().type == token::e_rbracket)
        {
            throw std::runtime_error("parse_expr: mismatched parentheses in stack");
        }
        output_queue.push(op_stack.top());
        op_stack.pop();
    }

    // Evaluate the RPN expression
    std::stack<double> val_stack;
    while (!output_queue.empty())
    {
        token t = output_queue.front();
        output_queue.pop();

        switch (t.type)
        {
            case token::e_number:
            {
                double num = std::stod(t.value);
                val_stack.push(num);
                break;
            }
            case token::e_pi:
            {
                val_stack.push(PI); // from QASMTransPrimitives.hpp
                break;
            }
            case token::e_add:
            case token::e_sub:
            case token::e_mul:
            case token::e_div:
            case token::e_pow:
            {
                // For binary ops, we pop val2 first, then val1
                // i.e. (val1 op val2).
                if (val_stack.size() < 1)
                {
                    throw std::runtime_error("parse_expr: binary operator with empty stack");
                }
                double val2 = val_stack.top(); val_stack.pop();

                double val1 = 0.0;
                if (!val_stack.empty())
                {
                    val1 = val_stack.top();
                    val_stack.pop();
                }

                if (t.type == token::e_add)
                    val_stack.push(val1 + val2);
                else if (t.type == token::e_sub)
                    val_stack.push(val1 - val2);
                else if (t.type == token::e_mul)
                    val_stack.push(val1 * val2);
                else if (t.type == token::e_div)
                    val_stack.push(val1 / val2);
                else if (t.type == token::e_pow)
                    val_stack.push(std::pow(val1, val2));
                break;
            }
            case token::e_func:
            case token::e_negative:
            {
                // unary operator or function => pop one value
                if (val_stack.empty())
                {
                    throw std::runtime_error("parse_expr: unary/function operator with empty stack");
                }
                double val = val_stack.top(); 
                val_stack.pop();

                if (t.type == token::e_negative)
                {
                    val_stack.push(-val);
                }
                else
                {
                    // function call
                    if (t.value == "cos")
                        val_stack.push(std::cos(val));
                    else if (t.value == "sin")
                        val_stack.push(std::sin(val));
                    else
                        cerr << "parse_expr: Unrecognized function '" << t.value << "'\n";
                }
                break;
            }
            default:
                cerr << "parse_expr: unrecognized token in RPN: "
                     << token::to_str(t.type) << " '" << t.value << "'\n";
                break;
        }
    }

    if (val_stack.empty())
    {
        throw std::runtime_error("parse_expr: no final result");
    }

    double result = val_stack.top();
    // If the expression had extra values, there's a risk we didn't pop everything
    // e.g., malformed expressions: "2 3 + 4"
    // We'll ignore that or you can throw a warning if there's more than one value in the stack.
    return result;
}

} // namespace QASMTrans