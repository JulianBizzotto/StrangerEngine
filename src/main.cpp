#include <iostream>
#include <stdint.h> 

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
#endif // _WIN32

// ##################################################################
//                          Main (Game Logic)
// ##################################################################

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
    draw_rect(buffer, (int)player_x, (int)player_y, 50, 50, player_color); 
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