#include "osmath.h"

static const char* input;
static uint32_t pos = 0;

static char current() {
    return input[pos];
}

static void skipSpaces() {
    while (input[pos] == ' ') pos++;
}

static uint32_t parseNumber() {
    uint32_t value = 0;

    while (current() >= '0' && current() <= '9') {
        value = value * 10 + (current() - '0');
        pos++;
    }

    return value;
}

static uint32_t parseExpression();

static uint32_t parseFactor() {
    skipSpaces();

    if (current() == '(') {
        pos++; // '('
        uint32_t value = parseExpression();
        skipSpaces();
        if (current() == ')') pos++;
        return value;
    }

    return parseNumber();
}

static uint32_t parseTerm() {
    uint32_t value = parseFactor();

    while (1) {
        skipSpaces();
        char op = current();

        if (op != '*' && op != '/') break;

        pos++;
        uint32_t rhs = parseFactor();

        if (op == '*') value *= rhs;
        else value /= rhs;
    }

    return value;
}

static uint32_t parseExpression() {
    uint32_t value = parseTerm();

    while (1) {
        skipSpaces();
        char op = current();

        if (op != '+' && op != '-') break;

        pos++;
        uint32_t rhs = parseTerm();

        if (op == '+') value += rhs;
        else value -= rhs;
    }

    return value;
}

uint32_t evaluate(const char* expr) {
    input = expr;
    pos = 0;
    return parseExpression();
}