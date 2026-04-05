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

// SRWLOCK: Permite múltiplas leituras simultâneas, mas escrita exclusiva!
SRWLOCK lock_banco;

queue<string> fila_requisicoes; // Fila onde o main() vai colocar os comandos
CRITICAL_SECTION mutex_fila;    // Mutex exclusivo para proteger a fila
CONDITION_VARIABLE cv_fila;     // Variável para fazer as threads dormirem/acordarem

// Mutex para não bagunçar o arquivo de Log se duas threads escreverem juntas
CRITICAL_SECTION mutex_log;

void gravarLog(const string& mensagem) {
    EnterCriticalSection(&mutex_log);
    ofstream arquivo("log.txt", ios::app); // ios::app para adicionar ao final
    if (arquivo.is_open()) {
        arquivo << mensagem << "\n";
        arquivo.close();
    }
    LeaveCriticalSection(&mutex_log);
}

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

            // ESCRITA: Bloqueia totalmente o banco para outras threads!
            AcquireSRWLockExclusive(&lock_banco);
            if (acao == "INSERT") inserirRegistro(id, v, tipo);
            else atualizarRegistro(id, v, tipo);
            ReleaseSRWLockExclusive(&lock_banco);
            
            // Monta resposta e salva log
            ostringstream resp;
            resp << "[THREAD " << GetCurrentThreadId() << "] Resolvido: " << comando;
            cout << resp.str() << "\n";
            gravarLog(resp.str());
        } 
        else if (acao == "DELETE") {
            int id; iss >> id;
            
            // ESCRITA: Bloqueia totalmente
            AcquireSRWLockExclusive(&lock_banco);
            deletarRegistro(id);
            ReleaseSRWLockExclusive(&lock_banco);

            ostringstream resp;
            resp << "[THREAD " << GetCurrentThreadId() << "] Resolvido: " << comando;
            cout << resp.str() << "\n";
            gravarLog(resp.str());
        }
        else if (acao == "SELECT") {
            int id; iss >> id; Registro r;
            
            // LEITURA: Deixa várias threads lerem ao mesmo tempo!
            AcquireSRWLockShared(&lock_banco);
            bool achou = buscarRegistro(id, &r);
            ReleaseSRWLockShared(&lock_banco);
            
            ostringstream resp;
            if (achou) {
                resp << "[THREAD " << GetCurrentThreadId() << "] Busca resolvida: ID " << id << " | Valor: ";
                if (r.tipo == TIPO_INT) resp << r.valor.i << " (INT)";
                else if (r.tipo == TIPO_DOUBLE) resp << r.valor.d << " (DOUBLE)";
                else if (r.tipo == TIPO_STRING) resp << r.valor.str << " (STRING)";
            } else {
                resp << "[THREAD " << GetCurrentThreadId() << "] Busca falhou: ID " << id << " nao encontrado.";
            }
            
            cout << resp.str() << "\n";
            gravarLog(resp.str());
        }
    }
    return 0;
}

int main() {
    inicializarBanco();

    // Inicializa SRWLOCK (Banco), Mutexes (Fila e Log) e Variável de Condição
    InitializeSRWLock(&lock_banco);
    InitializeCriticalSection(&mutex_fila);
    InitializeCriticalSection(&mutex_log);
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
    
   
    DeleteCriticalSection(&mutex_log);
    DeleteCriticalSection(&mutex_fila);
    CloseHandle(hPipe);
    return 0;
}