// Matīss Grūbe 251RDC035
// https://github.com/CodeMytz/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: From assignment text (the hint in 8.); course slides 4-6 (strings, files, directories, malloc); my own week5_task1_file_io.c and week5_task4_struct_binary_io.c; manual pages pow, mkdir, readdir; links in the comments above the functions.

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FILE_SUFFIX "_matiss_grube_251rdc035.txt"
#define DIR_SUFFIX "_matiss_251rdc035"
#define PATH_LEN 4096
#define NAME_LEN 256

// https://www.tutorialspoint.com/cprogramming/c_typedef.htm https://www.freecodecamp.org/news/structured-data-types-in-c-struct-and-typedef-explained-with-examples/
// one struct holds either an integer or a float result.
typedef struct Value {
  int is_float;
  long long i;
  double d;
} Value;

char* text;
long len;
long pos;
long error_pos = -1;

// pos indexes the whole file and never resets, so pos + 1 is the file position.
void skip_spaces(void) {
  while (text[pos] == ' ' || text[pos] == '\t') {
    pos++;
  }
}

int is_digit(char c) {
  if (c >= '0' && c <= '9') {
    return 1;
  }

  return 0;
}

// by squaring the base, the exponent is halved each time, which makes calculation with large exponents faster.
// source: https://helloacm.com/exponentiation-by-squaring/
long long int_power(long long base, long long exponent) {
  long long result = 1;

  while (exponent > 0) {
    if (exponent % 2 == 1) {
      result = result * base;
    }

    exponent = exponent / 2;

    if (exponent > 0) {
      base = base * base;
    }
  }

  return result;
}

double to_double(const Value* value) {
  if (value->is_float == 1) {
    return value->d;
  }

  return (double)value->i;
}

int is_zero(const Value* value) {
  if (value->is_float == 1) {
    return value->d == 0.0;
  }

  return value->i == 0;
}

// if either side is a float, the result is a float. Errors point to the right operand.
void calculate(char op, Value* left, const Value* right, long op_pos, long right_pos) {
  if (op == '/' && is_zero(right) == 1) {
    error_pos = right_pos;
    return;
  }

  if (left->is_float == 1 || right->is_float == 1) {
    double x = to_double(left);
    double y = to_double(right);

    switch (op) {
      case '+':
        left->d = x + y;
        break;
      case '-':
        left->d = x - y;
        break;
      case '*':
        left->d = x * y;
        break;
      case '/':
        left->d = x / y;
        break;
      case '^':
        left->d = pow(x, y);
        break;
    }

    left->is_float = 1;

    if (left->d != left->d || left->d - left->d != 0.0) {
      error_pos = op_pos;
    }

    return;
  }

  if (op == '^' && right->i < 0) {
    error_pos = right_pos;
    return;
  }

  switch (op) {
    case '+':
      left->i = left->i + right->i;
      break;
    case '-':
      left->i = left->i - right->i;
      break;
    case '*':
      left->i = left->i * right->i;
      break;
    case '/':
      left->i = left->i / right->i;
      break;
    case '^':
      left->i = int_power(left->i, right->i);
      break;
  }
}

// the only place that requires an operand, so an invalid token here marks the error position.
void parse_number(Value* value) {
  value->is_float = 0;
  value->i = 0;
  value->d = 0.0;

  if (is_digit(text[pos]) == 0) {
    error_pos = pos;
    return;
  }

  long start = pos;

  while (is_digit(text[pos]) == 1) {
    pos++;
  }

  if (text[pos] == '.' && is_digit(text[pos + 1]) == 1) {
    long dot = pos;
    pos++;

    while (is_digit(text[pos]) == 1) {
      pos++;
    }

    double mantissa = 0.0;

    for (long k = start; k < pos; k++) {
      if (k != dot) {
        mantissa = mantissa * 10.0 + (text[k] - '0');
      }
    }

    value->is_float = 1;
    value->d = mantissa / pow(10.0, (double)(pos - dot - 1));
    return;
  }

  for (long k = start; k < pos; k++) {
    value->i = value->i * 10 + (text[k] - '0');
  }
}

void parse_expr(Value* value);

// Source: https://gist.github.com/cpq/d76674d406d4d4362e14
void parse_primary(Value* value) {
  skip_spaces();

  if (text[pos] != '(') {
    parse_number(value);
    return;
  }

  pos++;
  parse_expr(value);

  if (error_pos >= 0) {
    return;
  }

  skip_spaces();

  if (text[pos] != ')') {
    error_pos = pos;
    return;
  }

  pos++;
}

