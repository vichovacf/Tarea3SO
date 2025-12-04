#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iomanip>

using namespace std;

// ============================================================================
// CONSTANTES Y CONFIGURACIÓN
// ============================================================================

// Colores para la consola - para visualización mejorada
const string COLOR_RAM   = "\033[32m";  // Verde para RAM
const string COLOR_SWAP  = "\033[36m";  // Cyan para SWAP
const string COLOR_INFO  = "\033[33m";  // Amarillo para información general
const string COLOR_ERROR = "\033[31m";  // Rojo para errores
const string COLOR_FALLO = "\033[35m";  // Magenta para fallos de página
const string COLOR_RESET = "\033[0m";   // Resetear color

// ============================================================================
// ESTRUCTURAS DE DATOS
// ============================================================================

// Estructura que representa una página de memoria
struct Pagina {
    int pid;                    // ID del proceso dueño de esta página
    int id_pagina;              // ID local de la página dentro del proceso (0, 1, 2...)
    int id_global;              // ID global único de la página
    bool en_ram;                // True si la página está actualmente en RAM
    int indice_marco;           // Índice del marco en RAM (-1 si no está en RAM)
    int indice_swap;            // Índice del marco en SWAP (-1 si no está en SWAP)
    unsigned long long tiempo_carga; // Tiempo de carga (para política FIFO)
    
    // Constructor de la página
    Pagina(int p_pid, int p_id_pagina, int p_id_global): 
        pid(p_pid), 
        id_pagina(p_id_pagina),
        id_global(p_id_global),
        en_ram(false), 
        indice_marco(-1), 
        indice_swap(-1), 
        tiempo_carga(0) {}
};

// Estructura que representa un proceso
struct Proceso {
    int pid;                   // ID único del proceso
    int tamano_mb;             // Tamaño total del proceso en MB
    int num_paginas;           // Número de páginas que ocupa el proceso
    bool activo;               // Si el proceso está activo (no finalizado)
    vector<int> indices_paginas; // Índices de las páginas en el vector global 'paginas'
    
    // Constructor del proceso
    Proceso(int p_pid, int p_tamano_mb, int p_num_paginas): 
        pid(p_pid), 
        tamano_mb(p_tamano_mb), 
        num_paginas(p_num_paginas), 
        activo(true) {}
};

// ============================================================================
// CLASE PRINCIPAL - SIMULADOR DE MEMORIA
// ============================================================================

class SimuladorMemoria {
private:
    // Configuración del sistema
    int memoria_fisica_mb;      // Tamaño de la memoria física en MB
    int tamano_pagina_mb;       // Tamaño de cada página en MB
    int proceso_min_mb;         // Tamaño mínimo de proceso en MB
    int proceso_max_mb;         // Tamaño máximo de proceso en MB
    
    // Representación de la memoria
    vector<int> marcos_ram;     // Vector de marcos de RAM: -1 = libre, índice = página
    vector<int> marcos_swap;    // Vector de marcos de SWAP: -1 = libre, índice = página
    
    // Listas de procesos y páginas
    vector<Proceso> procesos;   // Todos los procesos creados
    vector<Pagina> paginas;     // Todas las páginas de todos los procesos
    
    // Contadores y estado interno
    int siguiente_pid;          // Siguiente ID disponible para procesos
    int siguiente_id_pagina;    // Siguiente ID global para páginas
    unsigned long long contador_carga; // Contador para tiempo de carga (FIFO)
    
    // Estadísticas para reporte final
    int fallos_pagina;          // Número total de fallos de página ocurridos
    int procesos_creados;       // Número total de procesos creados
    int procesos_finalizados;   // Número total de procesos finalizados
    
    // Generador de números aleatorios
    mt19937 generador_aleatorio;

public:
    // Constructor principal - inicializa toda la simulación
    SimuladorMemoria(int mem_fisica_mb, int tam_pagina_mb, int proc_min_mb, int proc_max_mb) 
        : memoria_fisica_mb(mem_fisica_mb),  // Memoria fisica en MB
          tamano_pagina_mb(tam_pagina_mb),   // Tamaño de página en MB
          proceso_min_mb(proc_min_mb),       // Tamaño minimo de cada proceso en MB
          proceso_max_mb(proc_max_mb),       // Tamaño maximo de cada proceso en MB
          siguiente_pid(1),                  // Empezar PIDs desde 1
          siguiente_id_pagina(1),            // Empezar IDs de páginas desde 1
          contador_carga(0),                 // Contador de carga inicializado en 0
          fallos_pagina(0),                  // Contador de fallos de página en 0
          procesos_creados(0),               // Contador de procesos creados en 0
          procesos_finalizados(0),           // Contador de procesos finalizados en 0
          generador_aleatorio(random_device{}()) {
        
        inicializarMemoria(); // Inicializar la memoria del sistema
    }

