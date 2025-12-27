#include <iostream>
#include <stdint.h> 
#include <stdio.h> // Required for fopen, fseek, fread
#include <dsound.h> // Required for DirectSound
#include <math.h>   // Required for math functions like sin, cos

// Definición de PI por si acaso no está
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ##################################################################
//                      DirectSound Types
// ##################################################################
typedef HRESULT(WINAPI* direct_sound_create)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);

// ##################################################################
//                          Engine Types
// ##################################################################

// Represents the game's back buffer (framebuffer)
// Contains the pixel data and dimensions for drawing
struct GameBuffer {
    void* memory;       // Pointer to pixel data
    int width;          // Screen width in pixels
    int height;         // Screen height in pixels
    int pitch;          // Bytes per scanline (width * 4 for 32-bit)
};

// Tracks the state of a single input button/key
struct ButtonState {
    bool is_down;       // Whether the button is currently pressed
    bool changed;       // Whether the state changed this frame
};

// Contains the state of all input buttons
struct GameInput {
    ButtonState up;     // Up arrow or W key
    ButtonState down;   // Down arrow or S key
    ButtonState left;   // Left arrow or A key
    ButtonState right;  // Right arrow or D key
};

// Represents a loaded bitmap/image in memory
struct LoadedBitmap {
    int width;          // Image width in pixels
    int height;         // Image height in pixels
    uint32_t* pixels;   // Pointer to pixel color data (ARGB format)
};

// Structure for returning raw file data from disk
struct ReadResult {
    void* content;      // Pointer to the loaded file data
    size_t content_size; // Size of the loaded file in bytes
};

struct GameSoundOutput {
    int samples_per_second;     // Frecuencia de muestreo (48000 Hz)
    uint32_t running_sample_index; // El "tiempo" t acumulado (nunca se resetea)
    int bytes_per_sample;       // sizeof(int16) * 2 canales = 4 bytes
    int secondary_buffer_size;  // Tamaño total del buffer circular en bytes
    float t_sine;               // Fase de la onda senoidal (radianes)
    int latency_sample_count;   // Cuánto nos adelantamos al cursor de reproducción
};


// ##################################################################
//                          Platform Globals
// ##################################################################

// Game loop control flag
static bool running = true;

// Global back buffer used for all drawing operations
static GameBuffer global_back_buffer;

static GameSoundOutput global_sound_output;

// Player position in world coordinates
static float player_x = 100.0f;
static float player_y = 100.0f;

// ##################################################################
//                  Platform Functions Declarations
// ##################################################################

// Creates a window for the game
bool platform_create_window(int width, int height, const char* title); 

// Processes input events and updates the input structure
void platform_update_window(GameInput* input);

// Copies the back buffer to the window for display
void platform_blit_to_window(); 

// Reads an entire file from disk into memory
ReadResult debug_read_entire_file(const char* filename);

// Frees memory allocated for a file
void free_file_memory(void* memory);

// Loads a BMP image file from disk
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
#include <timeapi.h> // Required for timeBeginPeriod and timeEndPeriod

// Handle to the game window
static HWND window;

// Bitmap info used by Windows for rendering
static BITMAPINFO bitmap_info; 

// The loaded hero/player bitmap
static LoadedBitmap hero_bitmap;

// Puntero global al buffer donde escribiremos el audio
static LPDIRECTSOUNDBUFFER global_secondary_buffer;