// recursing for the right operand makes ** right-associative: 2 ** 3 ** 2 is 2 ** 9.
// Source: https://eli.thegreenplace.net/2009/03/14/some-problems-of-recursive-descent-parsers/ and https://github.com/jasanign/Parser
void parse_power(Value* left) {
  parse_primary(left);
  skip_spaces();

  if (error_pos >= 0 || text[pos] != '*' || text[pos + 1] != '*') {
    return;
  }

  long op_pos = pos;
  pos += 2;
  skip_spaces();

  long right_pos = pos;
  Value right;
  parse_power(&right);

  if (error_pos >= 0) {
    return;
  }

  calculate('^', left, &right, op_pos, right_pos);
}

// call order sets precedence; the loops make + - * / left-associative.
// Source: https://gist.github.com/cpq/d76674d406d4d4362e14 and https://introcs-python.cs.luc.edu/recursion/recursiondparser.html
void parse_term(Value* left) {
  parse_power(left);

  while (error_pos < 0) {
    skip_spaces();

    char op = text[pos];

    if (op != '*' && op != '/') {
      break;
    }

    long op_pos = pos;
    pos++;
    skip_spaces();

    long right_pos = pos;
    Value right;
    parse_power(&right);

    if (error_pos >= 0) {
      break;
    }

    calculate(op, left, &right, op_pos, right_pos);
  }
}

void parse_expr(Value* left) {
  parse_term(left);

  while (error_pos < 0) {
    skip_spaces();

    char op = text[pos];

    if (op != '+' && op != '-') {
      break;
    }

    long op_pos = pos;
    pos++;
    skip_spaces();

    long right_pos = pos;
    Value right;
    parse_term(&right);

    if (error_pos >= 0) {
      break;
    }

    calculate(op, left, &right, op_pos, right_pos);
  }
}

int ends_with_txt(const char* s) {
  int n = strlen(s);

  if (n > 4 && strcmp(s + n - 4, ".txt") == 0) {
    return 1;
  }

  return 0;
}

// it removes folders and an optional .txt extension, so the output name does NOT depend on the input path.
void get_name(const char* path, char* name, int cut_txt) {
  int end = strlen(path);

  while (end > 1 && path[end - 1] == '/') {
    end--;
  }

  int start = end;

  while (start > 0 && path[start - 1] != '/') {
    start--;
  }

  if (cut_txt == 1 && ends_with_txt(path) == 1) {
    end = end - 4;
  }

  int n = 0;

  while (start + n < end && n < NAME_LEN - 1) {
    name[n] = path[start + n];
    n++;
  }

  name[n] = '\0';
}

// joins with strlen/strcpy; returns -1 if the result would not fit in PATH_LEN.
int join(char* dest, const char* a, const char* b, const char* c, const char* d) {
  long total = (long)strlen(a) + (long)strlen(b) + (long)strlen(c) + (long)strlen(d);

  if (total >= PATH_LEN) {
    return -1;
  }

  strcpy(dest, a);
  strcpy(dest + strlen(dest), b);
  strcpy(dest + strlen(dest), c);
  strcpy(dest + strlen(dest), d);
  return 0;
}

// size with fseek + ftell.
// Source: https://cplusplus.com/fread, also same as in week5_task4_struct_binary_io.c
long file_size(const char* filename) {
  FILE* fp = fopen(filename, "rb");

  if (fp == NULL) {
    return -1;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return -1;
  }

  long size = ftell(fp);
  fclose(fp);
  return size;
}

// one malloc and one fread, then parsed in place, so nothing is copied; +1 is for '\0'.
// Source: https://cplusplus.com/fread and https://stackoverflow.com/questions/19080662
int load_file(const char* filename) {
  len = file_size(filename);

  if (len < 0) {
    perror(filename);
    return -1;
  }

  FILE* fp = fopen(filename, "rb");

  if (fp == NULL) {
    perror(filename);
    return -1;
  }

  text = malloc(len + 1);

  if (text == NULL) {
    fprintf(stderr, "calc: out of memory\n");
    fclose(fp);
    return -1;
  }

  if (len > 0 && fread(text, len, 1, fp) != 1) {
    fprintf(stderr, "calc: cannot read %s\n", filename);
    free(text);
    fclose(fp);
    return -1;
  }

  fclose(fp);
  text[len] = '\0';
  return 0;
}

