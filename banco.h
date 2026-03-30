#ifndef BANCO_H
#define BANCO_H

#include <string.h>
#include <stdbool.h>

#define MAX_REGISTROS 100

typedef enum {
    TIPO_INT,
    TIPO_DOUBLE,
    TIPO_STRING
} TipoDado;

typedef union {
    int i;
    double d;
    char str[50];
} Valor;

typedef struct {
    int id;
    TipoDado tipo;
    Valor valor;
} Registro;

void inicializarBanco();

bool inserirRegistro(int id, Valor valor, TipoDado tipo);

bool deletarRegistro(int id);

bool buscarRegistro(int id, Registro* retorno);

bool atualizarRegistro(int id, Valor novoValor, TipoDado tipo);

#endif