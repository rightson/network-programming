#include <windows.h>
#include <list>
#include <map>
using namespace std;

#include "resource.h"

#include "http-server.h"

#define SERVER_PORT 7799

#define WM_SOCKET_NOTIFICATION_HTTP_SERVER (WM_USER + 1)
#define WM_SOCKET_NOTIFICATION_CGI_SERVICE (WM_USER + 2)


BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
int passiveSocket(HWND &hwndEdit, HWND &hwnd, SOCKET &msock, struct sockaddr_in &sa);
void HttpServerEventLoop(LPARAM &lParam, WPARAM &wParam, HttpServer *httpServer);
void CGIServiceEventLoop(LPARAM &lParam, WPARAM &wParam, HttpServer *httpServer);
//=================================================================
//  Global Variables
//=================================================================
HWND hwndEdit; // for cross file access

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    WSADATA wsaData;

    static SOCKET msock, ssock;
    static struct sockaddr_in sa;
    static HttpServer *httpServer = new HttpServer(hwnd, WM_SOCKET_NOTIFICATION_CGI_SERVICE);

    switch (Message)
    {
        case WM_INITDIALOG:
            hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
            break;
        case WM_COMMAND:
            switch(LOWORD(wParam)) {
                case ID_LISTEN:
                    WSAStartup(MAKEWORD(2, 0), &wsaData);
                    if (passiveSocket(hwndEdit, hwnd, msock, sa) == FALSE) {
                        return FALSE;
                    }
                    break;
                case ID_EXIT:
                    EndDialog(hwnd, 0);
                    break;
            };
            break;

        case WM_CLOSE:
            EndDialog(hwnd, 0);
            break;

        case WM_SOCKET_NOTIFICATION_HTTP_SERVER:
            HttpServerEventLoop(lParam, wParam, httpServer);
            break;

        case WM_SOCKET_NOTIFICATION_CGI_SERVICE:
            CGIServiceEventLoop(lParam, wParam, httpServer);
            break;

        default:
            return FALSE;
    };
    return TRUE;
}

void HttpServerEventLoop(LPARAM &lParam, WPARAM &wParam, HttpServer *httpServer) {
    SOCKET ssock;
    switch (WSAGETSELECTEVENT(lParam)) {
        case FD_ACCEPT: {
            SOCKET msock = wParam;
            ssock = accept(msock, NULL, NULL);
            EditPrintf(hwndEdit, TEXT("=== [HTTP] Accept one new client(%d), client#:%d ===\r\n"), ssock, httpServer->clientNumbers());
            httpServer->acceptConnection(msock, ssock);
            break;
        }
        case FD_READ: {
            ssock = wParam;
            EditPrintf(hwndEdit, TEXT("=== [HTTP] Read from client(%d), client#:%d ===\r\n"), ssock, httpServer->clientNumbers());
            if (httpServer->readRequest(ssock) <= 0) {
                shutdown(ssock, 2);
            }
            break;
        }
        case FD_WRITE: {
            ssock = wParam;
            EditPrintf(hwndEdit, TEXT("=== [HTTP] Write to client(%d), client#:%d ===\r\n"), ssock, httpServer->clientNumbers());
            if (httpServer->writeResponse(ssock) <= 0) {
                shutdown(ssock, 2);
            }
            break;
        }
        case FD_CLOSE: {
            ssock = wParam;
            EditPrintf(hwndEdit, TEXT("=== [HTTP] Close client(%d), client#:%d ===\r\n"), ssock, httpServer->clientNumbers());
            httpServer->cleanupConnection(ssock);
            break;
        }
    };
}

void CGIServiceEventLoop(LPARAM &lParam, WPARAM &wParam, HttpServer *httpServer) {
    SOCKET connfd = wParam;
    switch (WSAGETSELECTEVENT(lParam)) {
        case FD_CONNECT:
            EditPrintf(hwndEdit, TEXT("=== [CGI] Connect event %d ===\r\n"), connfd);
            httpServer->cgiServerHandler(connfd, FD_CONNECT);
            break;
        case FD_READ: {
            EditPrintf(hwndEdit, TEXT("=== [CGI] Read event %d ===\r\n"), connfd);
            httpServer->cgiServerHandler(connfd, FD_READ);
            break;
        }
        case FD_WRITE: {
            EditPrintf(hwndEdit, TEXT("=== [CGI] Write event %d ===\r\n"), connfd);
            httpServer->cgiServerHandler(connfd, FD_WRITE);
            break;
        }
        case FD_CLOSE: {
            EditPrintf(hwndEdit, TEXT("=== [CGI] Close event %d ===\r\n"), connfd);
            httpServer->cgiServerHandler(connfd, FD_CLOSE);
            break;
        }
    };
}

int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
     TCHAR   szBuffer [10000] ;
     va_list pArgList ;

     va_start (pArgList, szFormat) ;
     wvsprintf (szBuffer, szFormat, pArgList) ;
     va_end (pArgList) ;

     SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1) ;
     SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer) ;
     SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0) ;
     return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0);
}

int passiveSocket(HWND &hwndEdit, HWND &hwnd, SOCKET &msock, struct sockaddr_in &sa) {
    msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (msock == INVALID_SOCKET) {
        EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
        WSACleanup();
        return FALSE;
    }

    int err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFICATION_HTTP_SERVER, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

    if (err == SOCKET_ERROR) {
        EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
        closesocket(msock);
        WSACleanup();
        return FALSE;
    }

    //fill the address info about server
    sa.sin_family = AF_INET;
    sa.sin_port = htons(SERVER_PORT);
    sa.sin_addr.s_addr = INADDR_ANY;

    //bind socket
    err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

    if (err == SOCKET_ERROR) {
        EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
        WSACleanup();
        return FALSE;
    }

    err = listen(msock, 2);

    if (err == SOCKET_ERROR) {
        EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
        WSACleanup();
        return FALSE;
    }
    else {
        EditPrintf(hwndEdit, TEXT("=== Server START listening port %d ===\r\n"), SERVER_PORT);
    }
    return TRUE;
}
