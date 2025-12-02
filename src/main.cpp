#include <iostream>
#include <stdint.h> // Para usar tipos de tamaño fijo (uint8_t, uint32_t)
#include <stdlib.h>


// ##################################################################
//                          Engine Types (Agnóstico a la Plataforma)
// ##################################################################
// Esta estructura la puede ver TODO el programa, no solo Windows.
// Aquí guardaremos los datos crudos de los píxeles.
struct GameBuffer {
    void* memory;       // Puntero a los píxeles
    int width;
    int height;
    int pitch;          // Bytes por fila
};
// ##################################################################
//                          Platform Globals
// ##################################################################
static bool running = true;
// Este buffer global es accesible tanto por la capa de Windows como por el main
static GameBuffer global_back_buffer;


// ##################################################################
//                          Platform Functions
// ##################################################################

bool platform_create_window(int width, int height, char* title); 
void platform_update_window(); // Procesa mensajes (Teclado/Mouse)
void platform_blit_to_window(); // Copia el buffer a la pantalla (Nuevo)
// ##################################################################
//                          Windows Platform
// ##################################################################
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// ##################################################################
//                          Windows Globals
// ##################################################################
static HWND window;
static BITMAPINFO bitmap_info; // Esto es privado de Windows, main no necesita verlo
// ##################################################################
//                         Windows interval functions
// ##################################################################

