#include <iostream>
#include <stdint.h> 
#include <stdio.h> // <--- NUEVO: Necesario para fopen, fseek, fread
// ##################################################################
//                          Engine Types
// ##################################################################
struct GameBuffer {
    void* memory;       
    int width;
    int height;
    int pitch;          
};

struct ButtonState {
    bool is_down;
    bool changed;
};

struct GameInput {
    ButtonState up;
    ButtonState down;
    ButtonState left;
    ButtonState right;
};

struct LoadedBitmap {
    int width;
    int height;
    uint32_t* pixels; // Puntero a la memoria donde están los colores de la imagen
};

// Estructura para devolver el archivo crudo
struct ReadResult {
    void* content; // Puntero a los datos leídos
    size_t content_size; // Tamaño de los datos leídos
};



// ##################################################################
//                          Platform Globals
// ##################################################################
static bool running = true;
static GameBuffer global_back_buffer;

static float player_x = 100.0f;
static float player_y = 100.0f;

// ##################################################################
//                          Platform Functions Declarations
// ##################################################################
bool platform_create_window(int width, int height, const char* title); 
void platform_update_window(GameInput* input);
void platform_blit_to_window(); 
ReadResult debug_read_entire_file(const char* filename);
void free_file_memory(void* memory);
LoadedBitmap debug_load_bmp(const char* filename);
// ##################################################################
//                          Windows Platform
// ##################################################################
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h> // <--- NUEVO: Necesario para timeBeginPeriod

static HWND window;
static BITMAPINFO bitmap_info; 
static LoadedBitmap hero_bitmap;

void win32_resize_DIB_section(GameBuffer* buffer, int width, int height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }
    buffer->width = width;
    buffer->height = height;
    buffer->pitch = width * 4; 

    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height; 
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    int bitmap_memory_size = (width * height) * 4;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
}

LRESULT CALLBACK windows_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        case WM_CLOSE: running = false; break;
        case WM_SIZE: {
            RECT rect;
            GetClientRect(window, &rect);
            win32_resize_DIB_section(&global_back_buffer, rect.right - rect.left, rect.bottom - rect.top);
        } break;
        default: result = DefWindowProcA(window, msg, wParam, lParam);
    }
    return result;
}

bool platform_create_window(int width, int height, const char* title) 
{
    HINSTANCE instance = GetModuleHandleA(0);
    WNDCLASSA wc = {};
    wc.hInstance = instance;
    wc.hIcon = LoadIcon(instance, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); 
    wc.lpszClassName = title; 
    wc.lpfnWndProc = windows_window_callback; 

    if(!RegisterClassA(&wc)) return false; 
    
    win32_resize_DIB_section(&global_back_buffer, width, height);

    window = CreateWindowExA(0, title, title, WS_OVERLAPPEDWINDOW,
        100, 100, width, height, NULL, NULL, instance, NULL);
    
    if(window == NULL) return false;
    ShowWindow(window, SW_SHOW);
    return true;
}

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
        switch(msg.message) {
            case WM_KEYDOWN:
            case WM_KEYUP: {
                bool is_down = (msg.message == WM_KEYDOWN);
                uint32_t vk_code = (uint32_t)msg.wParam;

                if (vk_code == VK_UP)    win32_process_keyboard_message(&input->up, is_down);
                else if (vk_code == VK_DOWN)  win32_process_keyboard_message(&input->down, is_down);
                else if (vk_code == VK_LEFT)  win32_process_keyboard_message(&input->left, is_down);
                else if (vk_code == VK_RIGHT) win32_process_keyboard_message(&input->right, is_down);
            } break;

            default:
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
                break;
        }
    }
}