void win32_init_dsound(HWND window, int32_t samples_per_second, int32_t buffer_size) {
    // 1. Cargar la librería dinámicamente
    HMODULE dsound_library = LoadLibraryA("dsound.dll");
    
    if (dsound_library) {
        // Obtenemos la dirección de la función creadora
        direct_sound_create DirectSoundCreatePtr = 
            (direct_sound_create)GetProcAddress(dsound_library, "DirectSoundCreate");

        LPDIRECTSOUND direct_sound;
        if (DirectSoundCreatePtr && SUCCEEDED(DirectSoundCreatePtr(0, &direct_sound, 0))) {
            
            // 2. Establecer nivel de cooperación
            // DSSCL_PRIORITY nos permite cambiar el formato de audio del buffer primario
            if (SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
                
                // 3. Configurar el BUFFER PRIMARIO (El mezclador de Windows)
                // No escribimos aquí, solo le decimos a Windows cómo mezclar.
                DSBUFFERDESC primary_buffer_desc = {};
                primary_buffer_desc.dwSize = sizeof(primary_buffer_desc);
                primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

                LPDIRECTSOUNDBUFFER primary_buffer;
                if (SUCCEEDED(direct_sound->CreateSoundBuffer(&primary_buffer_desc, &primary_buffer, 0))) {
                    
                    // Definimos el formato: PCM, Stereo, 48kHz, 16-bit
                    WAVEFORMATEX wave_format = {};
                    wave_format.wFormatTag = WAVE_FORMAT_PCM;
                    wave_format.nChannels = 2;
                    wave_format.nSamplesPerSec = samples_per_second;
                    wave_format.wBitsPerSample = 16;
                    wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
                    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
                    wave_format.cbSize = 0;

                    if (SUCCEEDED(primary_buffer->SetFormat(&wave_format))) {
                        // ¡Formato maestro establecido!
                        std::cout << "Audio Primario configurado a 48kHz Stereo." << std::endl;
                    }
                }
            }

            // 4. Crear el BUFFER SECUNDARIO (Nuestro Buffer Circular)
            DSBUFFERDESC secondary_buffer_desc = {};
            secondary_buffer_desc.dwSize = sizeof(secondary_buffer_desc);
            secondary_buffer_desc.dwFlags = 0; 
            secondary_buffer_desc.dwBufferBytes = buffer_size;
            
            // Le damos el mismo formato que al primario
            WAVEFORMATEX wave_format = {};
            wave_format.wFormatTag = WAVE_FORMAT_PCM;
            wave_format.nChannels = 2;
            wave_format.nSamplesPerSec = samples_per_second;
            wave_format.wBitsPerSample = 16;
            wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
            wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;

            secondary_buffer_desc.lpwfxFormat = &wave_format;

            if (SUCCEEDED(direct_sound->CreateSoundBuffer(&secondary_buffer_desc, &global_secondary_buffer, 0))) {
                std::cout << "Buffer Secundario de Audio Creado Exitosamente." << std::endl;
            }
        } else {
            std::cout << "No se pudo crear el objeto DirectSound." << std::endl;
        }
    } else {
        std::cout << "No se pudo cargar dsound.dll" << std::endl;
    }
}

void win32_fill_sound_buffer(GameSoundOutput* sound_output, DWORD byte_to_lock, DWORD bytes_to_write) {
    VOID* region1;
    DWORD region1_size;
    VOID* region2;
    DWORD region2_size;


    // Bloquear el buffer secundario para escribir audio
    if (SUCCEEDED(global_secondary_buffer->Lock(byte_to_lock, bytes_to_write,
        &region1, &region1_size,
        &region2, &region2_size,
        0))) {

        // Llenar región 1
        int16_t* sample_out = (int16_t*)region1;
        DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;

        for (DWORD i = 0; i < region1_sample_count; ++i) {
            float sine_value = sinf(sound_output->t_sine);
            int16_t sample_value = (int16_t)(sine_value * 3000); // 3000 es el volumen (Max 32000)

            // Escribir muestra en ambos canales (stereo)
            *sample_out++ = sample_value; // Canal izquierdo
            *sample_out++ = sample_value; // Canal derecho
            
            //Avanzamos el oscilador
            sound_output->t_sine += 2.0f * M_PI * 256.0f / (float)sound_output->samples_per_second;
            if(sound_output ->t_sine > 2.0f * M_PI) {
                sound_output->t_sine -= 2.0f * M_PI;
            }
            sound_output->running_sample_index++;
        }

        // Llenar región 2 si existe
        sample_out = (int16_t*)region2;
        DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;

        for (DWORD i = 0; i < region2_sample_count; ++i) {
            float sine_value = sinf(sound_output->t_sine);
            int16_t sample_value = (int16_t)(sine_value * 3000); // Volumen

            *sample_out++ = sample_value; // Canal izquierdo
            *sample_out++ = sample_value; // Canal derecho


            sound_output->t_sine += 2.0f * M_PI * 256.0f / (float)sound_output->samples_per_second;
            if(sound_output ->t_sine > 2.0f * M_PI) {
                sound_output->t_sine -= 2.0f * M_PI;
            }
            sound_output->running_sample_index++;

        }

        global_secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
    }
}