    // Inicializa la memoria física y virtual
    void inicializarMemoria() {
        // Calcular memoria virtual (entre 1.5 y 4.5 veces la física)
        uniform_real_distribution<double> distribucion_factor(1.5, 4.5);
        double memoria_virtual_mb = memoria_fisica_mb * distribucion_factor(generador_aleatorio);
        
        // Calcular número de marcos
        int num_marcos_ram = memoria_fisica_mb / tamano_pagina_mb;      // Marcos en RAM
        int total_marcos = memoria_virtual_mb / tamano_pagina_mb;       // Marcos totales (RAM + SWAP)
        int num_marcos_swap = total_marcos - num_marcos_ram;            // Marcos en SWAP
        
        // Asegurar valores mínimos
        if (num_marcos_ram <= 0) num_marcos_ram = 1;
        if (num_marcos_swap <= 0) num_marcos_swap = total_marcos; // Asegurar suficiente swap
        
        // Inicializar vectores de memoria (todos libres inicialmente: -1)
        marcos_ram.resize(num_marcos_ram, -1);
        marcos_swap.resize(num_marcos_swap, -1);
        
        // Mostrar configuración inicial
        cout << COLOR_INFO << "=== CONFIGURACIÓN INICIAL ===" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Memoria Física: " << memoria_fisica_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Memoria Virtual: " << memoria_virtual_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Tamaño Página: " << tamano_pagina_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Marcos RAM: " << num_marcos_ram << COLOR_RESET << endl;
        cout << COLOR_INFO << "Marcos SWAP: " << num_marcos_swap << COLOR_RESET << endl;
        cout << COLOR_INFO << "=============================" << COLOR_RESET << endl;
    }

    // Encuentra un marco libre en un vector de marcos (RAM o SWAP)
    int encontrarMarcoLibre(const vector<int>& marcos) {
        for (size_t i = 0; i < marcos.size(); ++i) {
            if (marcos[i] == -1) return i; // Retorna el índice del primer marco libre
        }
        return -1; // No hay marcos libres
    }

    // Política FIFO: selecciona la página más antigua en RAM para reemplazar
    int elegirPaginaVictima() {
        int victima = -1;
        unsigned long long tiempo_mas_antiguo = numeric_limits<unsigned long long>::max();
        
        // Buscar entre todas las páginas en RAM
        for (size_t i = 0; i < paginas.size(); ++i) {
            if (paginas[i].en_ram && paginas[i].pid != -1 && paginas[i].tiempo_carga < tiempo_mas_antiguo) {
                tiempo_mas_antiguo = paginas[i].tiempo_carga;
                victima = i; // Actualizar víctima con la página más antigua
            }
        }
        return victima; // Retorna -1 si no hay páginas en RAM
    }

