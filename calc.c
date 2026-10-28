// Omer Faruk Boyraz 241ADB161
// https://github.com/Omer-FarukBoyraz/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm

#include <stdio.h>
#include <stdlib.h>

char *p;

void skip_spaces(void)
{
    while (*p == ' ' || *p == '\t')
        p++;
}

long long expr(void);

long long primary(void)
{
    long long result;

    skip_spaces();

    if (*p == '(') {
        p++;
        result = expr();
        skip_spaces();

        if (*p == ')')
            p++;

        return result;
    }

    return strtoll(p, &p, 10);
}

long long term(void)
{
    long long result = primary();

    while (1) {
        skip_spaces();

        if (*p == '*') {
            p++;
            result *= primary();
        }
        else if (*p == '/') {
            p++;
            long long n = primary();

            if (n != 0)
                result /= n;
        }
        else {
            break;
        }
    }

    return result;
}

long long expr(void)
{
    long long result = term();

    while (1) {
        skip_spaces();

        if (*p == '+') {
            p++;
            result += term();
        }
        else if (*p == '-') {
            p++;
            result -= term();
        }
        else {
            break;
        }
    }

    return result;
}

int main(int argc, char *argv[])
{
    FILE *file;
    char line[256];

    if (argc < 2) {
        fprintf(stderr, "Missing input file\n");
        return 1;
    }

    file = fopen(argv[1], "r");

    if (file == NULL) {
        fprintf(stderr, "Cannot open input file\n");
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        p = line;
        skip_spaces();

        if (*p == '\0' || *p == '\n')
            continue;

        printf("%lld\n", expr());
    }

    fclose(file);
    return 0;
}