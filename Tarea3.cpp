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
const string COLOR_FAULT = "\033[35m";  // Magenta para page faults
const string COLOR_RESET = "\033[0m";   // Resetear color

// ============================================================================
// ESTRUCTURAS DE DATOS
// ============================================================================

// Estructura que representa una página de memoria
struct Page {
    int pid;                    // ID del proceso dueño de esta página
    int page_id;               // ID local de la página dentro del proceso (0, 1, 2...)
    bool in_ram;               // True si la página está actualmente en RAM
    int frame_index;           // Índice del frame en RAM (-1 si no está en RAM)
    int swap_index;            // Índice del frame en SWAP (-1 si no está en SWAP)
    unsigned long long load_time; // Tiempo de carga (para política FIFO)
    
    // Constructor de la página
    Page(int p_pid, int p_page_id) 
        : pid(p_pid), page_id(p_page_id), in_ram(false), 
          frame_index(-1), swap_index(-1), load_time(0) {}
};

// Estructura que representa un proceso
struct Process {
    int pid;                   // ID único del proceso
    int size_mb;              // Tamaño total del proceso en MB
    int page_count;           // Número de páginas que ocupa el proceso
    bool active;              // Si el proceso está activo (no finalizado)
    vector<int> page_indices; // Índices de las páginas en el vector global 'pages'
    
    // Constructor del proceso
    Process(int p_pid, int p_size_mb, int p_page_count)
        : pid(p_pid), size_mb(p_size_mb), page_count(p_page_count), 
          active(true) {}
};

// ============================================================================
// CLASE PRINCIPAL - SIMULADOR DE MEMORIA
// ============================================================================

class MemorySimulator {
private:
    // Configuración del sistema
    int physical_memory_mb;    // Tamaño de la memoria física en MB
    int page_size_mb;         // Tamaño de cada página en MB
    int min_process_mb;       // Tamaño mínimo de proceso en MB
    int max_process_mb;       // Tamaño máximo de proceso en MB
    
    // Representación de la memoria
    vector<int> ram_frames;    // Vector de frames de RAM: -1 = libre, índice = página
    vector<int> swap_frames;   // Vector de frames de SWAP: -1 = libre, índice = página
    
    // Listas de procesos y páginas
    vector<Process> processes; // Todos los procesos creados
    vector<Page> pages;        // Todas las páginas de todos los procesos
    
    // Contadores y estado interno
    int next_pid;              // Siguiente ID disponible para procesos
    unsigned long long load_counter; // Contador para tiempo de carga (FIFO)
    
    // Estadísticas para reporte final
    int page_faults;          // Número total de page faults ocurridos
    int processes_created;    // Número total de procesos creados
    int processes_finished;   // Número total de procesos finalizados
    
    // Generador de números aleatorios
    mt19937 random_generator;

public:
    // Constructor principal - inicializa toda la simulación
    MemorySimulator(int phys_mb, int page_mb, int min_proc, int max_proc) 
        : physical_memory_mb(phys_mb), 
          page_size_mb(page_mb),
          min_process_mb(min_proc), 
          max_process_mb(max_proc),
          next_pid(1), 
          load_counter(0), 
          page_faults(0),
          processes_created(0), 
          processes_finished(0),
          random_generator(random_device{}()) {
        
        initializeMemory(); // Inicializar la memoria del sistema
    }
//waton si lees esto, te quiero muxo
private:
    // Inicializa la memoria física y virtual
    void initializeMemory() {
        // Calcular memoria virtual (entre 1.5 y 4.5 veces la física, como pide la tarea gordis)
        uniform_real_distribution<double> factor_dist(1.5, 4.5);
        double virtual_memory_mb = physical_memory_mb * factor_dist(random_generator);
        
        // Calcular número de frames
        int ram_frames_count = physical_memory_mb / page_size_mb;  // Frames en RAM
        int total_frames_count = virtual_memory_mb / page_size_mb; // Frames totales (RAM + SWAP)
        int swap_frames_count = total_frames_count - ram_frames_count; // Frames en SWAP
        
        // Asegurar valores mínimos
        if (ram_frames_count <= 0) ram_frames_count = 1;
        if (swap_frames_count <= 0) swap_frames_count = total_frames_count; // Asegurar suficiente swap
        
        // Inicializar vectores de memoria (todos libres inicialmente: -1)
        ram_frames.resize(ram_frames_count, -1);
        swap_frames.resize(swap_frames_count, -1);
        
        // Mostrar configuración inicial
        cout << COLOR_INFO << "=== CONFIGURACIÓN INICIAL ===" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Memoria Física: " << physical_memory_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Memoria Virtual: " << virtual_memory_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Tamaño Página: " << page_size_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Frames RAM: " << ram_frames_count << COLOR_RESET << endl;
        cout << COLOR_INFO << "Frames SWAP: " << swap_frames_count << COLOR_RESET << endl;
        cout << COLOR_INFO << "=============================" << COLOR_RESET << endl;
    }