    // Crea un nuevo proceso con tamaño aleatorio
    bool crearProceso() {
        // Generar tamaño aleatorio del proceso dentro del rango especificado
        uniform_int_distribution<int> distribucion_tamano(proceso_min_mb, proceso_max_mb);
        int tamano_proceso = distribucion_tamano(generador_aleatorio);
        int num_paginas = ceil(static_cast<double>(tamano_proceso) / tamano_pagina_mb);
        
        // Crear nuevo proceso
        Proceso nuevo_proceso(siguiente_pid++, tamano_proceso, num_paginas);
        
        cout << COLOR_INFO << "[CREACIÓN] Proceso PID=" << nuevo_proceso.pid 
             << " (" << tamano_proceso << " MB, " << num_paginas << " páginas)" << COLOR_RESET << endl;
        
        // Verificar si hay suficiente memoria total (RAM + SWAP)
        int ram_libre = count(marcos_ram.begin(), marcos_ram.end(), -1);
        int swap_libre = count(marcos_swap.begin(), marcos_swap.end(), -1);
        
        if (ram_libre + swap_libre < num_paginas) {
            cout << COLOR_ERROR << "[ERROR] Memoria insuficiente para proceso PID=" 
                 << nuevo_proceso.pid << COLOR_RESET << endl;
            return false;
        }
        
        // Asignar páginas del proceso
        for (int i = 0; i < num_paginas; ++i) {
            Pagina nueva_pagina(nuevo_proceso.pid, i, siguiente_id_pagina++);
            int indice_pagina = paginas.size();
            
            // Intentar cargar en RAM primero (política de asignación)
            int marco_libre = encontrarMarcoLibre(marcos_ram);
            if (marco_libre != -1) {
                // Hay espacio en RAM
                nueva_pagina.en_ram = true;
                nueva_pagina.indice_marco = marco_libre;
                nueva_pagina.tiempo_carga = contador_carga++; // Marcar tiempo de carga para FIFO
                
                marcos_ram[marco_libre] = indice_pagina; // Asignar marco en RAM
                cout << COLOR_RAM << "  → Página " << nueva_pagina.id_global 
                     << " (PID=" << nuevo_proceso.pid << "-" << i 
                     << ") cargada en RAM (marco " << marco_libre << ")" << COLOR_RESET << endl;
            } else {
                // RAM llena, usar SWAP
                marco_libre = encontrarMarcoLibre(marcos_swap);
                if (marco_libre == -1) {
                    cout << COLOR_ERROR << "  → ERROR: No hay marcos libres en SWAP" << COLOR_RESET << endl;
                    return false;
                }
                nueva_pagina.en_ram = false;
                nueva_pagina.indice_swap = marco_libre;
                
                marcos_swap[marco_libre] = indice_pagina; // Asignar marco en SWAP
                cout << COLOR_SWAP << "  → Página " << nueva_pagina.id_global 
                     << " (PID=" << nuevo_proceso.pid << "-" << i 
                     << ") asignada a SWAP (marco " << marco_libre << ")" << COLOR_RESET << endl;
            }
            
            // Agregar página a la lista global y al proceso
            paginas.push_back(nueva_pagina);
            nuevo_proceso.indices_paginas.push_back(indice_pagina);
        }
        
        // Agregar proceso a la lista
        procesos.push_back(nuevo_proceso);
        procesos_creados++;
        return true;
    }

    // Finaliza un proceso aleatorio y libera su memoria
    void finalizarProcesoAleatorio() {
        // Obtener índices de procesos activos
        vector<int> indices_activos;
        for (size_t i = 0; i < procesos.size(); ++i) {
            if (procesos[i].activo) {
                indices_activos.push_back(i);
            }
        }
        
        if (indices_activos.empty()) {
            cout << COLOR_INFO << "[FINALIZACIÓN] No hay procesos activos" << COLOR_RESET << endl;
            return;
        }
        
        // Seleccionar proceso aleatorio para finalizar
        uniform_int_distribution<int> distribucion(0, indices_activos.size() - 1);
        int indice_proceso = indices_activos[distribucion(generador_aleatorio)];
        Proceso& proceso = procesos[indice_proceso];
        
        cout << COLOR_INFO << "[FINALIZACIÓN] Terminando proceso PID=" 
             << proceso.pid << COLOR_RESET << endl;
        
        // Liberar todas las páginas del proceso
        for (int indice_pagina : proceso.indices_paginas) {
            Pagina& pagina = paginas[indice_pagina];
            if (pagina.en_ram) {
                marcos_ram[pagina.indice_marco] = -1; // Liberar marco en RAM
                cout << COLOR_RAM << "  → Liberada página " << pagina.id_global 
                     << " (PID=" << pagina.pid << "-" << pagina.id_pagina 
                     << ") de RAM (marco " << pagina.indice_marco << ")" << COLOR_RESET << endl;
            } else {
                marcos_swap[pagina.indice_swap] = -1; // Liberar marco en SWAP
                cout << COLOR_SWAP << "  → Liberada página " << pagina.id_global 
                     << " (PID=" << pagina.pid << "-" << pagina.id_pagina 
                     << ") de SWAP (marco " << pagina.indice_swap << ")" << COLOR_RESET << endl;
            }
            // Marcar página como liberada (pid = -1)
            pagina.pid = -1;
        }
        
        // Marcar proceso como inactivo y limpiar sus páginas
        proceso.activo = false;
        proceso.indices_paginas.clear();
        procesos_finalizados++;
    }