void platform_blit_to_window() {
    HDC device_context = GetDC(window);
    RECT r; GetClientRect(window, &r);
    StretchDIBits(device_context, 0, 0, r.right - r.left, r.bottom - r.top,
        0, 0, global_back_buffer.width, global_back_buffer.height,
        global_back_buffer.memory, &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(window, device_context);
}

// Funcion para liberar la memoria del archivo despues
void free_file_memory(void* memory) {
    if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

// ##################################################################
//          Implementacion de carga de archivos (version windows)    
// ##################################################################

//Lee todo el archivo de un tiron
ReadResult debug_read_entire_file(const char* filename) {
    ReadResult result = {};


    // Abrimos el archivo (CreateFile es la forma Win32, pero fopen es más estándar C)
    // Usaremos fopen para que sea más legible, asegúrate de incluir <stdio.h>
    FILE* file = fopen(filename, "rb");

    if(file){
        // Buscamos el final para saber el tamaño
        fseek(file, 0, SEEK_END);
        result.content_size = ftell(file); // Obtenemos el tamaño del archivo
        fseek(file, 0, SEEK_SET); // Volvemos al inicio del archivo

        // Pedimos memoria
        result.content = VirtualAlloc(0, result.content_size, MEM_COMMIT, PAGE_READWRITE);

        //Leemos todo
        fread(result.content, result.content_size, 1, file);
        fclose(file); // Cerramos el archivo
    } else {
        std::cout << "Error al abrir el archivo: " << filename << std::endl;
    }
    return result;
}
LoadedBitmap debug_load_bmp(const char* filename) {
    LoadedBitmap result = {};
    ReadResult file = debug_read_entire_file(filename);

    if (file.content && file.content_size > 0) {
        BITMAPFILEHEADER* file_header = (BITMAPFILEHEADER*)file.content;
        BITMAPINFOHEADER* info_header = (BITMAPINFOHEADER*)((uint8_t*)file.content + sizeof(BITMAPFILEHEADER));

        // --- VALIDACIÓN DE SEGURIDAD ---
        // Si la imagen no es de 32 bits, no podemos leerla con nuestro código actual.
        // Evitamos el crash devolviendo una imagen vacía.
        if (info_header->biBitCount != 32) {
            std::cout << "ERROR: El archivo BMP no es de 32 bits (" << info_header->biBitCount << " bits detectados)." << std::endl;
            std::cout << "Por favor guarda la imagen como 'Bitmap de 32 bits' (con canal Alpha)." << std::endl;
            // Liberamos la memoria del archivo porque no la vamos a usar
            free_file_memory(file.content);
            return result; // Retornamos vacío (esto activará el fallback en el main)
        }
        // -------------------------------

        result.width = info_header->biWidth;
        result.height = info_header->biHeight;
        result.pixels = (uint32_t*)((uint8_t*)file.content + file_header->bfOffBits);
    }
    return result;
}
#endif // _WIN32

// ##################################################################
//                          Main (Game Logic)
// ##################################################################

// Crea una textura de ajedrez en memoria
LoadedBitmap make_test_bitmap(int width, int height) {
    LoadedBitmap bmp = {};
    bmp.width = width;
    bmp.height = height;
    
    // Reservamos memoria para esta imagen (4 bytes por pixel)
    // Usamos VirtualAlloc igual que con la pantalla
    int size = width * height * 4;
    bmp.pixels = (uint32_t*)VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);

    // Rellenamos los píxeles
    uint32_t* pixel_ptr = bmp.pixels;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Truco matemático para hacer un tablero de ajedrez
            // Si la suma de las coordenadas / 8 es par o impar...
            bool black = ((x / 8) + (y / 8)) % 2 == 0;
            
            if (black) {
                *pixel_ptr = 0xFF000000; // Negro
            } else {
                *pixel_ptr = 0xFFFF00FF; // Magenta brillante
            }
            pixel_ptr++;
        }
    }
    return bmp;
}