// Allocates and resizes the back buffer to the specified dimensions
void win32_resize_DIB_section(GameBuffer* buffer, int width, int height) {
    // Free existing memory if any
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }
    
    // Set buffer dimensions
    buffer->width = width;
    buffer->height = height;
    buffer->pitch = width * 4; // 4 bytes per pixel (ARGB)

    // Configure Windows bitmap info header
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height; // Negative for top-down bitmap
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    // Allocate memory for the pixel data
    int bitmap_memory_size = (width * height) * 4;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
}

// Windows message callback for handling window events
LRESULT CALLBACK windows_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        // User clicked close button
        case WM_CLOSE: 
            running = false; 
            break;
            
        // Window was resized
        case WM_SIZE: {
            RECT rect;
            GetClientRect(window, &rect);
            win32_resize_DIB_section(&global_back_buffer, rect.right - rect.left, rect.bottom - rect.top);
        } break;
        
        // Other messages handled by default Windows behavior
        default: 
            result = DefWindowProcA(window, msg, wParam, lParam);
    }
    return result;
}

// Creates the game window and initializes the back buffer
bool platform_create_window(int width, int height, const char* title) 
{
    // Get the application instance handle
    HINSTANCE instance = GetModuleHandleA(0);
    
    // Configure window class
    WNDCLASSA wc = {};
    wc.hInstance = instance;
    wc.hIcon = LoadIcon(instance, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); 
    wc.lpszClassName = title; 
    wc.lpfnWndProc = windows_window_callback; 

    // Register the window class
    if(!RegisterClassA(&wc)) return false; 
    
    // Initialize the back buffer
    win32_resize_DIB_section(&global_back_buffer, width, height);

    // Create the actual window
    window = CreateWindowExA(0, title, title, WS_OVERLAPPEDWINDOW,
        100, 100, width, height, NULL, NULL, instance, NULL);
    
    if(window == NULL) return false;
    
    // Show the window
    ShowWindow(window, SW_SHOW);
    return true;
}

// Processes a single keyboard button event
void win32_process_keyboard_message(ButtonState* new_state, bool is_down) {
    if (new_state->is_down != is_down) {
        // State changed this frame
        new_state->is_down = is_down;
        new_state->changed = true;
    } else {
        // State unchanged
        new_state->changed = false;
    }
}

// Processes all pending window messages (input events, etc.)
void platform_update_window(GameInput* input){
    MSG msg;
    while(PeekMessageA(&msg, window, 0, 0, PM_REMOVE)){
        switch(msg.message) {
            // Keyboard input
            case WM_KEYDOWN:
            case WM_KEYUP: {
                bool is_down = (msg.message == WM_KEYDOWN);
                uint32_t vk_code = (uint32_t)msg.wParam;

                // Map virtual key codes to input buttons
                if (vk_code == VK_UP)    win32_process_keyboard_message(&input->up, is_down);
                else if (vk_code == VK_DOWN)  win32_process_keyboard_message(&input->down, is_down);
                else if (vk_code == VK_LEFT)  win32_process_keyboard_message(&input->left, is_down);
                else if (vk_code == VK_RIGHT) win32_process_keyboard_message(&input->right, is_down);
            } break;

            // Other messages (translate and dispatch)
            default:
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
                break;
        }
    }
}

