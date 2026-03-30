#include "banco.h"
#include <iostream>
#include <fstream> 
#include <cstring>

Registro tabela[MAX_REGISTROS];
int totalRegistros = 0;

void salvarBancoJson() {
    std::ofstream arquivo("banco.json");
    if (!arquivo.is_open()) {
        std::cerr << "[BANCO] Erro ao abrir banco.json para escrita.\n";
        return;
    }

    arquivo << "[\n";
    for (int i = 0; i < totalRegistros; i++) {
        arquivo << "  {\n";
        arquivo << "    \"id\": " << tabela[i].id << ",\n";
        
        if (tabela[i].tipo == TIPO_INT) {
            arquivo << "    \"tipo\": \"INT\",\n";
            arquivo << "    \"valor\": " << tabela[i].valor.i << "\n";
        } 
        else if (tabela[i].tipo == TIPO_DOUBLE) {
            arquivo << "    \"tipo\": \"DOUBLE\",\n";
            arquivo << "    \"valor\": " << tabela[i].valor.d << "\n";
        } 
        else if (tabela[i].tipo == TIPO_STRING) {
            arquivo << "    \"tipo\": \"STRING\",\n";
            arquivo << "    \"valor\": \"" << tabela[i].valor.str << "\"\n"; // Aspas para string
        }

        arquivo << "  }";
        if (i < totalRegistros - 1) {
            arquivo << ",";
        }
        arquivo << "\n";
    }
    arquivo << "]\n";
    arquivo.close();
}

void inicializarBanco() {
    totalRegistros = 0;
   
    std::cout << "[BANCO] Inicializado. Memoria limpa.\n";
}

bool inserirRegistro(int id, Valor valor, TipoDado tipo) {
    if (totalRegistros >= MAX_REGISTROS) return false;

    for (int i = 0; i < totalRegistros; i++) {
        if (tabela[i].id == id) return false;
    }

    tabela[totalRegistros].id = id;
    tabela[totalRegistros].tipo = tipo;
    tabela[totalRegistros].valor = valor; // C++ copia a union toda automaticamente
    
    totalRegistros++;
    
    std::cout << "[BANCO] INSERT executado no ID " << id << ".\n";
    salvarBancoJson(); // Atualiza o JSON
    return true;
}

bool buscarRegistro(int id, Registro* retorno) {
    for (int i = 0; i < totalRegistros; i++) {
        if (tabela[i].id == id) {
            *retorno = tabela[i];
            return true;
        }
    }
    return false;
}

bool atualizarRegistro(int id, Valor novoValor, TipoDado tipo) {
    for (int i = 0; i < totalRegistros; i++) {
        if (tabela[i].id == id) {
            tabela[i].tipo = tipo;
            tabela[i].valor = novoValor;
            std::cout << "[BANCO] UPDATE executado no ID " << id << ".\n";
            salvarBancoJson(); // Atualiza o JSON
            return true;
        }
    }
    return false;
}

bool deletarRegistro(int id) {
    for (int i = 0; i < totalRegistros; i++) {
        if (tabela[i].id == id) {
            tabela[i] = tabela[totalRegistros - 1]; // Puxa o último pro buraco
            totalRegistros--;
            std::cout << "[BANCO] DELETE executado no ID " << id << ".\n";
            salvarBancoJson(); // Atualiza o JSON
            return true;
        }
    }
    return false;
}