void draw_rect(GameBuffer* buffer, int x, int y, int width, int height, uint32_t color) {
    int min_x = x;
    int min_y = y;
    int max_x = x + width;
    int max_y = y + height;

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > buffer->width) max_x = buffer->width;
    if (max_y > buffer->height) max_y = buffer->height;

    uint8_t* row = (uint8_t*)buffer->memory;
    row += min_y * buffer->pitch + min_x * 4;

    for (int cy = min_y; cy < max_y; ++cy) {
        uint32_t* pixel = (uint32_t*)row;
        for (int cx = min_x; cx < max_x; ++cx) {
            *pixel = color;
            pixel++;
        }
        row += buffer->pitch;
    }
}
bool check_collision(float x1, float y1, int w1, int h1, float x2, float y2, int w2, int h2){
   
    //1.Calcular los bordes
    bool collision_x = (x1<x2+w2) && (x1+w1>x2);
    bool collision_y = (y1<y2+h2) && (y1+h1>y2);

    // Si se solapan en X y también en Y, hay colisión real
    return collision_x && collision_y;

}

void draw_bitmap(GameBuffer* buffer, LoadedBitmap* bitmap, int x, int y){
    int min_x = x;
    int min_y = y;
    int max_x = x+ bitmap->width;
    int max_y = y + bitmap->height;

    // Clipping (Protección de bordes)
    // IMPORTANTE: Si recortamos el dibujo en la pantalla, también tenemos
    // que saber desde qué pixel de la textura empezamos a copiar.
    int source_offset_x = 0;
    int source_offset_y = 0;

    if (min_x < 0) {
        source_offset_x = -min_x; // Empezamos más adentro en la textura
        min_x = 0;
    }
    if (min_y < 0) {
        source_offset_y = -min_y;
        min_y = 0;
    }
    if (max_x > buffer->width) max_x = buffer->width;
    if (max_y > buffer->height) max_y = buffer->height;

    // Punteros iniciales
    // Pantalla:
    uint8_t* dest_row = (uint8_t*)buffer->memory + (min_y * buffer->pitch) + (min_x * 4);

    // Textura (Fuente):
    // La textura es lineal y compacta (pitch = width * 4)
    uint32_t* source_row = bitmap->pixels + (source_offset_y * bitmap->width) + source_offset_x;
    for (int cy = min_y; cy < max_y; ++cy) {
        
        uint32_t* dest_pixel = (uint32_t*)dest_row;
        uint32_t* source_pixel = source_row;

        for (int cx = min_x; cx < max_x; ++cx) {
            // COPIADO SIMPLE (Sin transparencia aun)
            // Leemos de la textura -> Escribimos en pantalla
            *dest_pixel = *source_pixel;

            dest_pixel++;
            source_pixel++;
        }
        
        // Avanzar a la siguiente fila
        dest_row += buffer->pitch;
        source_row += bitmap->width; // En la textura el pitch es simplemente el ancho
    }
}



void game_update_and_render(GameBuffer* buffer, GameInput* input, float dt) {

    // 1. Limpiar pantalla
    draw_rect(buffer, 0, 0, buffer->width, buffer->height, 0xFF333333);

    // 2. Definir Obstáculo (Pared)
    float wall_x = 400.0f;
    float wall_y = 300.0f;
    int wall_w = 100;
    int wall_h = 200;

    // 3. Movimiento del Jugador (Tentativo)
    // Guardamos la posición "futura"
    float next_x = player_x;
    float next_y = player_y;
    float speed = 500.0f;


    if (input->left.is_down)  next_x -= speed * dt;
    if (input->right.is_down) next_x += speed * dt;
    if (input->up.is_down)    next_y -= speed * dt;
    if (input->down.is_down)  next_y += speed * dt;

    // 4. DETECCIÓN DE COLISIÓN
    // Verificamos si la posición futura toca la pared
    bool hit = check_collision(next_x, next_y, 50, 50, wall_x, wall_y, wall_w, wall_h);

    // Color del jugador
    uint32_t player_color = 0xFF00FF00; // Verde (Normal)

    if (hit) {
        player_color = 0xFFFF0000; // Rojo (Choque!)
        // OPCIÓN A: Atravesar la pared pero cambiar color (Trigger)
        player_x = next_x;
        player_y = next_y;
    } else {
        // Si no choca, nos movemos normalmente
        player_x = next_x;
        player_y = next_y;
    }

    // 5. Dibujar Obstáculo (Gris claro)
    draw_rect(buffer, (int)wall_x, (int)wall_y, wall_w, wall_h, 0xFF888888);

    // 6. Dibujar Jugador
    draw_bitmap(buffer, &hero_bitmap, (int)player_x, (int)player_y);

}