// Copies the back buffer to the screen for display
void platform_blit_to_window() {
    HDC device_context = GetDC(window);
    RECT r; 
    GetClientRect(window, &r);
    
    // Use StretchDIBits to copy the back buffer to the window
    StretchDIBits(device_context, 0, 0, r.right - r.left, r.bottom - r.top,
        0, 0, global_back_buffer.width, global_back_buffer.height,
        global_back_buffer.memory, &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
    
    ReleaseDC(window, device_context);
}

// ##################################################################
//          File Loading Implementation (Windows version)    
// ##################################################################

// Frees memory allocated by debug_read_entire_file
void free_file_memory(void* memory) {
    if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

// Reads an entire file from disk into dynamically allocated memory
ReadResult debug_read_entire_file(const char* filename) {
    ReadResult result = {};

    // Open file in binary read mode
    FILE* file = fopen(filename, "rb");

    if(file){
        // Seek to end of file to determine size
        fseek(file, 0, SEEK_END);
        result.content_size = ftell(file); // Get file size in bytes
        fseek(file, 0, SEEK_SET); // Seek back to beginning

        // Allocate memory for file contents
        result.content = VirtualAlloc(0, result.content_size, MEM_COMMIT, PAGE_READWRITE);

        // Read entire file into memory
        fread(result.content, result.content_size, 1, file);
        fclose(file); // Close the file
    } else {
        std::cout << "Error opening file: " << filename << std::endl;
    }
    
    return result;
}

// Loads a BMP image file and returns it as a LoadedBitmap
LoadedBitmap debug_load_bmp(const char* filename) {
    LoadedBitmap result = {};
    ReadResult file = debug_read_entire_file(filename);

    if (file.content && file.content_size > 0) {
        // Parse BMP file headers
        BITMAPFILEHEADER* file_header = (BITMAPFILEHEADER*)file.content;
        BITMAPINFOHEADER* info_header = (BITMAPINFOHEADER*)((uint8_t*)file.content + sizeof(BITMAPFILEHEADER));

        // --- SAFETY VALIDATION ---
        // Only 32-bit BMPs are supported (with alpha channel)
        if (info_header->biBitCount != 32) {
            std::cout << "ERROR: BMP file is not 32-bit (" << info_header->biBitCount << " bits detected)." << std::endl;
            std::cout << "Please save the image as a '32-bit bitmap' (with Alpha channel)." << std::endl;
            // Free file memory since we won't use it
            free_file_memory(file.content);
            return result; // Return empty (will trigger fallback in main)
        }
        // -------------------------

        // Extract dimensions
        result.width = info_header->biWidth;
        result.height = info_header->biHeight;
        
        // Point to pixel data (starts after BMP headers)
        result.pixels = (uint32_t*)((uint8_t*)file.content + file_header->bfOffBits);
    }
    
    return result;
}

#endif // _WIN32


// ##################################################################
//                          Main (Game Logic)
// ##################################################################

// Creates a procedural test bitmap (checkerboard pattern)
// Useful as a fallback when image files fail to load
LoadedBitmap make_test_bitmap(int width, int height) {
    LoadedBitmap bmp = {};
    bmp.width = width;
    bmp.height = height;
    
    // Allocate memory for pixel data (4 bytes per pixel for ARGB)
    int size = width * height * 4;
    bmp.pixels = (uint32_t*)VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);

    // Fill pixels with checkerboard pattern
    uint32_t* pixel_ptr = bmp.pixels;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Checkerboard: divide by 8 and check if sum is even/odd
            bool black = ((x / 8) + (y / 8)) % 2 == 0;
            
            if (black) {
                *pixel_ptr = 0xFF000000; // Black
            } else {
                *pixel_ptr = 0xFFFF00FF; // Bright magenta
            }
            pixel_ptr++;
        }
    }
    return bmp;
}

// Draws a solid filled rectangle to the back buffer
// Parameters: buffer (target), x/y (position), width/height (size), color (ARGB)
void draw_rect(GameBuffer* buffer, int x, int y, int width, int height, uint32_t color) {
    int min_x = x;
    int min_y = y;
    int max_x = x + width;
    int max_y = y + height;

    // Clipping: ensure rectangle stays within screen bounds
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > buffer->width) max_x = buffer->width;
    if (max_y > buffer->height) max_y = buffer->height;

    // Pointer to the start of the rectangle in the back buffer
    uint8_t* row = (uint8_t*)buffer->memory;
    row += min_y * buffer->pitch + min_x * 4;

    // Fill each scanline of the rectangle
    for (int cy = min_y; cy < max_y; ++cy) {
        uint32_t* pixel = (uint32_t*)row;
        for (int cx = min_x; cx < max_x; ++cx) {
            *pixel = color;
            pixel++;
        }
        row += buffer->pitch;
    }
}