// writes results line by line to save memory amd then on error, process_file rewrites the file.
void evaluate_lines(FILE* out) {
  pos = 0;
  error_pos = -1;

  while (1) {
    skip_spaces();

    if (pos >= len) {
      break;
    }

    if (text[pos] == '\n') {
      pos++;
    } else if (text[pos] == '#') {
      while (pos < len && text[pos] != '\n') {
        pos++;
      }
    } else {
      Value value;
      parse_expr(&value);
      skip_spaces();

      if (error_pos < 0 && pos < len && text[pos] != '\n') {
        error_pos = pos;
      }

      if (error_pos >= 0) {
        break;
      }

      if (value.is_float == 1) {
        fprintf(out, "%.15g\n", value.d);
      } else {
        fprintf(out, "%lld\n", value.i);
      }
    }
  }
}

// mkdir failing with EEXIST is not an error, because an existing output folder is allowed.
// Source: https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950
int process_file(const char* input, const char* outdir) {
  if (load_file(input) != 0) {
    return EXIT_FAILURE;
  }

  char name[NAME_LEN];
  char path[PATH_LEN];

  get_name(input, name, 1);

  if (join(path, outdir, "/", name, FILE_SUFFIX) != 0) {
    fprintf(stderr, "calc: path too long\n");
    free(text);
    return EXIT_FAILURE;
  }

  #ifdef _WIN32
    int made = mkdir(outdir);
  #else
    int made = mkdir(outdir, 0777);
  #endif

  if (made != 0 && errno != EEXIST) {
    perror(outdir);
    free(text);
    return EXIT_FAILURE;
  }

  FILE* fp = fopen(path, "w");

  if (fp == NULL) {
    perror(path);
    free(text);
    return EXIT_FAILURE;
  }

  evaluate_lines(fp);
  free(text);

  if (error_pos >= 0) {
    fclose(fp);
    fp = fopen(path, "w");

    if (fp == NULL) {
      perror(path);
      return EXIT_FAILURE;
    }

    fprintf(fp, "ERROR:%ld\n", error_pos + 1);
  }

  if (fclose(fp) != 0) {
    fprintf(stderr, "calc: cannot write %s\n", path);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

// opendir works only on folders, so it is used to skip folders named like x.txt.
// Source: https://www.tutorialspoint.com/article/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-cplusplus and https://cplusplus.com/forum/beginner/61901
int process_dir(const char* dir, const char* outdir) {
  DIR* d = opendir(dir);

  if (d == NULL) {
    perror(dir);
    return EXIT_FAILURE;
  }

  int status = EXIT_SUCCESS;
  struct dirent* entry;

  while ((entry = readdir(d)) != NULL) {
    if (ends_with_txt(entry->d_name) == 1) {
      char path[PATH_LEN];

      if (join(path, dir, "/", entry->d_name, "") != 0) {
        fprintf(stderr, "calc: path too long\n");
        status = EXIT_FAILURE;
      } else {
        DIR* sub = opendir(path);

        if (sub != NULL) {
          closedir(sub);
        } else if (process_file(path, outdir) != 0) {
          status = EXIT_FAILURE;
        }
      }
    }
  }

  closedir(d);
  return status;
}

int usage(void) {
  fprintf(stderr, "usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
  return EXIT_FAILURE;
}

int main(int argc, char* argv[]) {
  char* outdir = NULL;
  char* dir = NULL;
  char* input = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0) {
      i++;

      if (i >= argc) {
        return usage();
      }

      outdir = argv[i];
    } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
      i++;

      if (i >= argc) {
        return usage();
      }

      dir = argv[i];
    } else if (argv[i][0] != '-' && input == NULL) {
      input = argv[i];
    } else {
      fprintf(stderr, "calc: bad argument %s\n", argv[i]);
      return usage();
    }
  }

  if (input == NULL && dir == NULL) {
    return usage();
  }

  if (input != NULL && dir != NULL) {
    return usage();
  }

  char default_outdir[PATH_LEN];

  if (outdir == NULL) {
    char name[NAME_LEN];

    if (dir != NULL) {
      get_name(dir, name, 0);
    } else {
      get_name(input, name, 1);
    }

    join(default_outdir, name, DIR_SUFFIX, "", "");
    outdir = default_outdir;
  }

  if (dir != NULL) {
    return process_dir(dir, outdir);
  }

  return process_file(input, outdir);
}