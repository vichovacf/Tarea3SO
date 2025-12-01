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

// Colores para la consola
const string COLOR_RAM   = "\033[32m";  // Verde
const string COLOR_SWAP  = "\033[36m";  // Cyan
const string COLOR_INFO  = "\033[33m";  // Amarillo
const string COLOR_ERROR = "\033[31m";  // Rojo
const string COLOR_FAULT = "\033[35m";  // Magenta
const string COLOR_RESET = "\033[0m";   // Reset

// ============================================================================
// ESTRUCTURAS DE DATOS
// ============================================================================

struct Page {
    int pid;                    // ID del proceso dueño
    int page_id;               // ID local de la página en el proceso
    bool in_ram;               // True si está en RAM
    int frame_index;           // Índice en RAM (-1 si no está)
    int swap_index;            // Índice en SWAP (-1 si no está)
    unsigned long long load_time; // Tiempo de carga (para FIFO)
    
    Page(int p_pid, int p_page_id) 
        : pid(p_pid), page_id(p_page_id), in_ram(false), 
          frame_index(-1), swap_index(-1), load_time(0) {}
};

struct Process {
    int pid;                   // ID del proceso
    int size_mb;              // Tamaño en MB
    int page_count;           // Número de páginas
    bool active;              // Si está activo
    vector<int> page_indices; // Índices a las páginas en el vector global
    
    Process(int p_pid, int p_size_mb, int p_page_count)
        : pid(p_pid), size_mb(p_size_mb), page_count(p_page_count), 
          active(true) {}
};

// ============================================================================
// CLASE PRINCIPAL - SIMULADOR DE MEMORIA
// ============================================================================

class MemorySimulator {
private:
    // Configuración
    int physical_memory_mb;
    int page_size_mb;
    int min_process_mb;
    int max_process_mb;
    
    // Memoria
    vector<int> ram_frames;    // -1 = libre, índice = página
    vector<int> swap_frames;   // -1 = libre, índice = página
    
    // Procesos y páginas
    vector<Process> processes;
    vector<Page> pages;
    
    // Contadores y estado
    int next_pid;
    unsigned long long load_counter;
    
    // Estadísticas
    int page_faults;
    int processes_created;
    int processes_finished;
    
    // Random generator
    mt19937 random_generator;

public:
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
        
        initializeMemory();
    }

private:
    void initializeMemory() {
        // Calcular memoria virtual (1.5 a 4.5 veces física)
        uniform_real_distribution<double> factor_dist(1.5, 4.5);
        double virtual_memory_mb = physical_memory_mb * factor_dist(random_generator);
        
        // Calcular frames
        int ram_frames_count = physical_memory_mb / page_size_mb;
        int total_frames_count = virtual_memory_mb / page_size_mb;
        int swap_frames_count = total_frames_count - ram_frames_count;
        
        if (ram_frames_count <= 0) ram_frames_count = 1;
        if (swap_frames_count <= 0) swap_frames_count = total_frames_count; // Asegurar suficiente swap
        
        // Inicializar memoria
        ram_frames.resize(ram_frames_count, -1);
        swap_frames.resize(swap_frames_count, -1);
        
        cout << COLOR_INFO << "=== CONFIGURACIÓN INICIAL ===" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Memoria Física: " << physical_memory_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Memoria Virtual: " << virtual_memory_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Tamaño Página: " << page_size_mb << " MB" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Frames RAM: " << ram_frames_count << COLOR_RESET << endl;
        cout << COLOR_INFO << "Frames SWAP: " << swap_frames_count << COLOR_RESET << endl;
        cout << COLOR_INFO << "=============================" << COLOR_RESET << endl;
    }

    int findFreeFrame(const vector<int>& frames) {
        for (size_t i = 0; i < frames.size(); ++i) {
            if (frames[i] == -1) return i;
        }
        return -1;
    }

    int chooseVictimPage() {
        // Política FIFO: elegir la página más antigua
        int victim = -1;
        unsigned long long oldest_time = numeric_limits<unsigned long long>::max();
        
        for (size_t i = 0; i < pages.size(); ++i) {
            if (pages[i].in_ram && pages[i].pid != -1 && pages[i].load_time < oldest_time) {
                oldest_time = pages[i].load_time;
                victim = i;
            }
        }
        return victim;
    }

