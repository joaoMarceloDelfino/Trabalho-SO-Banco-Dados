#include <iostream>
#include <cstring>
#include "banco.h"

#include <windows.h> // IPC do Windows
#define PIPE_NAME "\\\\.\\pipe\\banco_pipe"

void enviarRequisicao(HANDLE hPipe, const std::string& requisicao) {
    DWORD bytesEscritos;
    WriteFile(hPipe, requisicao.c_str(), requisicao.length(), &bytesEscritos, NULL);
    std::cout << "[CLIENTE] Enviado: " << requisicao << "\n";
    Sleep(1000); //1000ms
}

int main() {
    std::cout << "--- SISTEMA DE BANCO DE DADOS (JSON) ---\n";

    HANDLE hPipe;
    
    while (true) {
        hPipe = CreateFileA(PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) break; // Conectou!
        if (GetLastError() != ERROR_PIPE_BUSY) {
            std::cerr << "Erro ao conectar ao Servidor. Verifique se ele esta rodando.\n";
            return 1;
        }
        if (!WaitNamedPipeA(PIPE_NAME, 20000)) return 1;
    }

    enviarRequisicao(hPipe, "INSERT 1 STRING Maria");
    enviarRequisicao(hPipe, "INSERT 2 INT 25");
    enviarRequisicao(hPipe, "INSERT 3 DOUBLE 1500.75");
    enviarRequisicao(hPipe, "UPDATE 2 STRING Vinte e cinco");
    enviarRequisicao(hPipe, "DELETE 3");
    enviarRequisicao(hPipe, "SELECT 1");

    std::cout << "[CLIENTE] Todas as requisicoes foram enviadas!\n";
    CloseHandle(hPipe);
    return 0;
}