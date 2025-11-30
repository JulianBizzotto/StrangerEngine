#include <iostream>
#include <windows.h>

// Prototipo del procedimiento de ventana (el cerebro que maneja los eventos)
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int main() {
    std::cout << "Inicializando strangerEngine..." << std::endl;

    // 1. Obtener el manejador de la instancia actual (Necesario porque usamos main estándar)
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // 2. Definir la clase de ventana
    const char* CLASS_NAME = "StrangerEngineWindowClass";
    
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;   // Puntero a la función que maneja los mensajes
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); // Cursor normal

    // 3. Registrar la clase
    if (!RegisterClass(&wc)) {
        std::cout << "Fallo al registrar la ventana." << std::endl;
        return -1;
    }

    // 4. Crear la ventana
    HWND hwnd = CreateWindowEx(
        0,                              // Estilos opcionales
        CLASS_NAME,                     // Nombre de la clase
        "Stranger Engine v0.1",         // Título de la barra
        WS_OVERLAPPEDWINDOW,            // Ventana con bordes y botones estándar
        CW_USEDEFAULT, CW_USEDEFAULT,   // Posición X, Y
        1280, 720,                      // Resolución inicial
        NULL,                           // Ventana padre
        NULL,                           // Menú
        hInstance,                      // Instancia
        NULL                            // Puntero adicional
    );

    if (hwnd == NULL) {
        std::cout << "Fallo al crear la ventana." << std::endl;
        return -1;
    }

    // 5. Mostrar la ventana
    ShowWindow(hwnd, SW_SHOW);

    std::cout << "Motor corriendo. Cierra la ventana para terminar." << std::endl;

    // 6. El Bucle Principal (Game Loop)
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    std::cout << "Apagando strangerEngine." << std::endl;
    return 0;
}

// Implementación del manejo de eventos de Windows
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        // Mensaje cuando aprietas la X de la ventana
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        // Mensaje cuando la ventana ya se destruyó -> Salimos del loop
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Rellenar la pantalla de un color solido (Gris oscuro)
            HBRUSH brush = CreateSolidBrush(RGB(40, 40, 40));
            FillRect(hdc, &ps.rcPaint, brush);
            DeleteObject(brush); // Importante: borrar objetos GDI para no fugar memoria

            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}