// Esta función se encarga de reservar la memoria RAM
void win32_resize_DIB_section(GameBuffer* buffer, int width, int height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    int bytes_per_pixel = 4;
    buffer->pitch = width * bytes_per_pixel; 

    // Configuramos la info para Windows
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height; // Negativo = Top-Down
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    int bitmap_memory_size = (width * height) * bytes_per_pixel;
    
    // Pedimos memoria al sistema
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
}
// ##################################################################
//                         Windows callbacks
// ##################################################################
LRESULT CALLBACK windows_window_callback(HWND window, UINT msg,
                                        WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    switch (msg)
    {
        case WM_CLOSE:
        
        {
            running = false;
            break;
        }

        // Sin esto, nunca reservamos memoria para dibujar
        case WM_SIZE:
        {
            RECT rect;
            GetClientRect(window, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            
            win32_resize_DIB_section(&global_back_buffer, width, height);
            break;
        }

        default:
        {
            // Let windows handle the default input for now
            result = DefWindowProcA(window, msg, wParam, lParam);
        }
    }

    return result;
}

// ##################################################################
//                         Platform Implementations 
// ##################################################################
bool platform_create_window(int width, int height, char* title) 
{
    HINSTANCE instance = GetModuleHandleA(0);

    WNDCLASSA wc = {};
    wc.hInstance = instance;
    wc.hIcon = LoadIcon(instance, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); // This means we decide the look of the cursor(arrow)
    wc.lpszClassName = title; // This is NOT the title, just a unique identifier
    wc.lpfnWndProc = windows_window_callback; // Callback for Input into the Window

    if(!RegisterClassA(&wc)) return false; 



    // WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
    int dwStyle = WS_OVERLAPPEDWINDOW;

    window = CreateWindowExA(0, title,  //This reference lpszClassName from wc
        title,  //This is the actual Title
        dwStyle,
        100,
        100,
        width,
        height,
        NULL, // parent
        NULL, //menu
        instance,
        NULL //lpParam
        );
    
    if(window == NULL) return false;

    ShowWindow(window, SW_SHOW);

    return true;

}

void platform_update_window(){
    MSG msg;

    while(PeekMessageA(&msg, window, 0, 0, PM_REMOVE)){
        TranslateMessage(&msg);
        DispatchMessageA(&msg); // Calls the callback specified when creating  the window
    }
}
void platform_blit_to_window() {
    HDC device_context = GetDC(window);
    
    // Obtenemos dimensiones actuales
    RECT window_rect;
    GetClientRect(window, &window_rect);
    int window_width = window_rect.right - window_rect.left;
    int window_height = window_rect.bottom - window_rect.top;

    StretchDIBits(
        device_context,
        0, 0, window_width, window_height,
        0, 0, global_back_buffer.width, global_back_buffer.height,
        global_back_buffer.memory,
        &bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
    
    ReleaseDC(window, device_context);
}
#endif // _WIN32

// ##################################################################
//                          Main (Game Loop)
// ##################################################################

// Función auxiliar para dibujar el degradado (Simula ser el juego)
void render_game(GameBuffer* buffer, int x_offset, int y_offset) {
    // PASO 1: El Puntero Inicial
    // "row" apunta al byte número 0 de nuestra memoria (la esquina de arriba a la izquierda).
    // Usamos uint8_t (1 byte) porque queremos poder movernos byte por byte si es necesario.
    uint8_t* row = (uint8_t*)buffer->memory;

    // Bucle Y: Vamos fila por fila (de arriba a abajo)
    for (int y = 0; y < buffer->height; ++y) {
        // PASO 2: El Puntero de Píxeles
        // Aquí hacemos un CAST (Transformación).
        // Le decimos al compilador: "Oye, en esta fila, deja de ver bytes sueltos (uint8).
        // Quiero que veas bloques de 4 bytes (uint32)".
        // ¿Por qué? Porque 1 píxel = 4 bytes. 
        // Escribir un uint32 es pintar un píxel entero de golpe.
        uint32_t* pixel = (uint32_t*)row;

        // Bucle X: Vamos columna por columna (de izquierda a derecha)
        for (int x = 0; x < buffer->width; ++x) {
            // PASO 3: Crear el color (Matemáticas)
            // x_offset y y_offset son números que crecen (0, 1, 2...) en cada frame.
            // Al sumar (x + x_offset), el valor del color cambia según la posición.
            // Como es uint8, si llega a 255 y sumas 1, vuelve a 0 (efecto ciclo).
            uint8_t blue = (x + x_offset);
            uint8_t green = (y + y_offset);
            

            // PASO 4: Empaquetar los bits (Bit Shifting)
            // Esto es: 00000000(R) 11111111(G) 00000000(B)
            // El operador '|' pega los trozos. '<< 8' empuja el verde a su sitio.
            *pixel = (rand() % 255) | ((rand() % 255) << 8) | ((rand() % 255) << 16);

            // PASO 5: Avanzar
            // Como 'pixel' es uint32, al hacer ++, el puntero salta 4 bytes automáticamente.
            // Pasamos al siguiente píxel a la derecha.
            pixel++; 
        }
        // PASO 6: Bajar de renglón
        // Terminamos la fila X. Ahora tenemos que saltar al inicio de la siguiente fila Y.
        // Sumamos el 'pitch' a nuestro puntero original de bytes.
        row += buffer->pitch;
    }
}
int main() { 
    std::cout << "Inicializando strangerEngine..." << std::endl;

    int width = 1200;
    int height = 720;

    char* title = "strangerEngine v0.1";

    // 1. Inicialización Plataforma
    if (!platform_create_window(1280, 720, title)) return -1;

    std::cout << "Motor corriendo..." << std::endl;

    int x_offset = 0;
    int y_offset = 0;

    // 2. Bucle Principal
    while(running){
        
        // A. Input / Mensajes del Sistema
        platform_update_window();

        // B. Game Logic & Render (Dibujar en RAM)
        // Nota: Aquí pasamos el buffer genérico, no sabemos nada de Windows
        if (global_back_buffer.memory) {
            render_game(&global_back_buffer, x_offset, y_offset);
        }

        // Animación simple
        x_offset++;
        y_offset += 2;

        // C. Blit (Copiar RAM a Ventana)
        // Aquí le decimos a la plataforma: "Ya dibujé, muéstralo".
        #ifdef _WIN32
        platform_blit_to_window();
        #endif
    }

    std::cout << "Apagando strangerEngine." << std::endl;
    return 0;
}