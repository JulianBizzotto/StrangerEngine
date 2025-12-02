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
struct ButtonState{
    bool is_down; // Esta presionado ahora mismo?
    bool changed; // ¿Cambio de estado en este frame? (Para detectar 'One press')
};

struct GameInput {
    ButtonState up;
    ButtonState down;
    ButtonState left;
    ButtonState right;
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
void platform_update_window(GameInput* input); // Procesa mensajes (Teclado/Mouse)
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

    //Creamos la ventana visual. Envia un mensaje a WM_SIZE
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

// Función auxiliar interna para actualizar un botón
void win32_process_keyboard_message(ButtonState* new_state, bool is_down) {
    if (new_state->is_down != is_down) {
        new_state->is_down = is_down;
        new_state->changed = true;
    } else {
        new_state->changed = false;
    }
}

void platform_update_window(GameInput* input){
    MSG msg;

    while(PeekMessageA(&msg, window, 0, 0, PM_REMOVE)){
        switch (msg.message)
            {
            case WM_KEYUP:
            case WM_KEYDOWN: {
                // El bit 31 de lParam nos dice si la tecla ya estaba presionada antes (autorepeat)
                // y el bit 30 si estaba presionada antes del mensaje.
                // Para ser precisos
                bool is_down = (msg.message == WM_KEYDOWN);

                // wParam contiene el CÓDIGO de la tecla (VK_CODE)
                uint32_t vk_code = (uint32_t)msg.wParam;

                if (vk_code == VK_UP) {
                    win32_process_keyboard_message(&input->up, is_down);
                }
                else if (vk_code == VK_DOWN) {
                    win32_process_keyboard_message(&input->down, is_down);
                }
                else if (vk_code == VK_LEFT) {
                    win32_process_keyboard_message(&input->left, is_down);
                }
                else if (vk_code == VK_RIGHT) {
                    win32_process_keyboard_message(&input->right, is_down);
                }
            } break;
        
            default: {
                TranslateMessage(&msg);
                DispatchMessageA(&msg); // Calls the callback specified when creating  the window
            }   break;

        }
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
//                              Graphics Functions
// ##################################################################

void draw_rect(GameBuffer* buffer, int x, int y, int width, int height, uint32_t color){
    // 1. Calcular los limites (Bounding box)
    int min_x = x;
    int min_y = y;
    int max_x = x+width;
    int max_y = y+height;

    // 2. Clipping (Recorte de seguridad)
    // Si el rectángulo está totalmente fuera de la pantalla, no dibujamos nada
    if (min_x >= buffer->width) return;
    if (min_y >= buffer->height) return;
    if (max_x < 0) return;
    if (max_y < 0) return;

    // Si se sale un poco por los bordes, lo "empujamos" adentro
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > buffer->width) max_x = buffer->width;
    if (max_y > buffer->height) max_y = buffer->height;

    // 3. Dibujado
    // Calculamos dónde empieza la memoria para la primera fila a dibujar
    uint8_t* row = (uint8_t*)buffer->memory;

    // Avanzamos hasta la fila 'min_y' usando el Pitch
    row += min_y * buffer->pitch;

    // Avanzamos hasta la columna 'min_x' (4 bytes por pixel)
    row += min_x * 4;

    for (int current_y = min_y; current_y < max_y; ++current_y) {
        
        uint32_t* pixel = (uint32_t*)row;

        for (int current_x = min_x; current_x < max_x; ++current_x) {
            *pixel = color;
            pixel++;
        }

        // Al terminar la fila pequeña del rectángulo, saltamos a la siguiente línea de la pantalla
        // Importante: 'row' ya está apuntando al inicio (min_x) de esta fila,
        // así que solo sumamos el pitch para bajar recto.
        row += buffer->pitch;
    }
}
// Función para limpiar la pantalla (pintarla toda de un color)
void clear_screen(GameBuffer* buffer, uint32_t color){
    uint8_t* row = (uint8_t*)buffer->memory;
    for (int y = 0; y < buffer->height; ++y) {
        uint32_t* pixel = (uint32_t*)row;
        for (int x = 0; x < buffer->width; ++x) {
            *pixel = color;
            pixel++;
        }
        row += buffer->pitch;
    }
}
// ##################################################################
//                          Main (Game Loop)
// ##################################################################
// Estado del juego (Variables simples)

static int player_x = 100;
static int player_y = 100;


void game_update_and_render(GameBuffer* buffer, GameInput* input) {
    // 1. Limpiar pantalla (Fondo negro)
    clear_screen(buffer, 0xFF222222); // 0x00RRGGBB (Todo 0 es negro)

    // 2. Lógica del Juego (Input)
    int speed = 10;

    if (input->up.is_down)    player_y -= speed; // En computación, Y disminuye hacia arriba
    if (input->down.is_down)  player_y += speed;
    if (input->left.is_down)  player_x -= speed;
    if (input->right.is_down) player_x += speed;
    

    // 3. Dibujar Jugador (Cuadrado Verde)
    // Color: 0x0000FF00 (Verde puro)
    draw_rect(buffer, player_x, player_y, 50, 50, 0x0000FF00); 
    
}

int main() { 
    std::cout << "Inicializando strangerEngine..." << std::endl;

    int width = 1200;
    int height = 720;

    char* title = "strangerEngine v0.1";

    // 1. Inicialización Plataforma
    if (!platform_create_window(1280, 720, title)) return -1;

    std::cout << "Motor corriendo..." << std::endl;

    // Creamos la estructura de input vacía
    GameInput input = {};

    // 2. Bucle Principal
    while(running){
        
        // A. Input / Mensajes del Sistema
        platform_update_window(&input);

        // B. Game Logic & Render (Dibujar en RAM)
        // Nota: Aquí pasamos el buffer genérico, no sabemos nada de Windows
        if (global_back_buffer.memory) {
            game_update_and_render(&global_back_buffer, &input);
        }


        // C. Blit (Copiar RAM a Ventana)
        // Aquí le decimos a la plataforma: "Ya dibujé, muéstralo".
        #ifdef _WIN32
        platform_blit_to_window();
        #endif

        Sleep(16); // ~60 FPS chapuza
        
    }

    std::cout << "Apagando strangerEngine." << std::endl;
    return 0;
}