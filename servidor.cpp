#define _WIN32_WINNT 0x0600 // Força o uso da API moderna do Windows
#include "banco.h"
#include <iostream>
#include <fstream> 
#include <cstring>
#include <sstream>
#include <windows.h> // IPC do Windows
//tava dando problema com o Mingw, pelo que pesquisei eh bug do windows....
// #include <thread> 
// #include <mutex>  
#include <queue>

using namespace std;

#define PIPE_NAME "\\\\.\\pipe\\banco_pipe"

Registro tabela[MAX_REGISTROS];
int totalRegistros = 0;

// Mutex nativo do Windows (Equivalente ao pthread_mutex_t)
CRITICAL_SECTION mutex_banco;

queue<string> fila_requisicoes; // Fila onde o main() vai colocar os comandos
CRITICAL_SECTION mutex_fila;    // Mutex exclusivo para proteger a fila
CONDITION_VARIABLE cv_fila;     // Variável para fazer as threads dormirem/acordarem

void salvarBancoJson() {
    ofstream arquivo("banco.json");
    if (!arquivo.is_open()) {
        cerr << "[BANCO] Erro ao abrir banco.json para escrita.\n";
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
   
    cout << "[BANCO] Inicializado. Memoria limpa.\n";
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
    
    cout << "[BANCO] INSERT executado no ID " << id << ".\n";
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
            cout << "[BANCO] UPDATE executado no ID " << id << ".\n";
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
            cout << "[BANCO] DELETE executado no ID " << id << ".\n";
            salvarBancoJson(); // Atualiza o JSON
            return true;
        }
    }
    return false;
}

// Formato exigido para a thread nativa do Windows (DWORD WINAPI)
DWORD WINAPI threadDoPool(LPVOID arg) {
    while (true) {
        string comando;
        
        EnterCriticalSection(&mutex_fila); // Trava a fila
        
        while (fila_requisicoes.empty()) {
            SleepConditionVariableCS(&cv_fila, &mutex_fila, INFINITE);
        }
        
        comando = fila_requisicoes.front();
        fila_requisicoes.pop();
        
        LeaveCriticalSection(&mutex_fila); // Destrava a fila para o main() poder adicionar mais
       
        istringstream iss(comando);
        string acao;
        iss >> acao;

        EnterCriticalSection(&mutex_banco);

        if (acao == "INSERT" || acao == "UPDATE") {
            int id; string tipoStr;
            iss >> id >> tipoStr;
            Valor v; TipoDado tipo;

            if (tipoStr == "STRING") {
                tipo = TIPO_STRING; string resto; getline(iss >> ws, resto);
                strncpy(v.str, resto.c_str(), 49); v.str[49] = '\0';
            } else if (tipoStr == "INT") {
                tipo = TIPO_INT; iss >> v.i;
            } else if (tipoStr == "DOUBLE") {
                tipo = TIPO_DOUBLE; iss >> v.d;
            }

            if (acao == "INSERT") inserirRegistro(id, v, tipo);
            else atualizarRegistro(id, v, tipo);
            
            cout << "[THREAD " << GetCurrentThreadId() << "] Resolvido: " << comando << "\n";
        } 
        else if (acao == "DELETE") {
            int id; iss >> id;
            deletarRegistro(id);
            cout << "[THREAD " << GetCurrentThreadId() << "] Resolvido: " << comando << "\n";
        }
       else if (acao == "SELECT") {
            int id; 
            iss >> id; 
            Registro r;
            
            if (buscarRegistro(id, &r)) {
                cout << "[THREAD " << GetCurrentThreadId() << "] Busca resolvida: ID " << id << " | Valor: ";
                
                if (r.tipo == TIPO_INT) {
                    cout << r.valor.i << " (INT)\n";
                } 
                else if (r.tipo == TIPO_DOUBLE) {
                    cout << r.valor.d << " (DOUBLE)\n";
                } 
                else if (r.tipo == TIPO_STRING) {
                    cout << r.valor.str << " (STRING)\n";
                }
            } 
            else {
                cout << "[THREAD " << GetCurrentThreadId() << "] Busca falhou: ID " << id << " nao encontrado.\n";
            }
        }
        
        LeaveCriticalSection(&mutex_banco);
    }
    return 0;
}

int main() {
    inicializarBanco();

    // Inicia a variável do Mutex do Windows
    InitializeCriticalSection(&mutex_banco);
    InitializeCriticalSection(&mutex_fila);
    InitializeConditionVariable(&cv_fila);

   // CRIA AS THREADS DO POOL (ex: 4 threads) UMA ÚNICA VEZ
    cout << "[SERVIDOR] Criando Thread Pool com 4 threads...\n";
    const int NUM_THREADS = 4;
    HANDLE pool[NUM_THREADS];
    for(int i = 0; i < NUM_THREADS; i++) {
        pool[i] = CreateThread(NULL, 0, threadDoPool, NULL, 0, NULL);
    }

    cout << "[SERVIDOR] Aguardando requisicoes no Pipe...\n";

   
    HANDLE hPipe = CreateNamedPipeA(PIPE_NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) return 1;

  
    while (true) {
        bool conectado = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (conectado) {
            char buffer[256]; DWORD bytesLidos;
            
            // Lê a mensagem enviada pelo cliente
            while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesLidos, NULL) && bytesLidos != 0) {
                buffer[bytesLidos] = '\0'; 
                
                // Fila de Requisições
                EnterCriticalSection(&mutex_fila);
                fila_requisicoes.push(string(buffer));
                LeaveCriticalSection(&mutex_fila);
                
                // Acorda uma das threads do Pool que estava dormindo para trabalhar
                WakeConditionVariable(&cv_fila);
            }
        }
        DisconnectNamedPipe(hPipe);
    }
    
   
    DeleteCriticalSection(&mutex_banco);
    DeleteCriticalSection(&mutex_fila);
    CloseHandle(hPipe);
    return 0;
}