    // Simulamos un acceso a memoria (puede causar fallo de página)
    bool simularAccesoMemoria() {
        // Obtener índices de procesos activos
        vector<int> indices_activos;
        for (size_t i = 0; i < procesos.size(); ++i) {
            if (procesos[i].activo) {
                indices_activos.push_back(i);
            }
        }
        
        if (indices_activos.empty()) {
            cout << COLOR_INFO << "[ACCESO] No hay procesos activos" << COLOR_RESET << endl;
            return true;
        }
        
        // Buscamos específicamente páginas en SWAP para forzar fallos de página
        int indice_elegido = -1;
        int pagina_acceder = -1;
        bool encontrado_en_swap = false;
        
        // Primero buscar procesos con páginas en SWAP
        for (int idx : indices_activos) {
            Proceso& p = procesos[idx];
            for (int i = 0; i < p.num_paginas; ++i) {
                int indice_pag = p.indices_paginas[i];
                if (!paginas[indice_pag].en_ram) {
                    indice_elegido = idx;
                    pagina_acceder = i;
                    encontrado_en_swap = true;
                    break;
                }
            }
            if (encontrado_en_swap) break;
        }
        
        // Si no hay páginas en swap, elegir proceso y página aleatoria
        if (!encontrado_en_swap) {
            uniform_int_distribution<int> dist_proceso(0, indices_activos.size() - 1);
            indice_elegido = indices_activos[dist_proceso(generador_aleatorio)];
            Proceso& p = procesos[indice_elegido];
            uniform_int_distribution<int> dist_pagina(0, p.num_paginas - 1);
            pagina_acceder = dist_pagina(generador_aleatorio);
        }
        
        // Obtener la página específica
        Proceso& proceso = procesos[indice_elegido];
        int indice_pagina = proceso.indices_paginas[pagina_acceder];
        Pagina& pagina = paginas[indice_pagina];
        
        cout << COLOR_INFO << "[ACCESO] PID=" << proceso.pid 
             << " → Página: " << pagina.id_global << " (local:" << pagina.id_pagina << ")"
             << " → En RAM: " << (pagina.en_ram ? "Sí" : "No") << COLOR_RESET << endl;
        
        // Si la página ya está en RAM, acceso normal
        if (pagina.en_ram) {
            cout << COLOR_RAM << "  → Página " << pagina.id_global 
                 << " YA en RAM (marco " << pagina.indice_marco << ")" << COLOR_RESET << endl;
            return true;
        }
        
        // Esto es para los fallos de página
        cout << COLOR_FALLO << "  → FALLO DE PÁGINA! Página " << pagina.id_global 
             << " no está en RAM" << COLOR_RESET << endl;
        fallos_pagina++; // Incrementar contador de fallos de página
        
        // Buscar marco libre en RAM
        int marco_libre = encontrarMarcoLibre(marcos_ram);
        
        if (marco_libre == -1) {
            // RAM llena, necesitamos reemplazar una página usando FIFO
            cout << COLOR_FALLO << "  → RAM llena, buscando víctima para reemplazar..." << COLOR_RESET << endl;
            int indice_victima = elegirPaginaVictima();
            
            if (indice_victima == -1) {
                cout << COLOR_ERROR << "  → ERROR: No se encontró víctima para reemplazar" << COLOR_RESET << endl;
                return false;
            }
            
            Pagina& victima = paginas[indice_victima];
            cout << COLOR_SWAP << "  → Víctima seleccionada: Página " << victima.id_global 
                 << " (PID=" << victima.pid << "-" << victima.id_pagina 
                 << ") (marco " << victima.indice_marco << ")" << COLOR_RESET << endl;
            
            // Mover víctima a SWAP
            int marco_swap_libre = encontrarMarcoLibre(marcos_swap);
            if (marco_swap_libre == -1) {
                cout << COLOR_ERROR << "  → ERROR: No hay espacio en SWAP" << COLOR_RESET << endl;
                return false;
            }
            
            // Realizar swap-out de la víctima
            marcos_swap[marco_swap_libre] = indice_victima;
            marcos_ram[victima.indice_marco] = -1; // Liberar marco en RAM
            
            victima.en_ram = false;
            victima.indice_swap = marco_swap_libre;
            cout << COLOR_SWAP << "  → Víctima movida a SWAP (marco " << marco_swap_libre << ")" << COLOR_RESET << endl;
            
            marco_libre = victima.indice_marco; // Usar el marco liberado
            victima.indice_marco = -1;
        }
        
        // Mover la página solicitada a RAM (swap-in)
        if (pagina.indice_swap != -1) {
            marcos_swap[pagina.indice_swap] = -1;  // Liberar espacio en SWAP
            cout << COLOR_SWAP << "  → Página " << pagina.id_global 
                 << " liberada de SWAP (marco " << pagina.indice_swap << ")" << COLOR_RESET << endl;
        }
        
        // Actualizar estado de la página
        pagina.en_ram = true;
        pagina.indice_marco = marco_libre;
        pagina.indice_swap = -1;
        pagina.tiempo_carga = contador_carga++; // Actualizar tiempo de carga para FIFO
        
        marcos_ram[marco_libre] = indice_pagina; // Asignar marco en RAM
        
        cout << COLOR_RAM << "  → Página " << pagina.id_global 
             << " movida a RAM (marco " << marco_libre << ")" << COLOR_RESET << endl;
        return true;
    }