int main() { 
    std::cout << "Inicializando strangerEngine..." << std::endl;
    const char* title = "strangerEngine v0.5 - High Precision Loop";

    // NUEVO: Le pedimos al Scheduler de Windows que sea preciso (1ms)
    timeBeginPeriod(1);

    if (!platform_create_window(1280, 720, title)) return -1;

    GameInput input = {};

    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    long long perf_count_frequency = perf_count_frequency_result.QuadPart;

    // 60 FPS
    float target_seconds_per_frame = 1.0f / 60.0f;

    LARGE_INTEGER last_counter;
    QueryPerformanceCounter(&last_counter);


    // --- CARGAR ASSETS ---
    hero_bitmap = debug_load_bmp("C:\\Users\\thesu\\Desktop\\BizzottoProjects\\StrangerEngine\\test_hero.bmp");
    
    if(hero_bitmap.pixels == NULL) {
        // 1. Mostrar Alerta Visual (Esto detiene el programa hasta que le das OK)
        MessageBoxA(
            NULL, 
            "No se pudo cargar test_hero.bmp.\nSe usara una textura por defecto.", 
            "Advertencia de Assets", 
            MB_OK | MB_ICONWARNING
        );

        // 2. PLAN B (Fallback): Usar textura generada para no cerrar el juego
        std::cout << "Usando textura procedural..." << std::endl;
        hero_bitmap = make_test_bitmap(32, 32);
    }

    while(running){
        LARGE_INTEGER work_counter_begin; 
        QueryPerformanceCounter(&work_counter_begin);

        platform_update_window(&input);
        
        long long counter_elapsed = work_counter_begin.QuadPart - last_counter.QuadPart;
        float dt = (float)counter_elapsed / (float)perf_count_frequency;
        
        if (dt > 0.1f) dt = 0.1f;

        last_counter = work_counter_begin;

        if (global_back_buffer.memory) {
            game_update_and_render(&global_back_buffer, &input, dt);
        }

        #ifdef _WIN32
        platform_blit_to_window();
        #endif

        // --- LIMITADOR DE FPS HÍBRIDO (El secreto de la suavidad) ---
        LARGE_INTEGER work_counter_end;
        QueryPerformanceCounter(&work_counter_end);
        
        long long work_elapsed = work_counter_end.QuadPart - work_counter_begin.QuadPart;
        float seconds_elapsed_for_work = (float)work_elapsed / (float)perf_count_frequency;
        float seconds_elapsed_total = seconds_elapsed_for_work;


        while (seconds_elapsed_total < target_seconds_per_frame) {
            // Si nos falta mucho tiempo, dormimos un poco para no quemar CPU
            if (target_seconds_per_frame - seconds_elapsed_total > 0.002f) { // Si falta mas de 2ms
                Sleep(1); // Duerme 1ms aprox
            } else {
                // Si falta muy poco (menos de 2ms), hacemos "Busy Wait" (Loop infinito)
                // para clavar el tiempo exacto.
            }

            // Medimos de nuevo
            QueryPerformanceCounter(&work_counter_end);
            long long total_elapsed = work_counter_end.QuadPart - work_counter_begin.QuadPart;
            seconds_elapsed_total = (float)total_elapsed / (float)perf_count_frequency;
        }
    } 

    // Limpieza
    timeEndPeriod(1); // Devolvemos el scheduler a la normalidad
    std::cout << "Apagando strangerEngine." << std::endl;
    return 0;
}