#include <iostream>
#include <stdint.h> // Para usar tipos de tamaño fijo (uint8_t, uint32_t)

// ##################################################################
//                          Platform Globals
// ##################################################################
static bool running = true;



// ##################################################################
//                          Platform Functions
// ##################################################################

bool platform_create_window(int width, int height, char* title); 
void platform_update_window();
// ##################################################################
//                          Windows Platform
// ##################################################################
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// ##################################################################
//                          Windows Globals
// ##################################################################
static HWND window;

// ##################################################################
//                         Platform Implementations 
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
        
        default:
        {
            // Let windows handle the default input for now
            result = DefWindowProcA(window, msg, wParam, lParam);
        }
    }

    return result;
}

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

#endif


int main() { 
    std::cout << "Inicializando strangerEngine..." << std::endl;

    int width = 1200;
    int height = 720;

    char* title = "strangerEngine v0.1";

    platform_create_window(width, height, title);

    std::cout << "Motor corriendo. Cierra la ventana para terminar." << std::endl;

    while(running){
        platform_update_window();
    }

    std::cout << "Apagando strangerEngine." << std::endl;
    return 0;
}