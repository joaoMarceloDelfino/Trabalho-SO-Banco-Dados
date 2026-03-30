#include <iostream>
#include <cstring>
#include "banco.h"

int main() {
    std::cout << "--- SISTEMA DE BANCO DE DADOS (JSON) ---\n";
    
    inicializarBanco();

    Valor v1, v2, v3, v_update;

    strncpy(v1.str, "Maria", 49);
    v1.str[49] = '\0';
    inserirRegistro(1, v1, TIPO_STRING);

    v2.i = 25;
    inserirRegistro(2, v2, TIPO_INT);

    v3.d = 1500.75;
    inserirRegistro(3, v3, TIPO_DOUBLE);

    strncpy(v_update.str, "Vinte e cinco", 49);
    v_update.str[49] = '\0';
    atualizarRegistro(2, v_update, TIPO_STRING);

    deletarRegistro(3);

    Registro r;
    if (buscarRegistro(1, &r)) {
        std::cout << "-> BUSCA ID 1: Tipo " << r.tipo << " | Valor: ";
        if (r.tipo == TIPO_STRING) std::cout << r.valor.str << "\n";
    }

    std::cout << "-> Verifique o arquivo 'banco.json' na sua pasta!\n";

    return 0;
}