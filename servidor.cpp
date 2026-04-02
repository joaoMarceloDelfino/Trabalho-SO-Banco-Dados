#include "banco.h"
#include <iostream>
#include <fstream> 
#include <cstring>
#include <sstream>
#include <windows.h> // IPC do Windows


#define PIPE_NAME "\\\\.\\pipe\\banco_pipe"

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

void processarRequisicao(const std::string& comando) {
    std::istringstream iss(comando);
    std::string acao;
    iss >> acao;

    if (acao == "INSERT" || acao == "UPDATE") {
        int id; std::string tipoStr;
        iss >> id >> tipoStr;
        Valor v; TipoDado tipo;

        if (tipoStr == "STRING") {
            tipo = TIPO_STRING; std::string resto; std::getline(iss >> std::ws, resto);
            strncpy(v.str, resto.c_str(), 49); v.str[49] = '\0';
        } else if (tipoStr == "INT") {
            tipo = TIPO_INT; iss >> v.i;
        } else if (tipoStr == "DOUBLE") {
            tipo = TIPO_DOUBLE; iss >> v.d;
        }

        if (acao == "INSERT") inserirRegistro(id, v, tipo);
        else atualizarRegistro(id, v, tipo);
        std::cout << "[SERVIDOR] Processou sequencialmente: " << comando << "\n";
    } 
    else if (acao == "DELETE") {
        int id; iss >> id;
        deletarRegistro(id);
        std::cout << "[SERVIDOR] Processou sequencialmente: " << comando << "\n";
    }
    else if (acao == "SELECT") {
        int id; iss >> id; Registro r;
        if (buscarRegistro(id, &r)) std::cout << "[SERVIDOR] Busca resolvida: ID " << id << "\n";
    }
}

int main() {
    inicializarBanco();
    std::cout << "[SERVIDOR] Aguardando req pipe...\n";

    HANDLE hPipe = CreateNamedPipeA(PIPE_NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) return 1;

    while (true) {
        bool conectado = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (conectado) {
            char buffer[256]; DWORD bytesLidos;
            while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesLidos, NULL) && bytesLidos != 0) {
                buffer[bytesLidos] = '\0'; 
                processarRequisicao(std::string(buffer)); // Chamada sequencial
            }
        }
        DisconnectNamedPipe(hPipe);
    }
    CloseHandle(hPipe);
    return 0;
}