// Detects axis-aligned bounding box collision between two rectangles
// Returns true if the boxes overlap, false otherwise
bool check_collision(float x1, float y1, int w1, int h1, float x2, float y2, int w2, int h2){
    // Check if there is overlap on both X and Y axes
    bool collision_x = (x1 < x2 + w2) && (x1 + w1 > x2);
    bool collision_y = (y1 < y2 + h2) && (y1 + h1 > y2);

    // Collision occurs only if overlapping on both axes
    return collision_x && collision_y;
}

// Draws a bitmap/sprite to the back buffer at the specified position
// Supports clipping at screen edges
void draw_bitmap(GameBuffer* buffer, LoadedBitmap* bitmap, int x, int y){
    int min_x = x;
    int min_y = y;
    int max_x = x + bitmap->width;
    int max_y = y + bitmap->height;

    // Clipping calculation
    // When we clip the draw area, we also need to know where to start
    // reading from the source texture
    int source_offset_x = 0;
    int source_offset_y = 0;

    if (min_x < 0) {
        source_offset_x = -min_x; // Start further into the texture
        min_x = 0;
    }
    if (min_y < 0) {
        source_offset_y = -min_y;
        min_y = 0;
    }
    if (max_x > buffer->width) max_x = buffer->width;
    if (max_y > buffer->height) max_y = buffer->height;

    // Calculate initial pointers
    // Destination (screen):
    uint8_t* dest_row = (uint8_t*)buffer->memory + (min_y * buffer->pitch) + (min_x * 4);

    // Source (texture):
    // The texture is linear and compact (pitch = width * 4)
    uint32_t* source_row = bitmap->pixels + (source_offset_y * bitmap->width) + source_offset_x;
    
    // Copy each scanline
    for (int cy = min_y; cy < max_y; ++cy) {
        uint32_t* dest_pixel = (uint32_t*)dest_row;
        uint32_t* source_pixel = source_row;

        // Copy each pixel in the scanline
        for (int cx = min_x; cx < max_x; ++cx) {
            // Simple copy (no transparency blending yet)
            // Read from texture -> Write to screen
            *dest_pixel = *source_pixel;

            dest_pixel++;
            source_pixel++;
        }
        
        // Advance to next scanline
        dest_row += buffer->pitch;
        source_row += bitmap->width; // Texture pitch is simply the width
    }
}