    // Muestra el estado actual del sistema
    void mostrarEstado() {
        // Calcular uso de RAM
        int ram_usada = marcos_ram.size() - count(marcos_ram.begin(), marcos_ram.end(), -1);
        // Calcular uso de SWAP
        int swap_usada = marcos_swap.size() - count(marcos_swap.begin(), marcos_swap.end(), -1);
        // Contar procesos activos
        int procesos_activos = 0;
        for (const auto& p : procesos) {
            if (p.activo) procesos_activos++;
        }
        
        // Contar páginas actualmente en SWAP
        int paginas_en_swap = 0;
        for (const auto& pagina : paginas) {
            if (!pagina.en_ram && pagina.pid != -1) {
                paginas_en_swap++;
            }
        }
        
        // Mostrar estado completo
        cout << COLOR_INFO << "[ESTADO] RAM: " << ram_usada << "/" << marcos_ram.size()
             << " | SWAP: " << swap_usada << "/" << marcos_swap.size()
             << " | Procesos: " << procesos_activos
             << " | Páginas en SWAP: " << paginas_en_swap
             << " | Fallos de Página: " << fallos_pagina 
             << " | Total páginas: " << paginas.size() << COLOR_RESET << endl;
    }

    // Función principal que ejecuta la simulación
    void ejecutarSimulacion() {
        auto tiempo_inicio = chrono::steady_clock::now();
        auto ultima_creacion = tiempo_inicio;
        auto ultimo_periodico = tiempo_inicio;
        
        cout << COLOR_INFO << "=== INICIANDO SIMULACIÓN ===" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Nota: Los eventos periódicos (accesos y finalizaciones) comenzarán después de 30 segundos" << COLOR_RESET << endl;
        
        // Bucle principal de simulación
        while (true) {
            auto tiempo_actual = chrono::steady_clock::now();
            auto tiempo_total = chrono::duration_cast<chrono::seconds>(tiempo_actual - tiempo_inicio).count();
            auto tiempo_desde_creacion = chrono::duration_cast<chrono::seconds>(tiempo_actual - ultima_creacion).count();
            auto tiempo_desde_periodico = chrono::duration_cast<chrono::seconds>(tiempo_actual - ultimo_periodico).count();
            
            // Crear proceso cada 2 segundos
            if (tiempo_desde_creacion >= 2 && procesos_creados < 8) {
                if (!crearProceso()) {
                    cout << COLOR_ERROR << "No se puede crear más procesos. Continuando simulación..." << COLOR_RESET << endl;
                }
                mostrarEstado();
                ultima_creacion = tiempo_actual;
            }
            
            // Hacer eventos periódicos cada 5 segundos después de 30 segundos
            if (tiempo_total >= 10 && tiempo_desde_periodico >= 5) {
                cout << COLOR_INFO << "\n--- EVENTOS PERIÓDICOS (cada 5 segundos) ---" << COLOR_RESET << endl;
                
                // Finalizar proceso aleatorio (solo si hay procesos)
                if (procesos_creados > 0) {
                    finalizarProcesoAleatorio();
                    mostrarEstado();
                }
                
                // Simular acceso a memoria
                bool hay_activos = false;
                for (const auto& p : procesos) {
                    if (p.activo) {
                        hay_activos = true;
                        break;
                    }
                }
                
                if (hay_activos) {
                    cout << COLOR_INFO << "--- ACCESO A MEMORIA ALEATORIO ---" << COLOR_RESET << endl;
                    if (!simularAccesoMemoria()) {
                        cout << COLOR_ERROR << "Error en acceso a memoria. Continuando..." << COLOR_RESET << endl;
                    }
                    mostrarEstado();
                }
                
                ultimo_periodico = tiempo_actual;
            }
            
            // Verificar si hay memoria disponible 
            int ram_libre = count(marcos_ram.begin(), marcos_ram.end(), -1);
            int swap_libre = count(marcos_swap.begin(), marcos_swap.end(), -1);
            
            if (ram_libre == 0 && swap_libre == 0) {
                cout << COLOR_ERROR << "Memoria agotada. Finalizando simulación." << COLOR_RESET << endl;
                break;
            }
            
            // Terminar después de 60 segundos 
            if (tiempo_total >= 60) {
                cout << COLOR_INFO << "Tiempo de simulación completado. Finalizando." << COLOR_RESET << endl;
                break;
            }
            
            // Pausa para no sobrecargar la CPU
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        
        // Mostrar estadísticas finales
        auto tiempo_final = chrono::steady_clock::now();
        auto duracion_total = chrono::duration_cast<chrono::seconds>(tiempo_final - tiempo_inicio).count();
        
        cout << COLOR_INFO << "\n=== ESTADÍSTICAS FINALES ===" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Procesos creados: " << procesos_creados << COLOR_RESET << endl;
        cout << COLOR_INFO << "Procesos finalizados: " << procesos_finalizados << COLOR_RESET << endl;
        cout << COLOR_INFO << "Fallos de página: " << fallos_pagina << COLOR_RESET << endl;
        cout << COLOR_INFO << "Páginas totales creadas: " << paginas.size() << COLOR_RESET << endl;
        cout << COLOR_INFO << "Tiempo total de simulación: " << duracion_total << " segundos" << COLOR_RESET << endl;
        cout << COLOR_INFO << "============================" << COLOR_RESET << endl;
    }
};

// ============================================================================
// FUNCIÓN PRINCIPAL - PUNTO DE ENTRADA DEL PROGRAMA
// ============================================================================

int main() {
    cout << "=== SIMULADOR DE PAGINACIÓN - SISTEMAS OPERATIVOS ===" << endl;
    cout << "Implementación de memoria virtual con política de reemplazo FIFO" << endl;
    cout << "================================================================" << endl;
    
    int memoria_fisica, tamano_pagina, proceso_min, proceso_max;
    
    // Solicitar parámetros de configuración al usuario
    cout << "Tamaño memoria física (MB): ";
    cin >> memoria_fisica;
    cout << "Tamaño de página (MB): ";
    cin >> tamano_pagina;
    cout << "Tamaño mínimo de proceso (MB): ";
    cin >> proceso_min;
    cout << "Tamaño máximo de proceso (MB): ";
    cin >> proceso_max;
    
    // Validaciones básicas de entrada
    if (memoria_fisica <= 0 || tamano_pagina <= 0 || proceso_min <= 0 || proceso_max <= 0) {
        cout << COLOR_ERROR << "Error: Todos los valores deben ser positivos" << COLOR_RESET << endl;
        return 1;
    }
    
    if (proceso_min > proceso_max) {
        cout << COLOR_ERROR << "Error: El tamaño mínimo no puede ser mayor al máximo" << COLOR_RESET << endl;
        return 1;
    }
    
    // Crear y ejecutar el simulador
    SimuladorMemoria simulador(memoria_fisica, tamano_pagina, proceso_min, proceso_max);
    simulador.ejecutarSimulacion();
    
    return 0;
}