public:
    bool createProcess() {
        uniform_int_distribution<int> size_dist(min_process_mb, max_process_mb);
        int process_size = size_dist(random_generator);
        int page_count = ceil(static_cast<double>(process_size) / page_size_mb);
        
        Process new_process(next_pid++, process_size, page_count);
        
        cout << COLOR_INFO << "[CREACIÓN] Proceso PID=" << new_process.pid 
             << " (" << process_size << " MB, " << page_count << " páginas)" << COLOR_RESET << endl;
        
        // Verificar memoria disponible
        int free_ram = count(ram_frames.begin(), ram_frames.end(), -1);
        int free_swap = count(swap_frames.begin(), swap_frames.end(), -1);
        
        if (free_ram + free_swap < page_count) {
            cout << COLOR_ERROR << "[ERROR] Memoria insuficiente para proceso PID=" 
                 << new_process.pid << COLOR_RESET << endl;
            return false;
        }
        
        // Asignar páginas
        for (int i = 0; i < page_count; ++i) {
            Page new_page(new_process.pid, i);
            int page_index = pages.size();
            
            // Intentar cargar en RAM primero
            int free_frame = findFreeFrame(ram_frames);
            if (free_frame != -1) {
                new_page.in_ram = true;
                new_page.frame_index = free_frame;
                new_page.load_time = load_counter++;
                
                ram_frames[free_frame] = page_index;
                cout << COLOR_RAM << "  → Página " << i << " cargada en RAM (frame " 
                     << free_frame << ")" << COLOR_RESET << endl;
            } else {
                // Usar SWAP
                free_frame = findFreeFrame(swap_frames);
                if (free_frame == -1) {
                    cout << COLOR_ERROR << "  → ERROR: No hay frames libres en SWAP" << COLOR_RESET << endl;
                    return false;
                }
                new_page.in_ram = false;
                new_page.swap_index = free_frame;
                
                swap_frames[free_frame] = page_index;
                cout << COLOR_SWAP << "  → Página " << i << " asignada a SWAP (frame " 
                     << free_frame << ")" << COLOR_RESET << endl;
            }
            
            pages.push_back(new_page);
            new_process.page_indices.push_back(page_index);
        }
        
        processes.push_back(new_process);
        processes_created++;
        return true;
    }

    void finishRandomProcess() {
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
        
        uniform_int_distribution<int> dist(0, active_indices.size() - 1);
        int process_index = active_indices[dist(random_generator)];
        Process& process = processes[process_index];
        
        cout << COLOR_INFO << "[FINALIZACIÓN] Terminando proceso PID=" 
             << process.pid << COLOR_RESET << endl;
        
        // Liberar páginas
        for (int page_index : process.page_indices) {
            Page& page = pages[page_index];
            if (page.in_ram) {
                ram_frames[page.frame_index] = -1;
                cout << COLOR_RAM << "  → Liberada página " << page.page_id << " de RAM (frame " << page.frame_index << ")" << COLOR_RESET << endl;
            } else {
                swap_frames[page.swap_index] = -1;
                cout << COLOR_SWAP << "  → Liberada página " << page.page_id << " de SWAP (frame " << page.swap_index << ")" << COLOR_RESET << endl;
            }
            // Marcar página como liberada
            page.pid = -1;
        }
        
        process.active = false;
        process.page_indices.clear();
        processes_finished++;
    }

    bool simulateMemoryAccess() {
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
        
        // ESTRATEGIA MEJORADA: Buscar específicamente páginas en SWAP
        int chosen_index = -1;
        int page_to_access = -1;
        bool found_in_swap = false;
        
        // Primero buscar procesos con páginas en SWAP
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
        
        // Si no hay páginas en swap, elegir aleatorio
        if (!found_in_swap) {
            uniform_int_distribution<int> proc_dist(0, active_indices.size() - 1);
            chosen_index = active_indices[proc_dist(random_generator)];
            Process& p = processes[chosen_index];
            uniform_int_distribution<int> page_dist(0, p.page_count - 1);
            page_to_access = page_dist(random_generator);
        }
        
        Process& process = processes[chosen_index];
        int page_index = process.page_indices[page_to_access];
        Page& page = pages[page_index];
        
        cout << COLOR_INFO << "[ACCESO] PID=" << process.pid 
             << " → Página: " << page_to_access 
             << " → En RAM: " << (page.in_ram ? "Sí" : "No") << COLOR_RESET << endl;
        
        if (page.in_ram) {
            cout << COLOR_RAM << "  → Página YA en RAM (frame " << page.frame_index << ")" << COLOR_RESET << endl;
            return true;
        }
        
        // PAGE FAULT - Página no está en RAM
        cout << COLOR_FAULT << "  → PAGE FAULT! Página no está en RAM" << COLOR_RESET << endl;
        page_faults++;
        
        // Buscar frame libre en RAM
        int free_frame = findFreeFrame(ram_frames);
        
        if (free_frame == -1) {
            // NECESITAMOS REEMPLAZAR - FIFO
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
            
            // Mover víctima a SWAP
            swap_frames[free_swap_frame] = victim_index;
            ram_frames[victim.frame_index] = -1;
            
            victim.in_ram = false;
            victim.swap_index = free_swap_frame;
            cout << COLOR_SWAP << "  → Víctima movida a SWAP (frame " << free_swap_frame << ")" << COLOR_RESET << endl;
            
            free_frame = victim.frame_index;
            victim.frame_index = -1;
        }
        
        // MOVER PÁGINA SOLICITADA A RAM
        if (page.swap_index != -1) {
            swap_frames[page.swap_index] = -1;  // Liberar espacio en SWAP
            cout << COLOR_SWAP << "  → Página liberada de SWAP (frame " << page.swap_index << ")" << COLOR_RESET << endl;
        }
        
        page.in_ram = true;
        page.frame_index = free_frame;
        page.swap_index = -1;
        page.load_time = load_counter++;
        
        ram_frames[free_frame] = page_index;
        
        cout << COLOR_RAM << "  → Página movida a RAM (frame " << free_frame << ")" << COLOR_RESET << endl;
        return true;
    }

    void printStatus() {
        int ram_used = ram_frames.size() - count(ram_frames.begin(), ram_frames.end(), -1);
        int swap_used = swap_frames.size() - count(swap_frames.begin(), swap_frames.end(), -1);
        int active_processes = 0;
        for (const auto& p : processes) {
            if (p.active) active_processes++;
        }
        
        // Contar páginas en swap
        int pages_in_swap = 0;
        for (const auto& page : pages) {
            if (!page.in_ram && page.pid != -1) {
                pages_in_swap++;
            }
        }
        
        cout << COLOR_INFO << "[ESTADO] RAM: " << ram_used << "/" << ram_frames.size()
             << " | SWAP: " << swap_used << "/" << swap_frames.size()
             << " | Procesos: " << active_processes
             << " | Páginas en SWAP: " << pages_in_swap
             << " | Page Faults: " << page_faults << COLOR_RESET << endl;
    }

    void runSimulation() {
        auto start_time = chrono::steady_clock::now();
        auto last_creation = start_time;
        auto last_periodic = start_time;
        
        cout << COLOR_INFO << "=== INICIANDO SIMULACIÓN ===" << COLOR_RESET << endl;
        cout << COLOR_INFO << "Nota: Los eventos periódicos (accesos y finalizaciones) comenzarán después de 30 segundos" << COLOR_RESET << endl;
        
        while (true) {
            auto current_time = chrono::steady_clock::now();
            auto elapsed_total = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
            auto elapsed_since_create = chrono::duration_cast<chrono::seconds>(current_time - last_creation).count();
            auto elapsed_since_periodic = chrono::duration_cast<chrono::seconds>(current_time - last_periodic).count();
            
            // Crear proceso cada 2 segundos (solo hasta que tengamos algunos procesos)
            if (elapsed_since_create >= 2 && processes_created < 8) { // Limitar creación para no llenar memoria demasiado rápido
                if (!createProcess()) {
                    cout << COLOR_ERROR << "No se puede crear más procesos. Continuando simulación..." << COLOR_RESET << endl;
                    // No break, continuar con eventos periódicos
                }
                printStatus();
                last_creation = current_time;
            }
            
            // EVENTOS PERIÓDICOS CADA 5 SEGUNDOS DESPUÉS DE 30 SEGUNDOS
            if (elapsed_total >= 10 && elapsed_since_periodic >= 5) { // Cambiado a 10 segundos para demo más rápida
                cout << COLOR_INFO << "\n--- EVENTOS PERIÓDICOS (cada 5 segundos) ---" << COLOR_RESET << endl;
                
                // 1. Finalizar proceso aleatorio (si hay procesos)
                if (processes_created > 0) {
                    finishRandomProcess();
                    printStatus();
                }
                
                // 2. Simular acceso a memoria (si hay procesos activos)
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
            
            // Terminar después de 60 segundos para demo
            if (elapsed_total >= 60) {
                cout << COLOR_INFO << "Tiempo de simulación completado. Finalizando." << COLOR_RESET << endl;
                break;
            }
            
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
// FUNCIÓN MAIN
// ============================================================================

int main() {
    cout << "=== SIMULADOR DE PAGINACIÓN - SISTEMAS OPERATIVOS ===" << endl;
    cout << "Implementación de memoria virtual con política de reemplazo FIFO" << endl;
    cout << "================================================================" << endl;
    
    int physical_memory, page_size, min_process, max_process;
    
    cout << "Tamaño memoria física (MB): ";
    cin >> physical_memory;
    cout << "Tamaño de página (MB): ";
    cin >> page_size;
    cout << "Tamaño mínimo de proceso (MB): ";
    cin >> min_process;
    cout << "Tamaño máximo de proceso (MB): ";
    cin >> max_process;
    
    // Validaciones básicas
    if (physical_memory <= 0 || page_size <= 0 || min_process <= 0 || max_process <= 0) {
        cout << COLOR_ERROR << "Error: Todos los valores deben ser positivos" << COLOR_RESET << endl;
        return 1;
    }
    
    if (min_process > max_process) {
        cout << COLOR_ERROR << "Error: El tamaño mínimo no puede ser mayor al máximo" << COLOR_RESET << endl;
        return 1;
    }
    
    MemorySimulator simulator(physical_memory, page_size, min_process, max_process);
    simulator.runSimulation();
    
    return 0;
}