    // Encuentra un frame libre en un vector de frames (RAM o SWAP)
    int findFreeFrame(const vector<int>& frames) {
        for (size_t i = 0; i < frames.size(); ++i) {
            if (frames[i] == -1) return i; // Retorna el índice del primer frame libre
        }
        return -1; // No hay frames libres
    }

    // Política FIFO: selecciona la página más antigua en RAM para reemplazar
    int chooseVictimPage() {
        int victim = -1;
        unsigned long long oldest_time = numeric_limits<unsigned long long>::max();
        
        // Buscar entre todas las páginas en RAM
        for (size_t i = 0; i < pages.size(); ++i) {
            if (pages[i].in_ram && pages[i].pid != -1 && pages[i].load_time < oldest_time) {
                oldest_time = pages[i].load_time;
                victim = i; // Actualizar víctima con la página más antigua
            }
        }
        return victim; // Retorna -1 si no hay páginas en RAM
    }

public:
    // Crea un nuevo proceso con tamaño aleatorio
    bool createProcess() {
        // Generar tamaño aleatorio del proceso dentro del rango especificado
        uniform_int_distribution<int> size_dist(min_process_mb, max_process_mb);
        int process_size = size_dist(random_generator);
        int page_count = ceil(static_cast<double>(process_size) / page_size_mb);
        
        // Crear nuevo proceso
        Process new_process(next_pid++, process_size, page_count);
        
        cout << COLOR_INFO << "[CREACIÓN] Proceso PID=" << new_process.pid 
             << " (" << process_size << " MB, " << page_count << " páginas)" << COLOR_RESET << endl;
        
        // Verificar si hay suficiente memoria total (RAM + SWAP)
        int free_ram = count(ram_frames.begin(), ram_frames.end(), -1);
        int free_swap = count(swap_frames.begin(), swap_frames.end(), -1);
        
        if (free_ram + free_swap < page_count) {
            cout << COLOR_ERROR << "[ERROR] Memoria insuficiente para proceso PID=" 
                 << new_process.pid << COLOR_RESET << endl;
            return false;
        }
        
        // Asignar páginas del proceso
        for (int i = 0; i < page_count; ++i) {
            Page new_page(new_process.pid, i);
            int page_index = pages.size();
            
            // Intentar cargar en RAM primero (política de asignación)
            int free_frame = findFreeFrame(ram_frames);
            if (free_frame != -1) {
                // Hay espacio en RAM
                new_page.in_ram = true;
                new_page.frame_index = free_frame;
                new_page.load_time = load_counter++; // Marcar tiempo de carga para FIFO
                
                ram_frames[free_frame] = page_index; // Asignar frame en RAM
                cout << COLOR_RAM << "  → Página " << i << " cargada en RAM (frame " 
                     << free_frame << ")" << COLOR_RESET << endl;
            } else {
                // RAM llena, usar SWAP
                free_frame = findFreeFrame(swap_frames);
                if (free_frame == -1) {
                    cout << COLOR_ERROR << "  → ERROR: No hay frames libres en SWAP" << COLOR_RESET << endl;
                    return false;
                }
                new_page.in_ram = false;
                new_page.swap_index = free_frame;
                
                swap_frames[free_frame] = page_index; // Asignar frame en SWAP
                cout << COLOR_SWAP << "  → Página " << i << " asignada a SWAP (frame " 
                     << free_frame << ")" << COLOR_RESET << endl;
            }
            
            // Agregar página a la lista global y al proceso
            pages.push_back(new_page);
            new_process.page_indices.push_back(page_index);
        }
        
        // Agregar proceso a la lista
        processes.push_back(new_process);
        processes_created++;
        return true;
    }

