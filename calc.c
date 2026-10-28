// Omer Faruk Boyraz 241ADB161
// https://github.com/Omer-FarukBoyraz/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct { double f; long long i; int is_float; } Value;
typedef struct { const char *p, *end, *base; size_t error; } Parser;

typedef struct { char *data; size_t len, cap; } Buffer;

static int reserve(Buffer *b, size_t extra) {
    if (extra > (size_t)-1 - b->len - 1) return 0;
    size_t needed = b->len + extra + 1;
    if (needed <= b->cap) return 1;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < needed) {
        if (cap > (size_t)-1 / 2) { cap = needed; break; }
        cap *= 2;
    }
    char *next = realloc(b->data, cap);
    if (!next) return 0;
    b->data = next; b->cap = cap;
    return 1;
}
static int append(Buffer *b, const char *s, size_t n) {
    if (!reserve(b, n)) return 0;
    memcpy(b->data + b->len, s, n);
    b->len += n; b->data[b->len] = '\0';
    return 1;
}
// All parser pointers refer to the original file buffer; subtraction gives the
// 1-based file position, including previous lines and their newlines.
static void spaces(Parser *s) { while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' || *s->p == '\r')) s->p++; }
// The current token is blamed; at end of line p points to its newline
// (or one past the final character if no terminating newline exists).
static void fail(Parser *s, const char *at) { if (!s->error) s->error = (size_t)(at - s->base) + 1; }
static Value expr(Parser *s);
static Value make_int(long long n) { Value v = {0.0, n, 0}; return v; }
static Value make_float(double n) { Value v = {n, 0, 1}; return v; }
static double as_double(Value v) { return v.is_float ? v.f : (double)v.i; }

static Value primary(Parser *s) {
    spaces(s);
    if (s->p >= s->end) { fail(s, s->p); return make_int(0); }
    if (*s->p == '(') {
        s->p++;
        Value v = expr(s);
        if (s->error) return v;
        spaces(s);
        if (s->p >= s->end || *s->p != ')') { fail(s, s->p); return v; }
        s->p++;
        return v;
    }
    if (!isdigit((unsigned char)*s->p)) { fail(s, s->p); return make_int(0); }
    const char *begin = s->p;
    while (s->p < s->end && isdigit((unsigned char)*s->p)) s->p++;
    if (s->p < s->end && *s->p == '.' && s->p + 1 < s->end && isdigit((unsigned char)s->p[1])) {
        s->p++;
        while (s->p < s->end && isdigit((unsigned char)*s->p)) s->p++;
        return make_float(strtod(begin, NULL));
    }
    return make_int(strtoll(begin, NULL, 10));
}
// AI (ChatGPT): prompt "implement right-associative power in a C parser".
// Recursion on the right makes 2**3**2 equivalent to 2**(3**2).
static Value power(Parser *s) {
    Value a = primary(s);
    if (s->error) return a;
    spaces(s);
    if (s->end - s->p >= 2 && s->p[0] == '*' && s->p[1] == '*') {
        const char *op = s->p;
        s->p += 2;
        spaces(s);
        const char *rhs = s->p;
        Value b = power(s);
        if (s->error) return a;
        if (!b.is_float && b.i < 0 && !a.is_float) { fail(s, rhs); return a; }
        if (!a.is_float && !b.is_float) {
            long long result = 1, base = a.i, exp = b.i;
            while (exp > 0) { if (exp & 1) result *= base; exp >>= 1; if (exp) base *= base; }
            return make_int(result);
        }
        double n = pow(as_double(a), as_double(b));
        if (!isfinite(n)) { fail(s, op); return a; }
        return make_float(n);
    }
    return a;
}
// Separate grammar levels enforce power > multiply/divide > add/subtract.
// Float arithmetic is used if either operand is floating point; otherwise
// operations remain in long long (including truncating division).
static Value term(Parser *s) {
    Value a = power(s);
    while (!s->error) {
        spaces(s);
        if (s->p >= s->end || (*s->p != '*' && *s->p != '/')) break;
        if (*s->p == '*' && s->end - s->p >= 2 && s->p[1] == '*') break;
        const char *op_at = s->p;
        char op = *s->p++;
        spaces(s);
        const char *rhs = s->p;
        Value b = power(s);
        if (s->error) break;
        if (op == '/' && (b.is_float ? b.f == 0.0 : b.i == 0)) { fail(s, rhs); break; }
        if (a.is_float || b.is_float) {
            double result = op == '*' ? as_double(a) * as_double(b) : as_double(a) / as_double(b);
            if (!isfinite(result)) { fail(s, op_at); break; }
            a = make_float(result);
        } else a = make_int(op == '*' ? a.i * b.i : a.i / b.i);
    }
    return a;
}
static Value expr(Parser *s) {
    Value a = term(s);
    while (!s->error) {
        spaces(s);
        if (s->p >= s->end || (*s->p != '+' && *s->p != '-')) break;
        const char *op_at = s->p;
        char op = *s->p++;
        Value b = term(s);
        if (s->error) break;
        if (a.is_float || b.is_float) {
            double result = op == '+' ? as_double(a) + as_double(b) : as_double(a) - as_double(b);
            if (!isfinite(result)) { fail(s, op_at); break; }
            a = make_float(result);
        } else a = make_int(op == '+' ? a.i + b.i : a.i - b.i);
    }
    return a;
}

// Geometrically grown buffers avoid repeated full-file copies or line limits.
// Results stay in memory until the first error has been ruled out.
static int process(const char *input, const char *output) {
    FILE *in = fopen(input, "rb");
    if (!in) { fprintf(stderr, "Cannot open %s\n", input); return 0; }
    Buffer data = {0}, results = {0};
    char block[8192]; size_t n;
    while ((n = fread(block, 1, sizeof(block), in)) != 0) {
        if (!append(&data, block, n)) { fprintf(stderr, "Out of memory\n"); goto error; }
    }
    if (ferror(in)) { fprintf(stderr, "Cannot read %s\n", input); goto error; }
    fclose(in); in = NULL;
    if (!reserve(&data, 0)) goto error;
    data.data[data.len] = '\0';
    const char *at = data.data, *finish = at + data.len;
    size_t first_error = 0;
    while (at < finish) {
        const char *end = memchr(at, '\n', (size_t)(finish - at));
        if (!end) end = finish;
        Parser p = {at, end, data.data, 0};
        spaces(&p);
        if (p.p != end && *p.p != '#') {
            Value v = expr(&p);
            if (!p.error) { spaces(&p); if (p.p != end) fail(&p, p.p); }
            if (p.error) { first_error = p.error; break; }
            char line[128];
            int size = v.is_float ? snprintf(line, sizeof(line), "%.15g\n", v.f) : snprintf(line, sizeof(line), "%lld\n", v.i);
            if (size < 0 || !append(&results, line, (size_t)size)) goto error;
        }
        at = end == finish ? finish : end + 1;
    }
    FILE *out = fopen(output, "wb");
    if (!out) { fprintf(stderr, "Cannot write %s\n", output); goto error; }
    int ok;
    if (first_error) ok = fprintf(out, "ERROR:%zu\n", first_error) > 0;
    else ok = results.len == 0 || fwrite(results.data, 1, results.len, out) == results.len;
    if (fclose(out) != 0) ok = 0;
    free(data.data); free(results.data);
    if (!ok) fprintf(stderr, "Cannot write %s\n", output);
    return ok;
error:
    if (in) fclose(in);
    free(data.data); free(results.data);
    return 0;
}
static const char *basename_part(const char *path) {
    const char *last = path;
    for (const char *p = path; *p; p++) if (*p == '/') last = p + 1;
    return last;
}
static int txt(const char *name) { size_t n = strlen(name); return n >= 4 && strcmp(name + n - 4, ".txt") == 0; }
static char *join(const char *dir, const char *name) {
    size_t a = strlen(dir), b = strlen(name);
    char *p = malloc(a+b+2);
    if (p) { memcpy(p, dir, a); p[a] = '/'; memcpy(p+a+1, name, b+1); }
    return p;
}
static int run_file(const char *in, const char *outdir) {
    const char *name = basename_part(in);
    if (!txt(name)) { fprintf(stderr, "Input must be .txt: %s\n", in); return 0; }
    size_t stem = strlen(name)-4;
    const char *suffix = "_omer_faruk_boyraz_241ADB161.txt";
    char *outname = malloc(stem + strlen(suffix) + 1);
    if (!outname) return 0;
    memcpy(outname, name, stem); strcpy(outname+stem, suffix);
    char *outpath = join(outdir, outname);
    free(outname);
    if (!outpath) return 0;
    int ok = process(in, outpath);
    free(outpath);
    return ok;
}
int main(int argc, char **argv) {
    const char *dir = NULL, *outdir = NULL, *input = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (dir || i+1 >= argc) { fprintf(stderr, "Invalid directory option\n"); return 1; }
            dir = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0) {
            if (outdir || i+1 >= argc) { fprintf(stderr, "Invalid output option\n"); return 1; }
            outdir = argv[++i];
        } else if (argv[i][0] == '-' || input) { fprintf(stderr, "Unknown or extra argument: %s\n", argv[i]); return 1; }
        else input = argv[i];
    }
    if ((!dir && !input) || (dir && input)) { fprintf(stderr, "Usage: calc [-d DIR] [-o OUTDIR] [input.txt]\n"); return 1; }
    char *default_dir = NULL;
    if (!outdir) {
        const char *name = basename_part(dir ? dir : input);
        size_t n = strlen(name);
        while (n && name[n-1] == '/') n--;
        if (!dir && n >= 4 && strcmp(name+n-4, ".txt") == 0) n -= 4;
        const char *suffix = "_omer_faruk_241ADB161";
        default_dir = malloc(n+strlen(suffix)+1);
        if (!default_dir) return 1;
        memcpy(default_dir, name, n); strcpy(default_dir+n, suffix);
        outdir = default_dir;
    }
    struct stat st;
    if (stat(outdir, &st) != 0) {
        if (mkdir(outdir, 0777) != 0) { fprintf(stderr, "Cannot create output directory %s\n", outdir); free(default_dir); return 1; }
    } else if (!S_ISDIR(st.st_mode)) { fprintf(stderr, "Output path is not a directory\n"); free(default_dir); return 1; }
    int ok = 1;
    if (input) ok = run_file(input, outdir);
    else {
        DIR *d = opendir(dir);
        if (!d) { fprintf(stderr, "Cannot open directory %s\n", dir); free(default_dir); return 1; }
        struct dirent *entry;
        while ((entry = readdir(d))) {
            if (!txt(entry->d_name)) continue;
            char *path = join(dir, entry->d_name);
            if (!path) { ok = 0; break; }
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) { free(path); continue; }
            if (!run_file(path, outdir)) ok = 0;
            free(path);
            if (!ok) break;
        }
        closedir(d);
    }
    free(default_dir);
    return ok ? 0 : 1;
}