void draw_bitmap_alpha(GameBuffer* buffer, LoadedBitmap* bitmap, float x, float y){
    int min_x = (int)x;
    int min_y = (int)y;
    int max_x = min_x + bitmap->width;
    int max_y = min_y + bitmap->height;


    // Clipping calculation
    int source_offset_x = 0;
    int source_offset_y = 0;

    if(min_x < 0 ) { source_offset_x = -min_x; min_x = 0; }
    if(min_y < 0 ) { source_offset_y = -min_y; min_y = 0; }
    if(max_x > buffer->width )  max_x = buffer->width;
    if(max_y > buffer->height ) max_y = buffer->height;


    uint8_t* dest_row = (uint8_t*)buffer->memory + (min_y * buffer->pitch) + (min_x * 4);
    uint32_t* source_row = bitmap->pixels + (source_offset_y * bitmap->width) + source_offset_x;


    for(int cy = min_y; cy < max_y; ++cy) {
        uint32_t* dest_pixel = (uint32_t*)dest_row;
        uint32_t* source_pixel = source_row;

        for(int cx = min_x; cx < max_x; ++cx) {
            uint32_t src_color = *source_pixel;

            // Extract alpha component
            uint8_t alpha = (src_color >> 24) & 0xFF;

            if (alpha == 0){

            }
            else if (alpha == 255){
                *dest_pixel = src_color;
            }
            else {
                float a = (float)alpha / 255.0f;
                uint8_t src_r = (src_color >> 16) & 0xFF;
                uint8_t src_g = (src_color >> 8) & 0xFF;
                uint8_t src_b = src_color & 0xFF;

                uint32_t dst_color = *dest_pixel;
                uint8_t dst_r = (dst_color >> 16) & 0xFF;
                uint8_t dst_g = (dst_color >> 8) & 0xFF;
                uint8_t dst_b = dst_color & 0xFF;

                uint8_t final_r = (uint8_t)((src_r * a) + (dst_r * (1.0f - a)));
                uint8_t final_g = (uint8_t)((src_g * a) + (dst_g * (1.0f - a)));
                uint8_t final_b = (uint8_t)((src_b * a) + (dst_b * (1.0f - a)));

                *dest_pixel = (0xFF << 24) | (final_r << 16) | (final_g << 8) | final_b;
            }
            dest_pixel++;
            source_pixel++;
        }
        dest_row += buffer->pitch;
        source_row += bitmap->width;
    }
}
// Main game update and rendering function
// Called once per frame with delta time since last frame
void game_update_and_render(GameBuffer* buffer, GameInput* input, float dt) {

    // 1. Clear screen
    draw_rect(buffer, 0, 0, buffer->width, buffer->height, 0xFF333333);

    // 2. Define obstacle (wall)
    float wall_x = 400.0f;
    float wall_y = 300.0f;
    int wall_w = 100;
    int wall_h = 200;

    // 3. Player movement (tentative position)
    // Store the "potential" next position
    float next_x = player_x;
    float next_y = player_y;
    float speed = 500.0f; // Pixels per second


    if (input->left.is_down)  next_x -= speed * dt;
    if (input->right.is_down) next_x += speed * dt;
    if (input->up.is_down)    next_y -= speed * dt;
    if (input->down.is_down)  next_y += speed * dt;

    // 4. COLLISION DETECTION
    // Check if the new position would collide with the wall
    bool hit = check_collision(next_x, next_y, 50, 50, wall_x, wall_y, wall_w, wall_h);

    // Player color (changes on collision)
    uint32_t player_color = 0xFF00FF00; // Green (normal)

    if (hit) {
        player_color = 0xFFFF0000; // Red (collision!)
        // OPTION A: Allow passing through wall but change color (trigger)
        player_x = next_x;
        player_y = next_y;
    } else {
        // No collision, move normally
        player_x = next_x;
        player_y = next_y;
    }

    // 5. Draw obstacle (light gray)
    draw_rect(buffer, (int)wall_x, (int)wall_y, wall_w, wall_h, 0xFF888888);

    // 6. Draw player sprite
    draw_bitmap_alpha(buffer, &hero_bitmap, player_x, player_y);
}