    // Finaliza un proceso aleatorio y libera su memoria
    void finishRandomProcess() {
        // Obtener índices de procesos activos
        vector<int> active_indices;
        for (size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].active) {
                active_indices.push_back(i);
            }
        }
        
        if (active_indices.empty()) {
            cout << COLOR_INFO << "[FINALIZACIÓN] No hay procesos activos" << COLOR_RESET << endl;
            return;
        }
        
        // Seleccionar proceso aleatorio para finalizar
        uniform_int_distribution<int> dist(0, active_indices.size() - 1);
        int process_index = active_indices[dist(random_generator)];
        Process& process = processes[process_index];
        
        cout << COLOR_INFO << "[FINALIZACIÓN] Terminando proceso PID=" 
             << process.pid << COLOR_RESET << endl;
        
        // Liberar todas las páginas del proceso
        for (int page_index : process.page_indices) {
            Page& page = pages[page_index];
            if (page.in_ram) {
                ram_frames[page.frame_index] = -1; // Liberar frame en RAM
                cout << COLOR_RAM << "  → Liberada página " << page.page_id << " de RAM (frame " << page.frame_index << ")" << COLOR_RESET << endl;
            } else {
                swap_frames[page.swap_index] = -1; // Liberar frame en SWAP
                cout << COLOR_SWAP << "  → Liberada página " << page.page_id << " de SWAP (frame " << page.swap_index << ")" << COLOR_RESET << endl;
            }
            // Marcar página como liberada (pid = -1)
            page.pid = -1;
        }
        
        // Marcar proceso como inactivo y limpiar sus páginas
        process.active = false;
        process.page_indices.clear();
        processes_finished++;
    }

    // Simulamos un acceso a memoria (puede causar page fault)
    bool simulateMemoryAccess() {
        // Obtener índices de procesos activos
        vector<int> active_indices;
        for (size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].active) {
                active_indices.push_back(i);
            }
        }
        
        if (active_indices.empty()) {
            cout << COLOR_INFO << "[ACCESO] No hay procesos activos" << COLOR_RESET << endl;
            return true;
        }
        
        // Buscamos específicamente páginas en SWAP para forzar page faults
        int chosen_index = -1;
        int page_to_access = -1;
        bool found_in_swap = false;
        
        // Primero buscar procesos con páginas en SWAP con esto de aca demostramos los page faults gordito 
        for (int idx : active_indices) {
            Process& p = processes[idx];
            for (int i = 0; i < p.page_count; ++i) {
                int page_idx = p.page_indices[i];
                if (!pages[page_idx].in_ram) {
                    chosen_index = idx;
                    page_to_access = i;
                    found_in_swap = true;
                    break;
                }
            }
            if (found_in_swap) break;
        }
        
        // Si no hay páginas en swap, elegir proceso y página aleatoria tipo sexo
        if (!found_in_swap) {
            uniform_int_distribution<int> proc_dist(0, active_indices.size() - 1);
            chosen_index = active_indices[proc_dist(random_generator)];
            Process& p = processes[chosen_index];
            uniform_int_distribution<int> page_dist(0, p.page_count - 1);
            page_to_access = page_dist(random_generator);
        }
        
        // Obtener la página específica
        Process& process = processes[chosen_index];
        int page_index = process.page_indices[page_to_access];
        Page& page = pages[page_index];
        
        cout << COLOR_INFO << "[ACCESO] PID=" << process.pid 
             << " → Página: " << page_to_access 
             << " → En RAM: " << (page.in_ram ? "Sí" : "No") << COLOR_RESET << endl;
        
        // Si la página ya está en RAM, acceso normal
        if (page.in_ram) {
            cout << COLOR_RAM << "  → Página YA en RAM (frame " << page.frame_index << ")" << COLOR_RESET << endl;
            return true;
        }
        
        // esto es para los page faults
        cout << COLOR_FAULT << "  → PAGE FAULT! Página no está en RAM" << COLOR_RESET << endl;
        page_faults++; // Incrementar contador de page faults
        
        // Buscar frame libre en RAM
        int free_frame = findFreeFrame(ram_frames);
        
        if (free_frame == -1) {
            // RAM llena, necesitamos reemplazar una página usando FIFO
            cout << COLOR_FAULT << "  → RAM llena, buscando víctima para reemplazar..." << COLOR_RESET << endl;
            int victim_index = chooseVictimPage();
            
            if (victim_index == -1) {
                cout << COLOR_ERROR << "  → ERROR: No se encontró víctima para reemplazar" << COLOR_RESET << endl;
                return false;
            }
            
            Page& victim = pages[victim_index];
            cout << COLOR_SWAP << "  → Víctima seleccionada: Página " << victim.page_id << " del PID=" << victim.pid 
                 << " (frame " << victim.frame_index << ")" << COLOR_RESET << endl;
            
            // Mover víctima a SWAP
            int free_swap_frame = findFreeFrame(swap_frames);
            if (free_swap_frame == -1) {
                cout << COLOR_ERROR << "  → ERROR: No hay espacio en SWAP" << COLOR_RESET << endl;
                return false;
            }
            
            // Realizar swap-out de la víctima
            swap_frames[free_swap_frame] = victim_index;
            ram_frames[victim.frame_index] = -1; // Liberar frame en RAM
            
            victim.in_ram = false;
            victim.swap_index = free_swap_frame;
            cout << COLOR_SWAP << "  → Víctima movida a SWAP (frame " << free_swap_frame << ")" << COLOR_RESET << endl;
            
            free_frame = victim.frame_index; // Usar el frame liberado
            victim.frame_index = -1;
        }
        
        // Mover la página solicitada a RAM lo pense como un swap-in
        if (page.swap_index != -1) {
            swap_frames[page.swap_index] = -1;  // Liberar espacio en SWAP
            cout << COLOR_SWAP << "  → Página liberada de SWAP (frame " << page.swap_index << ")" << COLOR_RESET << endl;
        }
        
        // Actualizar estado de la página
        page.in_ram = true;
        page.frame_index = free_frame;
        page.swap_index = -1;
        page.load_time = load_counter++; // Actualizar tiempo de carga para FIFO
        
        ram_frames[free_frame] = page_index; // Asignar frame en RAM
        
        cout << COLOR_RAM << "  → Página movida a RAM (frame " << free_frame << ")" << COLOR_RESET << endl;
        return true;
    }

    // Muestra el estado actual del sistema
    void printStatus() {
        // Calcular uso de RAM
        int ram_used = ram_frames.size() - count(ram_frames.begin(), ram_frames.end(), -1);
        // Calcular uso de SWAP
        int swap_used = swap_frames.size() - count(swap_frames.begin(), swap_frames.end(), -1);
        // Contar procesos activos
        int active_processes = 0;
        for (const auto& p : processes) {
            if (p.active) active_processes++;
        }
        
        // Contar páginas actualmente en SWAP
        int pages_in_swap = 0;
        for (const auto& page : pages) {
            if (!page.in_ram && page.pid != -1) {
                pages_in_swap++;
            }
        }
        
        // Mostrar estado completo
        cout << COLOR_INFO << "[ESTADO] RAM: " << ram_used << "/" << ram_frames.size()
             << " | SWAP: " << swap_used << "/" << swap_frames.size()
             << " | Procesos: " << active_processes
             << " | Páginas en SWAP: " << pages_in_swap
             << " | Page Faults: " << page_faults << COLOR_RESET << endl;
    }

    // Función principal que ejecuta la simulación
    void runSimulation() {
        auto start_time = chrono::steady_clock::now();
        auto last_creation = start_time;
        auto last_periodic = start_time;
        
        cout << COLOR_INFO << "=== INICIANDO SIMULACIÓN ===" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Nota: Los eventos periódicos (accesos y finalizaciones) comenzarán después de 30 segundos" << COLOR_RESET << endl;
        
        // Bucle principal de simulación
        while (true) {
            auto current_time = chrono::steady_clock::now();
            auto elapsed_total = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
            auto elapsed_since_create = chrono::duration_cast<chrono::seconds>(current_time - last_creation).count();
            auto elapsed_since_periodic = chrono::duration_cast<chrono::seconds>(current_time - last_periodic).count();
            
            // Crear proceso cada 2 segundos
            if (elapsed_since_create >= 2 && processes_created < 8) {
                if (!createProcess()) {
                    cout << COLOR_ERROR << "No se puede crear más procesos. Continuando simulación..." << COLOR_RESET << endl;
                }
                printStatus();
                last_creation = current_time;
            }
            
            // esto de aca sirve para hacer eventos periodicos cada 5
            
            if (elapsed_total >= 10 && elapsed_since_periodic >= 5) {
                cout << COLOR_INFO << "\n--- EVENTOS PERIÓDICOS (cada 5 segundos) ---" << COLOR_RESET << endl;
                
                // Finalizar proceso aleatorio ojo solo si hay proceso gordito
                if (processes_created > 0) {
                    finishRandomProcess();
                    printStatus();
                }
                
                // Simular acceso a memoria y obviamente aca lo mismo
                bool has_active = false;
                for (const auto& p : processes) {
                    if (p.active) {
                        has_active = true;
                        break;
                    }
                }
                
                if (has_active) {
                    cout << COLOR_INFO << "--- ACCESO A MEMORIA ALEATORIO ---" << COLOR_RESET << endl;
                    if (!simulateMemoryAccess()) {
                        cout << COLOR_ERROR << "Error en acceso a memoria. Continuando..." << COLOR_RESET << endl;
                    }
                    printStatus();
                }
                
                last_periodic = current_time;
            }
            
            // Verificar si hay memoria disponible 
            int free_ram = count(ram_frames.begin(), ram_frames.end(), -1);
            int free_swap = count(swap_frames.begin(), swap_frames.end(), -1);
            
            if (free_ram == 0 && free_swap == 0) {
                cout << COLOR_ERROR << "Memoria agotada. Finalizando simulación." << COLOR_RESET << endl;
                break;
            }
            
            // Terminar después de 60 segundos 
            if (elapsed_total >= 60) {
                cout << COLOR_INFO << "Tiempo de simulación completado. Finalizando." << COLOR_RESET << endl;
                break;
            }
            
            // Un sleep para el cpu jeje
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        
        // Mostrar estadísticas finales
        auto final_time = chrono::steady_clock::now();
        auto total_duration = chrono::duration_cast<chrono::seconds>(final_time - start_time).count();
        
        cout << COLOR_INFO << "\n=== ESTADÍSTICAS FINALES ===" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Procesos creados: " << processes_created << COLOR_RESET << endl;
        cout << COLOR_INFO << "Procesos finalizados: " << processes_finished << COLOR_RESET << endl;
        cout << COLOR_INFO << "Page faults: " << page_faults << COLOR_RESET << endl;
        cout << COLOR_INFO << "Tiempo total de simulación: " << total_duration << " segundos" << COLOR_RESET << endl;
        cout << COLOR_INFO << "============================" << COLOR_RESET << endl;
    }
};

