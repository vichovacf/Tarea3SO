Integrantes:
Vicente Cataldo
Cristopher Vasquez

Video de demostracion:
Enlace directo:https://youtu.be/dgw28pb24i0  

Descripción:
Simulador en C++ del mecanismo de paginación que implementa memoria virtual con swap, creación de procesos, fallos de página y reemplazo FIFO.

Para compilar:
entrar a la carpeta, luego
g++ Tarea3.cpp -o  ejecutable
./ejecutable


El programa solicita:
Tamaño memoria física (MB)
Tamaño de página (MB)
Tamaño mínimo de proceso (MB)
Tamaño máximo de proceso (MB)

El simulador implementa todas las funcionalidades solicitadas:
Configuración de memoria calculando memoria virtual (1.5-4.5× física)
Creación de procesos cada 2 segundos con tamaño aleatorio
Eventos periódicos cada 5 segundos después del tiempo inicial:
    Finalización de proceso aleatorio
    Acceso a dirección virtual aleatoria
Manejo de fallos de página detectando y resolviendo page faults
Política FIFO para reemplazo claro de páginas cuando RAM está llena
Terminación cuando memoria RAM y SWAP se agotan
Output claro con colores diferenciados y mensajes descriptivos


Principales componentes:
SimuladorMemoria: Clase principal
Pagina: Representa una página con estado y ubicación 
Proceso: Representa proceso con sus páginas
marcos_ram, marcos_swap: Vectores que simulan memoria

Algoritmos implementados:
Para la asignacion utilizamos Ram primero luego utilizamos SWAP. Luego utilizando FIFO para el reemplazo cuando la RAM está llena, se reemplaza la página que lleva más tiempo en memoria. Implementado mediante el contador tiempo_carga en cada página. y para la busqueda utilizamos el mas antiguo

Posibles salidas:
1. Creación de proceso exitoso
[CREACIÓN] Proceso PID=1 (6 MB, 3 páginas)
  → Página 1 (PID=1-0) cargada en RAM (marco 0)
  → Página 2 (PID=1-1) cargada en RAM (marco 1)
  → Página 3 (PID=1-2) cargada en RAM (marco 2)
Que el programa crea procesos con tamaño aleatorio (6 MB se convierte en 3 páginas de 2 MB cada una) y asigna páginas a RAM mientras haya espacio.

2. Asignación a SWAP (RAM llena)
[CREACIÓN] Proceso PID=2 (5 MB, 3 páginas)
  → Página 4 (PID=2-0) cargada en RAM (marco 3)  ← Último espacio en RAM
  → Página 5 (PID=2-1) asignada a SWAP (marco 0) ← Ya no cabe en RAM
  → Página 6 (PID=2-2) asignada a SWAP (marco 1) ← Va a SWAP
Cuando la RAM se llena (4 marcos ocupados), las nuevas páginas van a SWAP. Esto es clave para la memoria virtual.

3. Error - Memoria insuficiente

[ERROR] Memoria insuficiente para proceso PID=4
No se puede crear más procesos. Continuando simulación...

Cuando la suma de RAM libre + SWAP libre es menor que las páginas que necesita el nuevo proceso. El programa maneja este error sin crashear.

4. Fallo de página
[ACCESO] PID=2 → Página: 5 (local:1) → En RAM: No
  → FALLO DE PÁGINA! Página 5 no está en RAM

Un page fault ocurre cuando se intenta acceder a una página que está en SWAP, no en RAM. El sistema operativo real debe traerla.

5. Reemplazo FIFO
  → RAM llena, buscando víctima para reemplazar...
  → Víctima seleccionada: Página 1 (PID=1-0) (marco 0)
  → Víctima movida a SWAP (marco 2)
  → Página 5 movida a RAM (marco 0)

¿Cuando funciona?
RAM está llena (4/4 marcos ocupados)
Necesita traer Página 5 desde SWAP
Usa FIFO: elige la página más antigua (Página 1)
Mueve Página 1 a SWAP (swap-out)
Trae Página 5 a RAM (swap-in)

6. Finalización de proceso
[FINALIZACIÓN] Terminando proceso PID=1
  → Liberada página 1 (PID=1-0) de RAM (marco 0)
  → Liberada página 2 (PID=1-1) de RAM (marco 1)
  → Liberada página 3 (PID=1-2) de RAM (marco 2)

Libera TODA la memoria del proceso (tanto RAM como SWAP). Los marcos quedan disponibles (-1).

7. Memoria agotada
Memoria agotada. Finalizando simulación.

¿Cuándo ocurre? Cuando RAM libre = 0 Y SWAP libre = 0. No hay dónde poner nuevas páginas.

8. Tiempo completado
Tiempo de simulación completado. Finalizando.

Termina después de 60 segundos para no ejecutar indefinidamente.

9. Estadísticas finales
=== ESTADÍSTICAS FINALES ===
Procesos creados: 
Procesos finalizados: 
Fallos de página: 
Páginas totales creadas: 
Tiempo total de simulación:

Eficiencia: Cuántos fallos de página hubo
Carga: Cuántos procesos se manejaron
Rendimiento: Tiempo total de simulación