// Main entry point of the application
int main() { 
    std::cout << "Initializing strangerEngine..." << std::endl;
    const char* title = "strangerEngine v0.5 - High Precision Loop";

    // Request high precision from Windows scheduler (1ms resolution)
    timeBeginPeriod(1);

    // Create the game window
    if (!platform_create_window(1280, 720, title)) return -1;

    // Initialize input structure
    GameInput input = {};

    // Setup high-resolution timer for frame timing
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    long long perf_count_frequency = perf_count_frequency_result.QuadPart;

    // Target 60 FPS
    float target_seconds_per_frame = 1.0f / 60.0f;

    // Record start time
    LARGE_INTEGER last_counter;
    QueryPerformanceCounter(&last_counter);


    // --- LOAD GAME ASSETS ---
    hero_bitmap = debug_load_bmp("C:\\Users\\thesu\\Desktop\\BizzottoProjects\\StrangerEngine\\test_hero.bmp");
    
    if(hero_bitmap.pixels == NULL) {
        // 1. Show warning dialog (blocks until user clicks OK)
        MessageBoxA(
            NULL, 
            "Could not load test_hero.bmp.\nUsing procedural texture instead.", 
            "Asset Warning", 
            MB_OK | MB_ICONWARNING
        );

        // 2. FALLBACK: Generate a procedural texture to keep the game running
        std::cout << "Using procedural texture..." << std::endl;
        hero_bitmap = make_test_bitmap(32, 32);
    }

    // --- SONIDO: Inicialización ---
    GameSoundOutput sound_output = {};
    sound_output.samples_per_second = 48000;
    sound_output.bytes_per_sample = sizeof(int16_t) * 2; // 4 bytes
    sound_output.latency_sample_count = sound_output.samples_per_second / 15; // 1/15 segundos de latencia
    // Buffer de 1 segundo
    sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample; 
    
    // Inicializamos DirectSound
    win32_init_dsound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
    
    // Llenado inicial (Pre-roll): Llenamos todo el buffer de silencio/sonido antes de arrancar
    // para evitar que suene "basura" al principio.
    win32_fill_sound_buffer(&sound_output, 0, sound_output.latency_sample_count * sound_output.bytes_per_sample);
    
    // START ENGINE: Le damos Play en modo LOOPING
    if (global_secondary_buffer) {
        global_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);
    }

    // --- MAIN GAME LOOP ---
    while(running){

        // --- AUDIO: Actualización por Frame ---
        DWORD play_cursor;
        DWORD write_cursor;
        if (global_secondary_buffer && SUCCEEDED(global_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor))) {
            
            // Calculamos el byte exacto donde NOSOTROS nos quedamos escribiendo la última vez.
            // Usamos modulo (%) porque es un buffer circular.
            DWORD byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % sound_output.secondary_buffer_size;
            
            // Calculamos hasta dónde queremos escribir (Target Cursor)
            // Queremos mantener la latencia constante (ej. siempre 60ms adelante del Play Cursor real)
            DWORD target_cursor = (play_cursor + (sound_output.latency_sample_count * sound_output.bytes_per_sample)) % sound_output.secondary_buffer_size;
            
            DWORD bytes_to_write;

            // Lógica circular: ¿El target está adelante o dio la vuelta?
            if (byte_to_lock > target_cursor) {
                // Caso A: Dio la vuelta (Wrap around)
                // Escribimos hasta el final + lo que falta del principio
                bytes_to_write = (sound_output.secondary_buffer_size - byte_to_lock) + target_cursor;
            } else {
                // Caso B: Lineal
                bytes_to_write = target_cursor - byte_to_lock;
            }
            
            // Rellenamos el hueco
            win32_fill_sound_buffer(&sound_output, byte_to_lock, bytes_to_write);
        }

        LARGE_INTEGER work_counter_begin; 
        QueryPerformanceCounter(&work_counter_begin);

        // Process input events
        platform_update_window(&input);
        
        // Calculate delta time since last frame
        long long counter_elapsed = work_counter_begin.QuadPart - last_counter.QuadPart;
        float dt = (float)counter_elapsed / (float)perf_count_frequency;
        
        // Clamp delta time to prevent huge jumps
        if (dt > 0.1f) dt = 0.1f;

        // Update last frame time
        last_counter = work_counter_begin;


        // Update and render game state
        if (global_back_buffer.memory) {
            game_update_and_render(&global_back_buffer, &input, dt);
        }

        // Display back buffer on screen
        #ifdef _WIN32
        platform_blit_to_window();
        #endif

        // --- HYBRID FPS LIMITER (Secret to smooth rendering) ---
        LARGE_INTEGER work_counter_end;
        QueryPerformanceCounter(&work_counter_end);
        
        // Calculate how long this frame took to render
        long long work_elapsed = work_counter_end.QuadPart - work_counter_begin.QuadPart;
        float seconds_elapsed_for_work = (float)work_elapsed / (float)perf_count_frequency;
        float seconds_elapsed_total = seconds_elapsed_for_work;

        // Busy-wait until we reach target frame time
        while (seconds_elapsed_total < target_seconds_per_frame) {
            // If there's significant time left, sleep to avoid wasting CPU
            if (target_seconds_per_frame - seconds_elapsed_total > 0.002f) { // More than 2ms remaining
                Sleep(1); // Sleep ~1ms
            } else {
                // Less than 2ms left: busy-wait (tight loop) for maximum precision
            }

            // Check elapsed time again
            QueryPerformanceCounter(&work_counter_end);
            long long total_elapsed = work_counter_end.QuadPart - work_counter_begin.QuadPart;
            seconds_elapsed_total = (float)total_elapsed / (float)perf_count_frequency;
        }
    } 

    // Cleanup
    timeEndPeriod(1); // Restore Windows scheduler to normal resolution
    std::cout << "Shutting down strangerEngine." << std::endl;
    return 0;
}