// ============================================================================
// FUNCIÓN MAIN - PUNTO DE ENTRADA DEL PROGRAMA
// ============================================================================

int main() {
    cout << "=== SIMULADOR DE PAGINACIÓN - SISTEMAS OPERATIVOS ===" << endl;
    cout << "Implementación de memoria virtual con política de reemplazo FIFO" << endl;
    cout << "================================================================" << endl;
    
    int physical_memory, page_size, min_process, max_process;
    
    // Solicitar parámetros de configuración al usuario
    cout << "Tamaño memoria física (MB): ";
    cin >> physical_memory;
    cout << "Tamaño de página (MB): ";
    cin >> page_size;
    cout << "Tamaño mínimo de proceso (MB): ";
    cin >> min_process;
    cout << "Tamaño máximo de proceso (MB): ";
    cin >> max_process;
    
    // Validaciones básicas de entrada
    if (physical_memory <= 0 || page_size <= 0 || min_process <= 0 || max_process <= 0) {
        cout << COLOR_ERROR << "Error: Todos los valores deben ser positivos" << COLOR_RESET << endl;
        return 1;
    }
    
    if (min_process > max_process) {
        cout << COLOR_ERROR << "Error: El tamaño mínimo no puede ser mayor al máximo" << COLOR_RESET << endl;
        return 1;
    }
    
    // Crear y ejecutar el simulador
    MemorySimulator simulator(physical_memory, page_size, min_process, max_process);
    simulator.runSimulation();
    
    return 0;
}