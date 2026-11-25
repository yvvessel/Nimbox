#include "gui.h"
#include "storage.h"
#include "hook.h"
#include "injector.h"
#include "theme.h"
#include "settings.h"
#include "autostart.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <objidl.h>
#include <gdiplus.h>
#include <ctype.h>
#include <wctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

// Identificadores de controles da janela principal
#define ID_SEARCH_BOX        1001
#define ID_SNIPPET_TABLE     1002
#define ID_BTN_NEW           1003
#define ID_BTN_EDIT          1004
#define ID_BTN_DELETE        1005
#define ID_BTN_TOGGLE_HOOK   1007
#define ID_BTN_THEME         1008

// Notificacoes customizadas da tabela de snippets
#define WM_APP_TABLE_DBLCLK  (WM_APP + 110)
#define WM_APP_TABLE_DELETE  (WM_APP + 111)

// Identificadores dos campos do modal de criacao/edicao
#define ID_MODAL_NAME        2001
#define ID_MODAL_TRIGGER     2002
#define ID_MODAL_CONTENT     2003
#define ID_MODAL_SAVE        2004
#define ID_MODAL_CANCEL      2005

// Identificadores do menu da bandeja do sistema (System Tray)
#define ID_TRAY_OPEN         3001
#define ID_TRAY_TOGGLE_HOOK  3002
#define ID_TRAY_THEME        3003
#define ID_TRAY_EXIT         3004
#define ID_TRAY_AUTOSTART    3005

// Identificadores dos recursos embutidos (res/app.rc)
#define IDI_APP_ICON         101
#define IDR_LOGO_PNG         102

// Constantes de layout da janela principal
#define UI_MARGIN            24
#define UI_HEADER_Y          14
#define UI_LOGO_SIZE         60
#define UI_TOOLBAR_Y         88
#define UI_TOOLBAR_H         36
#define UI_TABLE_Y           138
#define UI_FOOTER_H          62

// Constantes da tabela customizada de snippets
#define TABLE_HEADER_H       38
#define TABLE_ROW_H          46
#define TABLE_RADIUS         10.0f
#define TABLE_SCROLL_BAR_W   12

// Tipos de icones desenhados vetorialmente nos botoes
typedef enum {
    BTN_ICON_NONE = 0,
    BTN_ICON_PLUS,
    BTN_ICON_SUN,
    BTN_ICON_MOON
} ButtonIconType;

// Linha armazenada na tabela visual de snippets
typedef struct {
    int id;
    wchar_t name[MAX_NAME_LEN];
    wchar_t trigger[MAX_TRIGGER_LEN];
    wchar_t preview[160];
} SnippetTableRow;

typedef struct {
    SnippetTableRow *rows;
    int count;
    int capacity;
    int selected_index;
    int selected_id;
    int hover_index;
    int scroll_y;
    bool hover_scrollbar;
    bool dragging_scrollbar;
    int drag_start_y;
    int drag_start_scroll;
} SnippetTableState;

// Variaveis de estado da interface
static HINSTANCE s_hInstance = NULL;
static HWND s_hwndMain = NULL;
static HWND s_hwndSearch = NULL;
static HWND s_hwndTable = NULL;
static HWND s_hwndBtnNew = NULL;
static HWND s_hwndBtnEdit = NULL;
static HWND s_hwndBtnDelete = NULL;
static HWND s_hwndToggleHook = NULL;
static HWND s_hwndThemeBtn = NULL;

static HFONT s_hFont = NULL;
static HFONT s_hFontBold = NULL;
static HFONT s_hFontTitle = NULL;
static HFONT s_hFontSmall = NULL;
static HFONT s_hFontMono = NULL;
static HICON s_hAppIcon = NULL;

static ULONG_PTR s_gdipToken = 0;
static GpBitmap *s_pLogoBitmap = NULL;

static SnippetList *s_snippets = NULL;
static AppSettings *s_settings = NULL;
static SnippetTableState s_table = {0};
static int s_visible_snippet_count = 0;
static bool s_search_has_text = false;

// Dados para a bandeja do sistema (Unicode)
static NOTIFYICONDATAW s_nid = {0};
static bool s_tray_created = false;

// Dados em transito durante a edicao/criacao no modal
static Snippet *s_current_editing = NULL;
static HWND s_hwndModal = NULL;
static HWND s_hwndModalName = NULL;
static HWND s_hwndModalTrigger = NULL;
static HWND s_hwndModalContent = NULL;

// -------------------------------------------------------------
// Conversores de Codificacao (UTF-8 <-> UTF-16 Unicode)
// -------------------------------------------------------------

static void utf8_to_wchar_buf(const char *utf8_str, wchar_t *out_buf, int max_chars) {
    if (!out_buf || max_chars <= 0) return;
    out_buf[0] = L'\0';
    if (!utf8_str) return;
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, out_buf, max_chars);
}

static void wchar_to_utf8_buf(const wchar_t *wstr, char *out_buf, int max_bytes) {
    if (!out_buf || max_bytes <= 0) return;
    out_buf[0] = '\0';
    if (!wstr) return;
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out_buf, max_bytes, NULL, NULL);
}

// -------------------------------------------------------------
// Auxiliares GDI+ e Decoracoes de Janela
// -------------------------------------------------------------

static ARGB colorref_to_argb(COLORREF c, BYTE alpha) {
    return ((ARGB)alpha << 24) | ((ARGB)GetRValue(c) << 16) | ((ARGB)GetGValue(c) << 8) | (ARGB)GetBValue(c);
}

static void gdip_add_round_rect(GpPath *path, REAL x, REAL y, REAL w, REAL h, REAL r) {
    if (!path || w <= 0.0f || h <= 0.0f) return;
    REAL max_r = (w < h ? w : h) * 0.5f;
    if (r > max_r) r = max_r;
    if (r <= 0.0f) r = 0.5f;
    REAL d = r * 2.0f;

    GdipAddPathArc(path, x, y, d, d, 180.0f, 90.0f);
    GdipAddPathArc(path, x + w - d, y, d, d, 270.0f, 90.0f);
    GdipAddPathArc(path, x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    GdipAddPathArc(path, x, y + h - d, d, d, 90.0f, 90.0f);
    GdipClosePathFigure(path);
}

static void gdip_fill_round_rect(GpGraphics *gfx, REAL x, REAL y, REAL w, REAL h, REAL r, ARGB fillArgb) {
    GpPath *path = NULL;
    GpSolidFill *brush = NULL;
    if (GdipCreatePath(FillModeAlternate, &path) == Ok) {
        gdip_add_round_rect(path, x, y, w, h, r);
        if (GdipCreateSolidFill(fillArgb, &brush) == Ok) {
            GdipFillPath(gfx, (GpBrush *)brush, path);
            GdipDeleteBrush((GpBrush *)brush);
        }
        GdipDeletePath(path);
    }
}

static void gdip_stroke_round_rect(GpGraphics *gfx, REAL x, REAL y, REAL w, REAL h, REAL r, ARGB strokeArgb, REAL penWidth) {
    GpPath *path = NULL;
    GpPen *pen = NULL;
    if (GdipCreatePath(FillModeAlternate, &path) == Ok) {
        gdip_add_round_rect(path, x, y, w, h, r);
        if (GdipCreatePen1(strokeArgb, penWidth, UnitPixel, &pen) == Ok) {
            GdipDrawPath(gfx, pen, path);
            GdipDeletePen(pen);
        }
        GdipDeletePath(path);
    }
}

static void gdip_fill_and_stroke_round_rect(GpGraphics *gfx, REAL x, REAL y, REAL w, REAL h, REAL r, ARGB fillArgb, ARGB strokeArgb, REAL penWidth) {
    GpPath *path = NULL;
    if (GdipCreatePath(FillModeAlternate, &path) == Ok) {
        gdip_add_round_rect(path, x, y, w, h, r);

        GpSolidFill *brush = NULL;
        if (GdipCreateSolidFill(fillArgb, &brush) == Ok) {
            GdipFillPath(gfx, (GpBrush *)brush, path);
            GdipDeleteBrush((GpBrush *)brush);
        }

        if ((strokeArgb >> 24) > 0 && penWidth > 0.0f) {
            GpPen *pen = NULL;
            if (GdipCreatePen1(strokeArgb, penWidth, UnitPixel, &pen) == Ok) {
                GdipDrawPath(gfx, pen, path);
                GdipDeletePen(pen);
            }
        }
        GdipDeletePath(path);
    }
}

// Mascara suavemente os 4 cantos externos de um retangulo arredondado com a cor de fundo da janela
static void gdip_mask_outside_round_rect(GpGraphics *gfx, int w, int h, REAL x, REAL y, REAL rw, REAL rh, REAL r, ARGB outsideArgb) {
    GpPath *path = NULL;
    if (GdipCreatePath(FillModeAlternate, &path) == Ok) {
        GdipAddPathRectangle(path, -1.0f, -1.0f, (REAL)w + 2.0f, (REAL)h + 2.0f);
        gdip_add_round_rect(path, x, y, rw, rh, r);

        GpSolidFill *brush = NULL;
        if (GdipCreateSolidFill(outsideArgb, &brush) == Ok) {
            GdipFillPath(gfx, (GpBrush *)brush, path);
            GdipDeleteBrush((GpBrush *)brush);
        }
        GdipDeletePath(path);
    }
}

// Carrega a logo PNG otimizada (240x240) a partir do recurso RCDATA (ou disco como fallback)
static void load_logo_bitmap(HINSTANCE hInst) {
    if (s_pLogoBitmap) return;

    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(IDR_LOGO_PNG), MAKEINTRESOURCEW(10));
    if (hRes) {
        DWORD resSize = SizeofResource(hInst, hRes);
        HGLOBAL hResData = LoadResource(hInst, hRes);
        if (hResData && resSize > 0) {
            const void *pData = LockResource(hResData);
            if (pData) {
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, resSize);
                if (hMem) {
                    void *pMem = GlobalLock(hMem);
                    if (pMem) {
                        memcpy(pMem, pData, resSize);
                        GlobalUnlock(hMem);

                        IStream *pStream = NULL;
                        if (SUCCEEDED(CreateStreamOnHGlobal(hMem, TRUE, &pStream)) && pStream) {
                            GdipCreateBitmapFromStream(pStream, &s_pLogoBitmap);
                            pStream->lpVtbl->Release(pStream);
                            hMem = NULL; // Liberado pelo IStream
                        }
                    }
                    if (hMem) GlobalFree(hMem);
                }
            }
        }
    }

    if (!s_pLogoBitmap) {
        GdipCreateBitmapFromFile(L"assets\\branding\\NimboxLogo_240.png", &s_pLogoBitmap);
    }
}

// Aplica o tema na barra de titulo da janela nativa e cantos arredondados (Windows 10/11)
static void apply_window_decorations(HWND hwnd, BOOL is_dark) {
    if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_dark, sizeof(is_dark)))) {
        DwmSetWindowAttribute(hwnd, 19, &is_dark, sizeof(is_dark));
    }
    DWORD corner_preference = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_preference, sizeof(corner_preference));
}

// -------------------------------------------------------------
// Botoes Modernos (Double-Buffered GDI+ sem cantos brancos)
// -------------------------------------------------------------

static LRESULT CALLBACK ModernButtonSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)dwRefData;
    switch (msg) {
        case WM_ERASEBKGND:
            return 1; // Evita que o fundo retangular branco padrao seja pintado nos cantos

        case WM_MOUSEMOVE: {
            if (!GetPropW(hwnd, L"HOVER")) {
                SetPropW(hwnd, L"HOVER", (HANDLE)1);
                TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE: {
            RemovePropW(hwnd, L"HOVER");
            RemovePropW(hwnd, L"PRESSED");
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            SetPropW(hwnd, L"PRESSED", (HANDLE)1);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED: {
            if (GetPropW(hwnd, L"PRESSED")) {
                RemovePropW(hwnd, L"PRESSED");
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }
        case WM_SETFOCUS: {
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        case WM_KILLFOCUS: {
            RemovePropW(hwnd, L"PRESSED");
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        case WM_ENABLE: {
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        case WM_NCDESTROY: {
            RemovePropW(hwnd, L"HOVER");
            RemovePropW(hwnd, L"PRESSED");
            RemoveWindowSubclass(hwnd, ModernButtonSubclass, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void draw_button_vector_icon(GpGraphics *gfx, ButtonIconType icon, REAL cx, REAL cy, ARGB color) {
    if (icon == BTN_ICON_NONE) return;

    GpPen *pen = NULL;
    if (GdipCreatePen1(color, 1.5f, UnitPixel, &pen) != Ok) return;

    if (icon == BTN_ICON_PLUS) {
        GdipDrawLine(gfx, pen, cx - 4.5f, cy, cx + 4.5f, cy);
        GdipDrawLine(gfx, pen, cx, cy - 4.5f, cx, cy + 4.5f);
    } else if (icon == BTN_ICON_SUN) {
        GdipDrawEllipse(gfx, pen, cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        // 8 raios curtos e limpos
        GdipDrawLine(gfx, pen, cx, cy - 6.5f, cx, cy - 5.0f);
        GdipDrawLine(gfx, pen, cx, cy + 5.0f, cx, cy + 6.5f);
        GdipDrawLine(gfx, pen, cx - 6.5f, cy, cx - 5.0f, cy);
        GdipDrawLine(gfx, pen, cx + 5.0f, cy, cx + 6.5f, cy);
        GdipDrawLine(gfx, pen, cx - 4.6f, cy - 4.6f, cx - 3.6f, cy - 3.6f);
        GdipDrawLine(gfx, pen, cx + 3.6f, cy + 3.6f, cx + 4.6f, cy + 4.6f);
        GdipDrawLine(gfx, pen, cx - 4.6f, cy + 4.6f, cx - 3.6f, cy + 3.6f);
        GdipDrawLine(gfx, pen, cx + 3.6f, cy - 3.6f, cx + 4.6f, cy - 4.6f);
    } else if (icon == BTN_ICON_MOON) {
        // Lua crescente fechada por dois arcos unidos em caminho unico
        GpPath *moonPath = NULL;
        if (GdipCreatePath(FillModeAlternate, &moonPath) == Ok) {
            GdipAddPathArc(moonPath, cx - 5.5f, cy - 5.5f, 11.0f, 11.0f, 25.0f, 248.0f);
            GdipAddPathArc(moonPath, cx - 1.6f, cy - 6.4f, 9.2f, 9.2f, -126.3f, -169.4f);
            GdipClosePathFigure(moonPath);
            GdipDrawPath(gfx, pen, moonPath);
            GdipDeletePath(moonPath);
        }
    }

    GdipDeletePen(pen);
}

static void draw_modern_button(
    LPDRAWITEMSTRUCT dis,
    const wchar_t *text,
    ButtonIconType icon,
    COLORREF parentBg,
    COLORREF bgNormal,
    COLORREF bgHover,
    COLORREF bgPressed,
    COLORREF borderColor,
    COLORREF borderHover,
    COLORREF textColor,
    HFONT font
) {
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    bool isPressed = ((dis->itemState & ODS_SELECTED) != 0) || (GetPropW(dis->hwndItem, L"PRESSED") != NULL);
    bool isDisabled = (dis->itemState & ODS_DISABLED) != 0;
    bool isHovered = (GetPropW(dis->hwndItem, L"HOVER") != NULL);
    bool hasFocus = ((dis->itemState & ODS_FOCUS) != 0) && ((dis->itemState & ODS_NOFOCUSRECT) == 0);

    COLORREF bg = bgNormal;
    COLORREF border = borderColor;
    const ThemePalette *p = theme_get_current();

    if (isDisabled) {
        bg = p->bg_surface;
        textColor = p->text_muted;
        border = p->border_normal;
    } else if (isPressed) {
        bg = bgPressed;
        border = borderHover;
    } else if (isHovered) {
        bg = bgHover;
        border = borderHover;
    }

    // Offscreen double-buffering para eliminar cintilacao e cantos retangulares residuais
    HDC memDC = CreateCompatibleDC(dis->hDC);
    HBITMAP hbm = CreateCompatibleBitmap(dis->hDC, w, h);
    HBITMAP oldBm = (HBITMAP)SelectObject(memDC, hbm);

    // 1. Preenche TODO o retangulo com a cor real do fundo da janela pai
    RECT fullRc = {0, 0, w, h};
    HBRUSH hParentBr = CreateSolidBrush(parentBg);
    FillRect(memDC, &fullRc, hParentBr);
    DeleteObject(hParentBr);

    // 2. Desenha o botao arredondado com antialiasing via GDI+
    GpGraphics *gfx = NULL;
    if (GdipCreateFromHDC(memDC, &gfx) == Ok) {
        GdipSetSmoothingMode(gfx, SmoothingModeAntiAlias);
        GdipSetPixelOffsetMode(gfx, PixelOffsetModeHalf);

        gdip_fill_and_stroke_round_rect(
            gfx,
            0.5f, 0.5f, (REAL)w - 1.0f, (REAL)h - 1.0f, 8.0f,
            colorref_to_argb(bg, 255),
            colorref_to_argb(hasFocus && !isDisabled ? p->border_focus : border, 255),
            hasFocus && !isDisabled ? 1.5f : 1.0f
        );

        // Mede o texto para centralizar conjunto [icone + texto]
        HFONT oldFontMeasure = (HFONT)SelectObject(memDC, font);
        SIZE textSize = {0, 0};
        GetTextExtentPoint32W(memDC, text, (int)wcslen(text), &textSize);
        SelectObject(memDC, oldFontMeasure);

        int iconSpace = (icon != BTN_ICON_NONE) ? 18 : 0;
        int totalW = iconSpace + textSize.cx;
        int startX = (w - totalW) / 2;

        if (icon != BTN_ICON_NONE) {
            REAL iconCx = (REAL)startX + 6.0f;
            REAL iconCy = (REAL)h * 0.5f;
            draw_button_vector_icon(gfx, icon, iconCx, iconCy, colorref_to_argb(textColor, 255));
        }

        GdipDeleteGraphics(gfx);

        // 3. Desenha texto com ClearType nativo perfeitamente alinhado
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, textColor);
        HFONT oldFont = (HFONT)SelectObject(memDC, font);
        RECT textRc = {startX + iconSpace, 0, startX + iconSpace + textSize.cx + 2, h};
        DrawTextW(memDC, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(memDC, oldFont);
    }

    // 4. Copia o buffer final pronto de uma so vez para o DC do controle
    BitBlt(dis->hDC, rc.left, rc.top, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBm);
    DeleteObject(hbm);
    DeleteDC(memDC);
}

// -------------------------------------------------------------
// Tabela Customizada de Snippets (Win32 + GDI+ Moderna)
// -------------------------------------------------------------

static void table_free_rows(void) {
    if (s_table.rows) {
        free(s_table.rows);
        s_table.rows = NULL;
    }
    s_table.count = 0;
    s_table.capacity = 0;
}

static void table_clear_rows(void) {
    s_table.count = 0;
    s_table.hover_index = -1;
}

static void table_add_row(int id, const wchar_t *name, const wchar_t *trigger, const wchar_t *preview) {
    if (s_table.count >= s_table.capacity) {
        int new_cap = (s_table.capacity == 0) ? 16 : s_table.capacity * 2;
        SnippetTableRow *new_rows = (SnippetTableRow *)realloc(s_table.rows, (size_t)new_cap * sizeof(SnippetTableRow));
        if (!new_rows) return;
        s_table.rows = new_rows;
        s_table.capacity = new_cap;
    }

    SnippetTableRow *r = &s_table.rows[s_table.count];
    memset(r, 0, sizeof(SnippetTableRow));
    r->id = id;
    wcsncpy(r->name, name, MAX_NAME_LEN - 1);
    wcsncpy(r->trigger, trigger, MAX_TRIGGER_LEN - 1);
    wcsncpy(r->preview, preview, 159);
    s_table.count++;
}

static int table_get_body_height(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int body_h = (rc.bottom - rc.top) - TABLE_HEADER_H - 1;
    return (body_h > 0) ? body_h : 0;
}

static int table_get_max_scroll(HWND hwnd) {
    int body_h = table_get_body_height(hwnd);
    int total_h = s_table.count * TABLE_ROW_H;
    int max_scroll = total_h - body_h;
    return (max_scroll > 0) ? max_scroll : 0;
}

static void table_clamp_scroll(HWND hwnd) {
    int max_scroll = table_get_max_scroll(hwnd);
    if (s_table.scroll_y < 0) s_table.scroll_y = 0;
    if (s_table.scroll_y > max_scroll) s_table.scroll_y = max_scroll;
}

static void table_ensure_visible(HWND hwnd, int index) {
    if (index < 0 || index >= s_table.count) return;
    int body_h = table_get_body_height(hwnd);
    if (body_h <= 0) return;

    int row_top = index * TABLE_ROW_H;
    int row_bottom = row_top + TABLE_ROW_H;

    if (row_top < s_table.scroll_y) {
        s_table.scroll_y = row_top;
    } else if (row_bottom > s_table.scroll_y + body_h) {
        s_table.scroll_y = row_bottom - body_h;
    }
    table_clamp_scroll(hwnd);
}

static void table_select_index(HWND hwnd, int index, bool ensure_visible) {
    if (index < 0 || index >= s_table.count) {
        s_table.selected_index = -1;
        s_table.selected_id = -1;
    } else {
        s_table.selected_index = index;
        s_table.selected_id = s_table.rows[index].id;
        if (ensure_visible) {
            table_ensure_visible(hwnd, index);
        }
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void table_get_scrollbar_rect(HWND hwnd, RECT *outTrack, RECT *outThumb) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int body_h = table_get_body_height(hwnd);
    int total_h = s_table.count * TABLE_ROW_H;

    if (total_h <= body_h || body_h <= 16) {
        SetRectEmpty(outTrack);
        SetRectEmpty(outThumb);
        return;
    }

    int track_top = TABLE_HEADER_H + 6;
    int track_bottom = rc.bottom - 7;
    int track_h = track_bottom - track_top;
    if (track_h < 20) track_h = 20;

    outTrack->left = w - TABLE_SCROLL_BAR_W - 2;
    outTrack->top = track_top;
    outTrack->right = w - 2;
    outTrack->bottom = track_bottom;

    int thumb_h = (body_h * track_h) / total_h;
    if (thumb_h < 28) thumb_h = 28;
    if (thumb_h > track_h) thumb_h = track_h;

    int max_scroll = total_h - body_h;
    int travel = track_h - thumb_h;
    int thumb_top = track_top + ((max_scroll > 0 && travel > 0) ? (s_table.scroll_y * travel) / max_scroll : 0);

    outThumb->left = outTrack->left + 3;
    outThumb->top = thumb_top;
    outThumb->right = outTrack->right - 3;
    outThumb->bottom = thumb_top + thumb_h;
}

static int table_hit_test_row(HWND hwnd, int y) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    if (y < TABLE_HEADER_H || y >= rc.bottom - 1) return -1;

    int rel_y = (y - TABLE_HEADER_H) + s_table.scroll_y;
    if (rel_y < 0) return -1;

    int idx = rel_y / TABLE_ROW_H;
    if (idx >= 0 && idx < s_table.count) return idx;
    return -1;
}

static void table_paint(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    const ThemePalette *p = theme_get_current();

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBm = (HBITMAP)SelectObject(memDC, hbm);

    // 1. Preenche o fundo interno da tabela (os cantos externos serao mascarados com antialiasing ao final)
    HBRUSH hTableBr = CreateSolidBrush(p->table_bg);
    FillRect(memDC, &rc, hTableBr);
    DeleteObject(hTableBr);

    // Calcula larguras responsivas das colunas
    RECT trackRc, thumbRc;
    table_get_scrollbar_rect(hwnd, &trackRc, &thumbRc);
    bool has_scroll = !IsRectEmpty(&thumbRc);
    int right_pad = has_scroll ? (TABLE_SCROLL_BAR_W + 6) : 14;
    int usable_w = w - 16 - right_pad;

    int col0_x = 16;
    int col0_w = (usable_w * 28) / 100;
    if (col0_w < 140) col0_w = 140;

    int col1_x = col0_x + col0_w + 14;
    int col1_w = (usable_w * 20) / 100;
    if (col1_w < 110) col1_w = 110;

    int col2_x = col1_x + col1_w + 14;
    int col2_w = (w - right_pad) - col2_x;
    if (col2_w < 80) col2_w = 80;

    // 3. Cabecalho discreto e moderno
    RECT headerRc = {1, 1, w - 1, TABLE_HEADER_H};
    HBRUSH hHdrBr = CreateSolidBrush(p->table_header_bg);
    FillRect(memDC, &headerRc, hHdrBr);
    DeleteObject(hHdrBr);

    RECT hdrSepRc = {1, TABLE_HEADER_H - 1, w - 1, TABLE_HEADER_H};
    HBRUSH hSepBr = CreateSolidBrush(p->table_separator);
    FillRect(memDC, &hdrSepRc, hSepBr);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, p->text_secondary);
    HFONT oldFont = (HFONT)SelectObject(memDC, s_hFontBold);

    RECT h0 = {col0_x, 1, col0_x + col0_w, TABLE_HEADER_H - 1};
    RECT h1 = {col1_x, 1, col1_x + col1_w, TABLE_HEADER_H - 1};
    RECT h2 = {col2_x, 1, col2_x + col2_w, TABLE_HEADER_H - 1};
    DrawTextW(memDC, L"Nome", -1, &h0, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    DrawTextW(memDC, L"Gatilho", -1, &h1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    DrawTextW(memDC, L"Conteúdo", -1, &h2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // 4. Corpo da tabela (linhas ou estado vazio)
    HRGN hBodyRgn = CreateRectRgn(1, TABLE_HEADER_H, w - 1, h - 1);
    SelectClipRgn(memDC, hBodyRgn);
    DeleteObject(hBodyRgn);

    if (s_table.count == 0) {
        RECT emptyRc = {24, TABLE_HEADER_H, w - 24, h - 1};
        SelectObject(memDC, s_hFont);
        SetTextColor(memDC, p->text_muted);
        const wchar_t *emptyMsg = s_search_has_text
            ? L"Nenhum snippet encontrado para esta pesquisa."
            : L"Nenhum snippet cadastrado. Clique em \"+ Novo Snippet\" para começar.";
        DrawTextW(memDC, emptyMsg, -1, &emptyRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    } else {
        int body_h = h - TABLE_HEADER_H - 1;
        int first_row = s_table.scroll_y / TABLE_ROW_H;
        int last_row = (s_table.scroll_y + body_h) / TABLE_ROW_H;
        if (first_row < 0) first_row = 0;
        if (last_row >= s_table.count) last_row = s_table.count - 1;

        GpGraphics *rowGfx = NULL;
        GdipCreateFromHDC(memDC, &rowGfx);
        if (rowGfx) {
            GdipSetSmoothingMode(rowGfx, SmoothingModeAntiAlias);
            GdipSetPixelOffsetMode(rowGfx, PixelOffsetModeHalf);
        }

        for (int i = first_row; i <= last_row; i++) {
            SnippetTableRow *row = &s_table.rows[i];
            int row_y = TABLE_HEADER_H + (i * TABLE_ROW_H) - s_table.scroll_y;
            RECT rowRc = {1, row_y, w - 1, row_y + TABLE_ROW_H};

            bool isSelected = (i == s_table.selected_index);
            bool isHovered = (!isSelected && i == s_table.hover_index);

            COLORREF rowBg = (i % 2 == 1) ? p->table_row_alt : p->table_row;
            if (isSelected) {
                rowBg = p->table_row_selected;
            } else if (isHovered) {
                rowBg = p->table_row_hover;
            }

            HBRUSH hRowBr = CreateSolidBrush(rowBg);
            FillRect(memDC, &rowRc, hRowBr);
            DeleteObject(hRowBr);

            // Indicador lateral sutil para linha selecionada
            if (isSelected && rowGfx) {
                gdip_fill_round_rect(
                    rowGfx,
                    4.0f, (REAL)(row_y + 8), 3.0f, (REAL)(TABLE_ROW_H - 16), 1.5f,
                    colorref_to_argb(p->table_row_selected_border, 255)
                );
            }

            // Separador horizontal muito sutil (suprimido junto a linha selecionada para destaque limpo)
            bool nextSelected = (i + 1 == s_table.selected_index);
            if (!isSelected && !nextSelected && (i < s_table.count - 1 || row_y + TABLE_ROW_H < h - 2)) {
                RECT lineRc = {14, row_y + TABLE_ROW_H - 1, w - (has_scroll ? 18 : 14), row_y + TABLE_ROW_H};
                FillRect(memDC, &lineRc, hSepBr);
            }

            // Coluna 0: Nome
            SelectObject(memDC, s_hFontBold);
            SetTextColor(memDC, p->text_primary);
            RECT c0 = {col0_x, row_y, col0_x + col0_w, row_y + TABLE_ROW_H - 1};
            DrawTextW(memDC, row->name, -1, &c0, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            // Coluna 1: Gatilho dentro de badge discreta
            SelectObject(memDC, s_hFontMono);
            SIZE trigSz = {0, 0};
            GetTextExtentPoint32W(memDC, row->trigger, (int)wcslen(row->trigger), &trigSz);
            int pill_w = trigSz.cx + 18;
            if (pill_w > col1_w - 4) pill_w = col1_w - 4;
            if (pill_w < 32) pill_w = 32;
            int pill_h = 24;
            int pill_y = row_y + (TABLE_ROW_H - pill_h) / 2;

            if (rowGfx) {
                COLORREF pillBg = isSelected ? p->badge_selected_bg : p->badge_bg;
                COLORREF pillBd = isSelected ? p->badge_selected_border : p->badge_border;
                gdip_fill_and_stroke_round_rect(
                    rowGfx,
                    (REAL)col1_x + 0.5f, (REAL)pill_y + 0.5f, (REAL)pill_w - 1.0f, (REAL)pill_h - 1.0f, 6.0f,
                    colorref_to_argb(pillBg, 255),
                    colorref_to_argb(pillBd, 255),
                    1.0f
                );
            }

            SetTextColor(memDC, p->badge_fg);
            RECT c1 = {col1_x + 8, pill_y, col1_x + pill_w - 8, pill_y + pill_h};
            DrawTextW(memDC, row->trigger, -1, &c1, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            // Coluna 2: Resumo do Conteudo
            SelectObject(memDC, s_hFont);
            SetTextColor(memDC, isSelected ? p->text_primary : p->text_secondary);
            RECT c2 = {col2_x, row_y, col2_x + col2_w, row_y + TABLE_ROW_H - 1};
            DrawTextW(memDC, row->preview, -1, &c2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        if (rowGfx) {
            GdipDeleteGraphics(rowGfx);
        }
    }

    DeleteObject(hSepBr);
    SelectObject(memDC, oldFont);

    // 5. Scrollbar moderna e discreta (quando ha rolagem)
    if (has_scroll) {
        GpGraphics *sbGfx = NULL;
        if (GdipCreateFromHDC(memDC, &sbGfx) == Ok) {
            GdipSetSmoothingMode(sbGfx, SmoothingModeAntiAlias);
            GdipSetPixelOffsetMode(sbGfx, PixelOffsetModeHalf);

            bool activeThumb = s_table.hover_scrollbar || s_table.dragging_scrollbar;
            REAL thumbW = activeThumb ? 6.0f : 4.0f;
            REAL thumbX = (REAL)trackRc.right - thumbW - 2.0f;
            COLORREF thumbClr = activeThumb ? p->scrollbar_thumb_hover : p->scrollbar_thumb;

            gdip_fill_round_rect(
                sbGfx,
                thumbX, (REAL)thumbRc.top, thumbW, (REAL)(thumbRc.bottom - thumbRc.top), thumbW * 0.5f,
                colorref_to_argb(thumbClr, 255)
            );
            GdipDeleteGraphics(sbGfx);
        }
    }

    // 6. Remove clip, mascara cantos externos com antialiasing e desenha a borda do container
    SelectClipRgn(memDC, NULL);

    GpGraphics *borderGfx = NULL;
    if (GdipCreateFromHDC(memDC, &borderGfx) == Ok) {
        GdipSetSmoothingMode(borderGfx, SmoothingModeAntiAlias);
        GdipSetPixelOffsetMode(borderGfx, PixelOffsetModeHalf);

        gdip_mask_outside_round_rect(
            borderGfx, w, h,
            0.5f, 0.5f, (REAL)w - 1.0f, (REAL)h - 1.0f, TABLE_RADIUS,
            colorref_to_argb(p->bg_window, 255)
        );
        gdip_stroke_round_rect(
            borderGfx,
            0.5f, 0.5f, (REAL)w - 1.0f, (REAL)h - 1.0f, TABLE_RADIUS,
            colorref_to_argb(p->table_border, 255),
            1.0f
        );
        GdipDeleteGraphics(borderGfx);
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBm);
    DeleteObject(hbm);
    DeleteDC(memDC);
}

static LRESULT CALLBACK SnippetTableWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            table_paint(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
            table_paint(hwnd, (HDC)wParam);
            return 0;

        case WM_SIZE:
            table_clamp_scroll(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_MOUSEMOVE: {
            int x = (int)(short)LOWORD(lParam);
            int y = (int)(short)HIWORD(lParam);

            TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);

            if (s_table.dragging_scrollbar) {
                RECT trackRc, thumbRc;
                table_get_scrollbar_rect(hwnd, &trackRc, &thumbRc);
                int track_h = trackRc.bottom - trackRc.top;
                int thumb_h = thumbRc.bottom - thumbRc.top;
                int travel = track_h - thumb_h;
                int max_scroll = table_get_max_scroll(hwnd);

                if (travel > 0 && max_scroll > 0) {
                    int dy = y - s_table.drag_start_y;
                    s_table.scroll_y = s_table.drag_start_scroll + (dy * max_scroll) / travel;
                    table_clamp_scroll(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            RECT trackRc, thumbRc;
            table_get_scrollbar_rect(hwnd, &trackRc, &thumbRc);
            POINT pt = {x, y};
            bool over_sb = !IsRectEmpty(&trackRc) && PtInRect(&trackRc, pt);
            int new_hover = over_sb ? -1 : table_hit_test_row(hwnd, y);

            if (over_sb != s_table.hover_scrollbar || new_hover != s_table.hover_index) {
                s_table.hover_scrollbar = over_sb;
                s_table.hover_index = new_hover;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (s_table.hover_index != -1 || s_table.hover_scrollbar) {
                s_table.hover_index = -1;
                s_table.hover_scrollbar = false;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            int x = (int)(short)LOWORD(lParam);
            int y = (int)(short)HIWORD(lParam);

            RECT trackRc, thumbRc;
            table_get_scrollbar_rect(hwnd, &trackRc, &thumbRc);
            POINT pt = {x, y};

            if (!IsRectEmpty(&trackRc) && PtInRect(&trackRc, pt)) {
                if (PtInRect(&thumbRc, pt)) {
                    s_table.dragging_scrollbar = true;
                    s_table.drag_start_y = y;
                    s_table.drag_start_scroll = s_table.scroll_y;
                    SetCapture(hwnd);
                } else if (y < thumbRc.top) {
                    s_table.scroll_y -= table_get_body_height(hwnd);
                    table_clamp_scroll(hwnd);
                } else if (y > thumbRc.bottom) {
                    s_table.scroll_y += table_get_body_height(hwnd);
                    table_clamp_scroll(hwnd);
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            int idx = table_hit_test_row(hwnd, y);
            if (idx != -1) {
                table_select_index(hwnd, idx, false);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (s_table.dragging_scrollbar) {
                s_table.dragging_scrollbar = false;
                ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_CAPTURECHANGED: {
            s_table.dragging_scrollbar = false;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            int y = (int)(short)HIWORD(lParam);
            int idx = table_hit_test_row(hwnd, y);
            if (idx != -1) {
                table_select_index(hwnd, idx, false);
                PostMessageW(GetParent(hwnd), WM_APP_TABLE_DBLCLK, 0, 0);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int step = (TABLE_ROW_H * 2 * (-delta)) / WHEEL_DELTA;
            s_table.scroll_y += step;
            table_clamp_scroll(hwnd);

            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            RECT trackRc, thumbRc;
            table_get_scrollbar_rect(hwnd, &trackRc, &thumbRc);
            if (!IsRectEmpty(&trackRc) && PtInRect(&trackRc, pt)) {
                s_table.hover_index = -1;
            } else {
                s_table.hover_index = table_hit_test_row(hwnd, pt.y);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_KEYDOWN: {
            if (s_table.count == 0) break;
            int idx = s_table.selected_index;
            int page_rows = table_get_body_height(hwnd) / TABLE_ROW_H;
            if (page_rows < 1) page_rows = 1;

            switch (wParam) {
                case VK_UP:
                    idx = (idx <= 0) ? 0 : idx - 1;
                    table_select_index(hwnd, idx, true);
                    return 0;
                case VK_DOWN:
                    idx = (idx < 0) ? 0 : (idx + 1 >= s_table.count ? s_table.count - 1 : idx + 1);
                    table_select_index(hwnd, idx, true);
                    return 0;
                case VK_PRIOR:
                    idx = (idx < 0) ? 0 : (idx - page_rows < 0 ? 0 : idx - page_rows);
                    table_select_index(hwnd, idx, true);
                    return 0;
                case VK_NEXT:
                    idx = (idx < 0) ? 0 : (idx + page_rows >= s_table.count ? s_table.count - 1 : idx + page_rows);
                    table_select_index(hwnd, idx, true);
                    return 0;
                case VK_HOME:
                    table_select_index(hwnd, 0, true);
                    return 0;
                case VK_END:
                    table_select_index(hwnd, s_table.count - 1, true);
                    return 0;
                case VK_RETURN:
                    if (s_table.selected_index >= 0) {
                        PostMessageW(GetParent(hwnd), WM_APP_TABLE_DBLCLK, 0, 0);
                    }
                    return 0;
                case VK_DELETE:
                    if (s_table.selected_index >= 0) {
                        PostMessageW(GetParent(hwnd), WM_APP_TABLE_DELETE, 0, 0);
                    }
                    return 0;
            }
            break;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// -------------------------------------------------------------
// Bandeja do Sistema (System Tray)
// -------------------------------------------------------------

static void tray_init(HWND hwnd) {
    if (s_tray_created) return;

    memset(&s_nid, 0, sizeof(s_nid));
    s_nid.cbSize = sizeof(NOTIFYICONDATAW);
    s_nid.hWnd = hwnd;
    s_nid.uID = 1;
    s_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    s_nid.uCallbackMessage = WM_APP_TRAYMSG;
    s_nid.hIcon = s_hAppIcon;
    wcscpy(s_nid.szTip, L"Nimbox — Gerenciador de Snippets");

    s_tray_created = Shell_NotifyIconW(NIM_ADD, &s_nid);
}

static void tray_remove(void) {
    if (s_tray_created) {
        Shell_NotifyIconW(NIM_DELETE, &s_nid);
        s_tray_created = false;
    }
}

static void tray_show_menu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Abrir Nimbox");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

        if (hook_is_paused()) {
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE_HOOK, L"Retomar detecção");
        } else {
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE_HOOK, L"Pausar detecção");
        }

        if (theme_get_current_type() == THEME_DARK) {
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_THEME, L"Alternar para Tema Claro");
        } else {
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_THEME, L"Alternar para Tema Escuro");
        }

        bool autostart_enabled = autostart_is_enabled();
        if (s_settings) {
            s_settings->start_with_windows = autostart_enabled;
        }
        UINT autostart_flags = MF_STRING | (autostart_enabled ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(hMenu, autostart_flags, ID_TRAY_AUTOSTART, L"Iniciar com o Windows");

        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Sair");

        SetForegroundWindow(hwnd);
        TrackPopupMenuEx(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, hwnd, NULL);
        PostMessage(hwnd, WM_NULL, 0, 0);
        DestroyMenu(hMenu);
    }
}

// -------------------------------------------------------------
// Auxiliares de Pesquisa, Fontes e Selecao
// -------------------------------------------------------------

static bool string_contains_case_insensitive_w(const wchar_t *haystack, const wchar_t *needle) {
    if (!haystack || !needle) return false;
    if (needle[0] == L'\0') return true;

    size_t needle_len = wcslen(needle);
    size_t haystack_len = wcslen(haystack);
    if (needle_len > haystack_len) return false;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        size_t j = 0;
        while (j < needle_len && towlower(haystack[i + j]) == towlower(needle[j])) {
            j++;
        }
        if (j == needle_len) return true;
    }
    return false;
}

static void apply_font_to_window_and_children(HWND hwnd, HFONT font) {
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    HWND child = GetWindow(hwnd, GW_CHILD);
    while (child) {
        SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
        child = GetWindow(child, GW_HWNDNEXT);
    }
}

static int get_selected_snippet_id(void) {
    if (s_table.selected_index >= 0 && s_table.selected_index < s_table.count) {
        return s_table.rows[s_table.selected_index].id;
    }
    return -1;
}

static void get_search_container_rect(HWND hwnd, RECT *outRc) {
    RECT clientRc;
    GetClientRect(hwnd, &clientRc);
    int cw = clientRc.right - clientRc.left;

    int buttons_block_w = 142 + 8 + 82 + 8 + 82;
    int search_right = cw - UI_MARGIN - buttons_block_w - 14;
    if (search_right < UI_MARGIN + 180) search_right = UI_MARGIN + 180;

    outRc->left = UI_MARGIN;
    outRc->top = UI_TOOLBAR_Y;
    outRc->right = search_right;
    outRc->bottom = UI_TOOLBAR_Y + UI_TOOLBAR_H;
}

static void layout_main_controls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) return;

    // 1. Cabecalho: botao de tema alinhado a direita e centralizado com a logo
    if (s_hwndThemeBtn) {
        int themeY = UI_HEADER_Y + (UI_LOGO_SIZE - 34) / 2;
        MoveWindow(s_hwndThemeBtn, cw - UI_MARGIN - 104, themeY, 104, 34, FALSE);
    }

    // 2. Barra de pesquisa e acoes principais
    RECT searchRc;
    get_search_container_rect(hwnd, &searchRc);
    if (s_hwndSearch) {
        int editW = (searchRc.right - searchRc.left) - 44;
        if (editW < 80) editW = 80;
        MoveWindow(s_hwndSearch, searchRc.left + 34, searchRc.top + 9, editW, 18, FALSE);
    }

    int btn_x_del = cw - UI_MARGIN - 82;
    int btn_x_edit = btn_x_del - 8 - 82;
    int btn_x_new = btn_x_edit - 8 - 142;

    if (s_hwndBtnNew) MoveWindow(s_hwndBtnNew, btn_x_new, UI_TOOLBAR_Y, 142, UI_TOOLBAR_H, FALSE);
    if (s_hwndBtnEdit) MoveWindow(s_hwndBtnEdit, btn_x_edit, UI_TOOLBAR_Y, 82, UI_TOOLBAR_H, FALSE);
    if (s_hwndBtnDelete) MoveWindow(s_hwndBtnDelete, btn_x_del, UI_TOOLBAR_Y, 82, UI_TOOLBAR_H, FALSE);

    // 3. Tabela de snippets responsiva
    int table_w = cw - (UI_MARGIN * 2);
    int table_h = ch - UI_TABLE_Y - UI_FOOTER_H;
    if (table_w < 200) table_w = 200;
    if (table_h < 120) table_h = 120;

    if (s_hwndTable) {
        MoveWindow(s_hwndTable, UI_MARGIN, UI_TABLE_Y, table_w, table_h, FALSE);
    }

    // 4. Rodape: botao Pausar/Retomar deteccao alinhado a direita
    if (s_hwndToggleHook) {
        MoveWindow(s_hwndToggleHook, cw - UI_MARGIN - 158, ch - 48, 158, 34, FALSE);
    }
}

void gui_apply_theme(AppTheme theme) {
    theme_set(theme);
    const ThemePalette *p = theme_get_current();

    if (s_hwndMain) {
        apply_window_decorations(s_hwndMain, p->is_dark);
        SetClassLongPtrW(s_hwndMain, GCLP_HBRBACKGROUND, (LONG_PTR)theme_get_window_brush());
    }

    if (s_hwndModal) {
        apply_window_decorations(s_hwndModal, p->is_dark);
        SetClassLongPtrW(s_hwndModal, GCLP_HBRBACKGROUND, (LONG_PTR)theme_get_window_brush());
        if (s_hwndModalContent) {
            SetWindowTheme(s_hwndModalContent, p->is_dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
        }
        RedrawWindow(s_hwndModal, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
    }

    if (s_hwndMain) {
        gui_refresh_list();
        RedrawWindow(s_hwndMain, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

void gui_refresh_list(void) {
    if (!s_hwndTable || !s_snippets) return;

    int prev_selected_id = s_table.selected_id;
    table_clear_rows();

    wchar_t search_text[128] = {0};
    if (s_hwndSearch) {
        GetWindowTextW(s_hwndSearch, search_text, 128);
    }
    s_search_has_text = (search_text[0] != L'\0');

    int new_selected_index = -1;

    for (size_t i = 0; i < s_snippets->count; i++) {
        Snippet *s = &s_snippets->items[i];

        wchar_t w_name[MAX_NAME_LEN];
        wchar_t w_trigger[MAX_TRIGGER_LEN];
        wchar_t w_content[MAX_CONTENT_LEN];

        utf8_to_wchar_buf(s->name, w_name, MAX_NAME_LEN);
        utf8_to_wchar_buf(s->trigger, w_trigger, MAX_TRIGGER_LEN);
        utf8_to_wchar_buf(s->content, w_content, MAX_CONTENT_LEN);

        if (s_search_has_text) {
            bool match_name = string_contains_case_insensitive_w(w_name, search_text);
            bool match_trigger = string_contains_case_insensitive_w(w_trigger, search_text);
            if (!match_name && !match_trigger) {
                continue;
            }
        }

        wchar_t preview[160];
        wcsncpy(preview, w_content, 159);
        preview[159] = L'\0';
        for (wchar_t *ch = preview; *ch; ch++) {
            if (*ch == L'\r' || *ch == L'\n' || *ch == L'\t') *ch = L' ';
        }

        table_add_row(s->id, w_name, w_trigger, preview);
        if (s->id == prev_selected_id) {
            new_selected_index = s_table.count - 1;
        }
    }

    s_visible_snippet_count = s_table.count;
    s_table.selected_index = new_selected_index;
    if (new_selected_index == -1) {
        s_table.selected_id = -1;
    }

    table_clamp_scroll(s_hwndTable);
    InvalidateRect(s_hwndTable, NULL, FALSE);

    if (s_hwndMain) {
        RECT rc;
        GetClientRect(s_hwndMain, &rc);
        RECT footerRc = {0, rc.bottom - UI_FOOTER_H, rc.right, rc.bottom};
        InvalidateRect(s_hwndMain, &footerRc, FALSE);
    }
    if (s_hwndToggleHook) InvalidateRect(s_hwndToggleHook, NULL, FALSE);
    if (s_hwndThemeBtn) InvalidateRect(s_hwndThemeBtn, NULL, FALSE);
}

// -------------------------------------------------------------
// Janela Modal de Criacao e Edicao de Snippet
// -------------------------------------------------------------

static LRESULT CALLBACK ModalContentEditSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)dwRefData;
    switch (msg) {
        case WM_GETDLGCODE: {
            LRESULT code = DefSubclassProc(hwnd, msg, wParam, lParam);
            MSG *pMsg = (MSG *)lParam;
            // O estilo ES_WANTRETURN faz o EDIT multilinha retornar DLGC_WANTALLKEYS ate para VK_TAB;
            // removemos essas flags apenas para Tab sem Ctrl para permitir navegacao via IsDialogMessageW.
            if ((wParam == VK_TAB || (pMsg && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)) &&
                !(GetKeyState(VK_CONTROL) & 0x8000)) {
                code &= ~(DLGC_WANTALLKEYS | DLGC_WANTTAB);
            }
            return code;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ModalContentEditSubclass, uIdSubclass);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ModalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const ThemePalette *p = theme_get_current();

    switch (msg) {
        case WM_CREATE: {
            apply_window_decorations(hwnd, p->is_dark);
            break;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int w = rcClient.right - rcClient.left;
            int h = rcClient.bottom - rcClient.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP hbm = CreateCompatibleBitmap(hdc, w, h);
            HBITMAP oldBm = (HBITMAP)SelectObject(memDC, hbm);

            HBRUSH hBg = CreateSolidBrush(p->bg_window);
            FillRect(memDC, &rcClient, hBg);
            DeleteObject(hBg);

            GpGraphics *gfx = NULL;
            if (GdipCreateFromHDC(memDC, &gfx) == Ok) {
                GdipSetSmoothingMode(gfx, SmoothingModeAntiAlias);
                GdipSetPixelOffsetMode(gfx, PixelOffsetModeHalf);

                HWND focused = GetFocus();
                HWND edits[] = { s_hwndModalName, s_hwndModalTrigger, s_hwndModalContent };
                int padsY[] = { 7, 7, 6 };

                for (int i = 0; i < 3; i++) {
                    if (edits[i]) {
                        RECT rc;
                        GetWindowRect(edits[i], &rc);
                        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&rc, 2);
                        InflateRect(&rc, 10, padsY[i]);

                        bool isFocused = (focused == edits[i]);
                        COLORREF borderClr = isFocused ? p->border_focus : p->border_normal;

                        gdip_fill_and_stroke_round_rect(
                            gfx,
                            (REAL)rc.left + 0.5f, (REAL)rc.top + 0.5f,
                            (REAL)(rc.right - rc.left) - 1.0f, (REAL)(rc.bottom - rc.top) - 1.0f,
                            8.0f,
                            colorref_to_argb(p->bg_surface, 255),
                            colorref_to_argb(borderClr, 255),
                            isFocused ? 1.5f : 1.0f
                        );
                    }
                }
                GdipDeleteGraphics(gfx);
            }

            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBm);
            DeleteObject(hbm);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, p->text_primary);
            SetBkColor(hdcStatic, p->bg_window);
            return (LRESULT)theme_get_window_brush();
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, p->text_primary);
            SetBkColor(hdcEdit, p->bg_surface);
            return (LRESULT)theme_get_surface_brush();
        }

        case WM_CTLCOLORBTN: {
            return (LRESULT)theme_get_window_brush();
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                if (dis->CtlID == ID_MODAL_SAVE) {
                    draw_modern_button(dis, L"Salvar", BTN_ICON_NONE, p->bg_window,
                        p->accent, p->accent_hover, p->accent_active,
                        p->accent, p->accent_hover, p->accent_text, s_hFontBold);
                    return TRUE;
                } else if (dis->CtlID == ID_MODAL_CANCEL) {
                    draw_modern_button(dis, L"Cancelar", BTN_ICON_NONE, p->bg_window,
                        p->bg_surface, p->bg_hover, p->bg_active,
                        p->border_normal, p->border_hover, p->text_primary, s_hFont);
                    return TRUE;
                }
            }
            break;
        }

        case WM_COMMAND: {
            int control_id = LOWORD(wParam);
            int notification = HIWORD(wParam);

            if ((control_id == ID_MODAL_NAME || control_id == ID_MODAL_TRIGGER || control_id == ID_MODAL_CONTENT) &&
                (notification == EN_SETFOCUS || notification == EN_KILLFOCUS)) {
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            if (control_id == ID_MODAL_SAVE) {
                wchar_t w_name[MAX_NAME_LEN] = {0};
                wchar_t w_trigger[MAX_TRIGGER_LEN] = {0};
                wchar_t w_content[MAX_CONTENT_LEN] = {0};

                GetWindowTextW(s_hwndModalName, w_name, MAX_NAME_LEN);
                GetWindowTextW(s_hwndModalTrigger, w_trigger, MAX_TRIGGER_LEN);
                GetWindowTextW(s_hwndModalContent, w_content, MAX_CONTENT_LEN);

                char name[MAX_NAME_LEN] = {0};
                char trigger[MAX_TRIGGER_LEN] = {0};
                char content[MAX_CONTENT_LEN] = {0};

                wchar_to_utf8_buf(w_name, name, sizeof(name));
                wchar_to_utf8_buf(w_trigger, trigger, sizeof(trigger));
                wchar_to_utf8_buf(w_content, content, sizeof(content));

                if (strlen(name) == 0) {
                    MessageBoxW(hwnd, L"Por favor, digite um nome para o snippet.", L"Campo Obrigatório", MB_OK | MB_ICONWARNING);
                    SetFocus(s_hwndModalName);
                    return 0;
                }
                if (strlen(trigger) == 0) {
                    MessageBoxW(hwnd, L"Por favor, defina um gatilho (ex: email ou ;email).", L"Campo Obrigatório", MB_OK | MB_ICONWARNING);
                    SetFocus(s_hwndModalTrigger);
                    return 0;
                }
                if (strlen(content) == 0) {
                    MessageBoxW(hwnd, L"Por favor, digite o texto a ser expandido.", L"Campo Obrigatório", MB_OK | MB_ICONWARNING);
                    SetFocus(s_hwndModalContent);
                    return 0;
                }

                Snippet *existing_with_trigger = snippet_list_find_by_trigger(s_snippets, trigger);
                if (existing_with_trigger && (!s_current_editing || existing_with_trigger->id != s_current_editing->id)) {
                    MessageBoxW(hwnd, L"Já existe um snippet com este mesmo gatilho. Escolha outro.", L"Gatilho Duplicado", MB_OK | MB_ICONWARNING);
                    SetFocus(s_hwndModalTrigger);
                    return 0;
                }

                if (s_current_editing) {
                    snprintf(s_current_editing->name, sizeof(s_current_editing->name), "%s", name);
                    snprintf(s_current_editing->trigger, sizeof(s_current_editing->trigger), "%s", trigger);
                    snprintf(s_current_editing->content, sizeof(s_current_editing->content), "%s", content);
                    snippet_list_update(s_snippets, s_current_editing);
                    s_table.selected_id = s_current_editing->id;
                } else {
                    Snippet new_snip;
                    memset(&new_snip, 0, sizeof(Snippet));
                    new_snip.id = snippet_list_get_next_id(s_snippets);
                    snprintf(new_snip.name, sizeof(new_snip.name), "%s", name);
                    snprintf(new_snip.trigger, sizeof(new_snip.trigger), "%s", trigger);
                    snprintf(new_snip.content, sizeof(new_snip.content), "%s", content);
                    snippet_list_add(s_snippets, &new_snip);
                    s_table.selected_id = new_snip.id;
                }

                storage_save_snippets(s_snippets);
                gui_refresh_list();

                EnableWindow(s_hwndMain, TRUE);
                DestroyWindow(hwnd);
                s_hwndModal = NULL;
                s_current_editing = NULL;
                SetActiveWindow(s_hwndMain);
                return 0;
            } else if (control_id == ID_MODAL_CANCEL || control_id == IDCANCEL) {
                EnableWindow(s_hwndMain, TRUE);
                DestroyWindow(hwnd);
                s_hwndModal = NULL;
                s_current_editing = NULL;
                SetActiveWindow(s_hwndMain);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            EnableWindow(s_hwndMain, TRUE);
            DestroyWindow(hwnd);
            s_hwndModal = NULL;
            s_current_editing = NULL;
            SetActiveWindow(s_hwndMain);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static void open_snippet_modal(HWND hParent, Snippet *snippet_to_edit) {
    if (s_hwndModal) {
        SetActiveWindow(s_hwndModal);
        return;
    }

    s_current_editing = snippet_to_edit;
    const ThemePalette *p = theme_get_current();
    const wchar_t *title = snippet_to_edit ? L"Nimbox — Editar Snippet" : L"Nimbox — Novo Snippet";

    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = ModalWndProc;
        wc.hInstance = s_hInstance;
        wc.lpszClassName = L"NimboxModalClassW";
        wc.hbrBackground = theme_get_window_brush();
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        class_registered = true;
    }

    RECT rcMain;
    GetWindowRect(hParent, &rcMain);
    int modal_width = 500;
    int modal_height = 456;
    int x = rcMain.left + (rcMain.right - rcMain.left - modal_width) / 2;
    int y = rcMain.top + (rcMain.bottom - rcMain.top - modal_height) / 2;

    s_hwndModal = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"NimboxModalClassW",
        title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, modal_width, modal_height,
        hParent, NULL, s_hInstance, NULL
    );

    apply_window_decorations(s_hwndModal, p->is_dark);
    SendMessageW(s_hwndModal, WM_SETICON, ICON_BIG, (LPARAM)s_hAppIcon);
    SendMessageW(s_hwndModal, WM_SETICON, ICON_SMALL, (LPARAM)s_hAppIcon);
    EnableWindow(hParent, FALSE);

    // Labels e campos de entrada alinhados dentro das caixas arredondadas
    HWND lbl1 = CreateWindowW(L"STATIC", L"Nome do Snippet", WS_CHILD | WS_VISIBLE, 24, 20, 436, 18, s_hwndModal, NULL, s_hInstance, NULL);
    s_hwndModalName = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 34, 49, 416, 18, s_hwndModal, (HMENU)ID_MODAL_NAME, s_hInstance, NULL);

    HWND lbl2 = CreateWindowW(L"STATIC", L"Palavra-chave / Gatilho (ex: email ou ;email)", WS_CHILD | WS_VISIBLE, 24, 86, 436, 18, s_hwndModal, NULL, s_hInstance, NULL);
    s_hwndModalTrigger = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 34, 115, 416, 18, s_hwndModal, (HMENU)ID_MODAL_TRIGGER, s_hInstance, NULL);

    HWND lbl3 = CreateWindowW(L"STATIC", L"Conteúdo a ser inserido", WS_CHILD | WS_VISIBLE, 24, 152, 436, 18, s_hwndModal, NULL, s_hInstance, NULL);
    s_hwndModalContent = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP, 34, 180, 416, 156, s_hwndModal, (HMENU)ID_MODAL_CONTENT, s_hInstance, NULL);
    SetWindowSubclass(s_hwndModalContent, ModalContentEditSubclass, ID_MODAL_CONTENT, 0);
    SetWindowTheme(s_hwndModalContent, p->is_dark ? L"DarkMode_Explorer" : L"Explorer", NULL);

    HWND btnSave = CreateWindowW(L"BUTTON", L"Salvar", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 248, 362, 102, 34, s_hwndModal, (HMENU)ID_MODAL_SAVE, s_hInstance, NULL);
    HWND btnCancel = CreateWindowW(L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 358, 362, 102, 34, s_hwndModal, (HMENU)ID_MODAL_CANCEL, s_hInstance, NULL);

    SetWindowSubclass(btnSave, ModernButtonSubclass, ID_MODAL_SAVE, 0);
    SetWindowSubclass(btnCancel, ModernButtonSubclass, ID_MODAL_CANCEL, 0);

    apply_font_to_window_and_children(s_hwndModal, s_hFont);
    SendMessageW(lbl1, WM_SETFONT, (WPARAM)s_hFontBold, TRUE);
    SendMessageW(lbl2, WM_SETFONT, (WPARAM)s_hFontBold, TRUE);
    SendMessageW(lbl3, WM_SETFONT, (WPARAM)s_hFontBold, TRUE);

    if (snippet_to_edit) {
        wchar_t w_buf[MAX_CONTENT_LEN];
        utf8_to_wchar_buf(snippet_to_edit->name, w_buf, MAX_NAME_LEN);
        SetWindowTextW(s_hwndModalName, w_buf);

        utf8_to_wchar_buf(snippet_to_edit->trigger, w_buf, MAX_TRIGGER_LEN);
        SetWindowTextW(s_hwndModalTrigger, w_buf);

        utf8_to_wchar_buf(snippet_to_edit->content, w_buf, MAX_CONTENT_LEN);
        SetWindowTextW(s_hwndModalContent, w_buf);
    }

    SetFocus(s_hwndModalName);
}

// -------------------------------------------------------------
// Pintura da Janela Principal (Cabecalho, Logo, Busca, Rodape)
// -------------------------------------------------------------

static void paint_main_window(HWND hwnd, HDC hdc) {
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;
    if (w <= 0 || h <= 0) return;

    const ThemePalette *p = theme_get_current();

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBm = (HBITMAP)SelectObject(memDC, hbm);

    HBRUSH hBg = CreateSolidBrush(p->bg_window);
    FillRect(memDC, &rcClient, hBg);
    DeleteObject(hBg);

    GpGraphics *gfx = NULL;
    if (GdipCreateFromHDC(memDC, &gfx) == Ok) {
        GdipSetSmoothingMode(gfx, SmoothingModeAntiAlias);
        GdipSetPixelOffsetMode(gfx, PixelOffsetModeHighQuality);
        GdipSetInterpolationMode(gfx, InterpolationModeHighQualityBicubic);

        // 1. Logo PNG em alta qualidade preservando proporcao original
        int textLeftX = UI_MARGIN;
        if (s_pLogoBitmap) {
            UINT imgW = 0, imgH = 0;
            GdipGetImageWidth((GpImage *)s_pLogoBitmap, &imgW);
            GdipGetImageHeight((GpImage *)s_pLogoBitmap, &imgH);

            int dstW = UI_LOGO_SIZE;
            int dstH = UI_LOGO_SIZE;
            if (imgW > 0 && imgH > 0) {
                if (imgW >= imgH) {
                    dstH = (int)((imgH * (UINT)UI_LOGO_SIZE) / imgW);
                } else {
                    dstW = (int)((imgW * (UINT)UI_LOGO_SIZE) / imgH);
                }
            }
            int logoX = UI_MARGIN + (UI_LOGO_SIZE - dstW) / 2;
            int logoY = UI_HEADER_Y + (UI_LOGO_SIZE - dstH) / 2;
            GdipDrawImageRectI(gfx, (GpImage *)s_pLogoBitmap, logoX, logoY, dstW, dstH);
            textLeftX = UI_MARGIN + UI_LOGO_SIZE + 14;
        }

        // 2. Titulo e subtitulo da aplicacao no cabecalho (centralizados verticalmente com a logo)
        int textTopY = UI_HEADER_Y + (UI_LOGO_SIZE - 46) / 2;
        SetBkMode(memDC, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(memDC, s_hFontTitle);
        SetTextColor(memDC, p->text_primary);
        RECT titleRc = {textLeftX, textTopY, textLeftX + 340, textTopY + 28};
        DrawTextW(memDC, L"Nimbox", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(memDC, s_hFontSmall);
        SetTextColor(memDC, p->text_secondary);
        RECT subRc = {textLeftX, textTopY + 27, textLeftX + 380, textTopY + 46};
        DrawTextW(memDC, L"Gerenciador de snippets de texto", -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(memDC, oldFont);

        // 3. Container arredondado da barra de pesquisa + icone de lupa
        GdipSetPixelOffsetMode(gfx, PixelOffsetModeHalf);
        if (s_hwndSearch) {
            RECT searchRc;
            get_search_container_rect(hwnd, &searchRc);
            bool searchFocused = (GetFocus() == s_hwndSearch);
            COLORREF borderClr = searchFocused ? p->border_focus : p->border_normal;

            gdip_fill_and_stroke_round_rect(
                gfx,
                (REAL)searchRc.left + 0.5f, (REAL)searchRc.top + 0.5f,
                (REAL)(searchRc.right - searchRc.left) - 1.0f,
                (REAL)(searchRc.bottom - searchRc.top) - 1.0f,
                8.0f,
                colorref_to_argb(p->bg_surface, 255),
                colorref_to_argb(borderClr, 255),
                searchFocused ? 1.5f : 1.0f
            );

            // Icone de lupa vetorial nitido
            GpPen *searchPen = NULL;
            if (GdipCreatePen1(colorref_to_argb(p->text_muted, 255), 1.5f, UnitPixel, &searchPen) == Ok) {
                REAL lx = (REAL)searchRc.left + 13.0f;
                REAL ly = (REAL)searchRc.top + 11.0f;
                GdipDrawEllipse(gfx, searchPen, lx, ly, 9.5f, 9.5f);
                GdipDrawLine(gfx, searchPen, lx + 8.3f, ly + 8.3f, lx + 12.2f, ly + 12.2f);
                GdipDeletePen(searchPen);
            }
        }

        // 4. Linha divisoria sutil do rodape e indicador de status em pill
        int divY = h - UI_FOOTER_H + 8;
        RECT divRc = {UI_MARGIN, divY, w - UI_MARGIN, divY + 1};
        HBRUSH hDivBr = CreateSolidBrush(p->table_separator);
        FillRect(memDC, &divRc, hDivBr);
        DeleteObject(hDivBr);

        bool paused = hook_is_paused();
        const wchar_t *badgeText = paused ? L"Detecção pausada" : L"Detecção ativa";
        COLORREF pillBg = paused ? p->status_paused_bg : p->status_active_bg;
        COLORREF pillBorder = paused ? p->status_paused_border : p->status_active_border;
        COLORREF pillFg = paused ? p->status_paused_fg : p->status_active_fg;

        oldFont = (HFONT)SelectObject(memDC, s_hFontBold);
        SIZE badgeSz = {0, 0};
        GetTextExtentPoint32W(memDC, badgeText, (int)wcslen(badgeText), &badgeSz);

        int pillX = UI_MARGIN;
        int pillY = h - 45;
        int pillW = 26 + badgeSz.cx + 12;
        int pillH = 28;

        gdip_fill_and_stroke_round_rect(
            gfx,
            (REAL)pillX + 0.5f, (REAL)pillY + 0.5f,
            (REAL)pillW - 1.0f, (REAL)pillH - 1.0f,
            14.0f,
            colorref_to_argb(pillBg, 255),
            colorref_to_argb(pillBorder, 255),
            1.0f
        );

        // Ponto indicador suavizado
        GpSolidFill *dotBrush = NULL;
        if (GdipCreateSolidFill(colorref_to_argb(pillFg, 255), &dotBrush) == Ok) {
            GdipFillEllipse(gfx, (GpBrush *)dotBrush, (REAL)(pillX + 11), (REAL)(pillY + 10), 7.5f, 7.5f);
            GdipDeleteBrush((GpBrush *)dotBrush);
        }

        SetTextColor(memDC, pillFg);
        RECT badgeRc = {pillX + 24, pillY, pillX + pillW - 10, pillY + pillH};
        DrawTextW(memDC, badgeText, -1, &badgeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Contador de snippets ao lado da pill
        wchar_t countText[128];
        size_t totalCount = s_snippets ? s_snippets->count : 0;
        if (s_search_has_text) {
            swprintf(countText, 128, L"•   Exibindo %d de %zu snippets", s_visible_snippet_count, totalCount);
        } else if (totalCount == 1) {
            swprintf(countText, 128, L"•   1 snippet cadastrado");
        } else {
            swprintf(countText, 128, L"•   %zu snippets cadastrados", totalCount);
        }

        SelectObject(memDC, s_hFont);
        SetTextColor(memDC, p->text_secondary);
        int countRight = w - UI_MARGIN - 172;
        if (countRight > pillX + pillW + 14) {
            RECT countRc = {pillX + pillW + 14, pillY, countRight, pillY + pillH};
            DrawTextW(memDC, countText, -1, &countRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        SelectObject(memDC, oldFont);
        GdipDeleteGraphics(gfx);
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBm);
    DeleteObject(hbm);
    DeleteDC(memDC);
}

// -------------------------------------------------------------
// Procedimento da Janela Principal (Main WndProc Unicode)
// -------------------------------------------------------------

static void handle_edit_selected_snippet(HWND hwnd) {
    int id = get_selected_snippet_id();
    if (id != -1) {
        Snippet *s = snippet_list_find_by_id(s_snippets, id);
        if (s) open_snippet_modal(hwnd, s);
    } else {
        MessageBoxW(hwnd, L"Selecione um snippet na lista para editar.", L"Nenhum Snippet Selecionado", MB_OK | MB_ICONINFORMATION);
    }
}

static void handle_delete_selected_snippet(HWND hwnd) {
    int id = get_selected_snippet_id();
    if (id != -1) {
        Snippet *s = snippet_list_find_by_id(s_snippets, id);
        if (s) {
            wchar_t msg[300];
            wchar_t w_name[MAX_NAME_LEN];
            utf8_to_wchar_buf(s->name, w_name, MAX_NAME_LEN);
            swprintf(msg, 300, L"Tem certeza que deseja excluir o snippet \"%ls\"?", w_name);

            int choice = MessageBoxW(hwnd, msg, L"Confirmar Exclusão", MB_YESNO | MB_ICONQUESTION);
            if (choice == IDYES) {
                snippet_list_remove(s_snippets, id);
                storage_save_snippets(s_snippets);
                gui_refresh_list();
            }
        }
    } else {
        MessageBoxW(hwnd, L"Selecione um snippet na lista para excluir.", L"Nenhum Snippet Selecionado", MB_OK | MB_ICONINFORMATION);
    }
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const ThemePalette *p = theme_get_current();

    switch (msg) {
        case WM_CREATE: {
            apply_window_decorations(hwnd, p->is_dark);

            SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)s_hAppIcon);
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)s_hAppIcon);

            // 1. Botao de alternancia de tema no cabecalho
            s_hwndThemeBtn = CreateWindowW(
                L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                0, 0, 104, 34,
                hwnd, (HMENU)ID_BTN_THEME, s_hInstance, NULL
            );
            SetWindowSubclass(s_hwndThemeBtn, ModernButtonSubclass, ID_BTN_THEME, 0);

            // 2. Barra de busca
            s_hwndSearch = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                0, 0, 300, 18,
                hwnd, (HMENU)ID_SEARCH_BOX, s_hInstance, NULL
            );
            SendMessageW(s_hwndSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Pesquisar por nome ou gatilho...");

            // 3. Botoes de acao
            s_hwndBtnNew = CreateWindowW(L"BUTTON", L"Novo Snippet", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 142, 36, hwnd, (HMENU)ID_BTN_NEW, s_hInstance, NULL);
            s_hwndBtnEdit = CreateWindowW(L"BUTTON", L"Editar", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 82, 36, hwnd, (HMENU)ID_BTN_EDIT, s_hInstance, NULL);
            s_hwndBtnDelete = CreateWindowW(L"BUTTON", L"Excluir", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 82, 36, hwnd, (HMENU)ID_BTN_DELETE, s_hInstance, NULL);

            SetWindowSubclass(s_hwndBtnNew, ModernButtonSubclass, ID_BTN_NEW, 0);
            SetWindowSubclass(s_hwndBtnEdit, ModernButtonSubclass, ID_BTN_EDIT, 0);
            SetWindowSubclass(s_hwndBtnDelete, ModernButtonSubclass, ID_BTN_DELETE, 0);

            // 4. Tabela customizada moderna de snippets
            s_hwndTable = CreateWindowExW(
                0, L"NimboxSnippetTableW", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 400, 300,
                hwnd, (HMENU)ID_SNIPPET_TABLE, s_hInstance, NULL
            );

            // 5. Botao de pausa/retomada no rodape
            s_hwndToggleHook = CreateWindowW(
                L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 158, 34,
                hwnd, (HMENU)ID_BTN_TOGGLE_HOOK, s_hInstance, NULL
            );
            SetWindowSubclass(s_hwndToggleHook, ModernButtonSubclass, ID_BTN_TOGGLE_HOOK, 0);

            apply_font_to_window_and_children(hwnd, s_hFont);
            layout_main_controls(hwnd);

            tray_init(hwnd);
            gui_refresh_list();
            break;
        }

        case WM_GETMINMAXINFO: {
            LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
            mmi->ptMinTrackSize.x = 720;
            mmi->ptMinTrackSize.y = 480;
            return 0;
        }

        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                layout_main_controls(hwnd);
                RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Pintura 100% double-buffered em WM_PAINT

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint_main_window(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
            paint_main_window(hwnd, (HDC)wParam);
            return 0;

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, p->text_primary);
            SetBkColor(hdcStatic, p->bg_window);
            return (LRESULT)theme_get_window_brush();
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, p->text_primary);
            SetBkColor(hdcEdit, p->bg_surface);
            return (LRESULT)theme_get_surface_brush();
        }

        case WM_CTLCOLORBTN: {
            return (LRESULT)theme_get_window_brush();
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                if (dis->CtlID == ID_BTN_NEW) {
                    draw_modern_button(dis, L"Novo Snippet", BTN_ICON_PLUS, p->bg_window,
                        p->accent, p->accent_hover, p->accent_active,
                        p->accent, p->accent_hover, p->accent_text, s_hFontBold);
                    return TRUE;
                } else if (dis->CtlID == ID_BTN_THEME) {
                    const wchar_t *theme_text = (p->type == THEME_DARK) ? L"Claro" : L"Escuro";
                    ButtonIconType theme_icon = (p->type == THEME_DARK) ? BTN_ICON_SUN : BTN_ICON_MOON;
                    draw_modern_button(dis, theme_text, theme_icon, p->bg_window,
                        p->bg_surface, p->bg_hover, p->bg_active,
                        p->border_normal, p->border_hover, p->text_primary, s_hFont);
                    return TRUE;
                } else if (dis->CtlID == ID_BTN_EDIT) {
                    draw_modern_button(dis, L"Editar", BTN_ICON_NONE, p->bg_window,
                        p->bg_surface, p->bg_hover, p->bg_active,
                        p->border_normal, p->border_hover, p->text_primary, s_hFont);
                    return TRUE;
                } else if (dis->CtlID == ID_BTN_DELETE) {
                    draw_modern_button(dis, L"Excluir", BTN_ICON_NONE, p->bg_window,
                        p->bg_surface, p->danger_hover_bg, p->danger_active_bg,
                        p->border_normal, p->danger_hover_border, p->danger_fg, s_hFont);
                    return TRUE;
                } else if (dis->CtlID == ID_BTN_TOGGLE_HOOK) {
                    bool paused = hook_is_paused();
                    if (paused) {
                        draw_modern_button(dis, L"Retomar detecção", BTN_ICON_NONE, p->bg_window,
                            p->status_paused_bg, p->status_paused_hover_bg, p->status_paused_active_bg,
                            p->status_paused_border, p->danger_hover_border, p->status_paused_fg, s_hFontBold);
                    } else {
                        draw_modern_button(dis, L"Pausar detecção", BTN_ICON_NONE, p->bg_window,
                            p->bg_surface, p->bg_hover, p->bg_active,
                            p->border_normal, p->border_hover, p->text_secondary, s_hFont);
                    }
                    return TRUE;
                }
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = {(int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam)};
            RECT searchRc;
            get_search_container_rect(hwnd, &searchRc);
            if (s_hwndSearch && PtInRect(&searchRc, pt)) {
                SetFocus(s_hwndSearch);
            }
            return 0;
        }

        case WM_COMMAND: {
            int control_id = LOWORD(wParam);
            int notification = HIWORD(wParam);

            if (control_id == ID_SEARCH_BOX) {
                if (notification == EN_CHANGE) {
                    gui_refresh_list();
                } else if (notification == EN_SETFOCUS || notification == EN_KILLFOCUS) {
                    RECT searchRc;
                    get_search_container_rect(hwnd, &searchRc);
                    InvalidateRect(hwnd, &searchRc, FALSE);
                }
            } else if (control_id == ID_BTN_THEME) {
                AppTheme next = theme_toggle();
                if (s_settings) {
                    s_settings->theme = next;
                    settings_save(s_settings);
                }
                gui_apply_theme(next);
            } else if (control_id == ID_BTN_NEW) {
                open_snippet_modal(hwnd, NULL);
            } else if (control_id == ID_BTN_EDIT) {
                handle_edit_selected_snippet(hwnd);
            } else if (control_id == ID_BTN_DELETE) {
                handle_delete_selected_snippet(hwnd);
            } else if (control_id == ID_BTN_TOGGLE_HOOK) {
                bool new_state = !hook_is_paused();
                hook_set_paused(new_state);
                gui_refresh_list();
            } else if (control_id == ID_TRAY_OPEN) {
                gui_show_window(true);
            } else if (control_id == ID_TRAY_TOGGLE_HOOK) {
                bool new_state = !hook_is_paused();
                hook_set_paused(new_state);
                gui_refresh_list();
            } else if (control_id == ID_TRAY_THEME) {
                AppTheme next = theme_toggle();
                if (s_settings) {
                    s_settings->theme = next;
                    settings_save(s_settings);
                }
                gui_apply_theme(next);
            } else if (control_id == ID_TRAY_AUTOSTART) {
                bool desired = !autostart_is_enabled();
                if (!autostart_set_enabled(desired)) {
                    MessageBoxW(
                        hwnd,
                        L"Não foi possível atualizar a inicialização com o Windows no Registro.",
                        L"Erro de Inicialização",
                        MB_OK | MB_ICONERROR
                    );
                }
                if (s_settings) {
                    s_settings->start_with_windows = autostart_is_enabled();
                    settings_save(s_settings);
                }
            } else if (control_id == ID_TRAY_EXIT) {
                tray_remove();
                DestroyWindow(hwnd);
            }
            break;
        }

        case WM_APP_TABLE_DBLCLK:
            handle_edit_selected_snippet(hwnd);
            return 0;

        case WM_APP_TABLE_DELETE:
            handle_delete_selected_snippet(hwnd);
            return 0;

        case WM_APP_EXPAND_SNIPPET: {
            int snippet_id = (int)wParam;
            Snippet *s = snippet_list_find_by_id(s_snippets, snippet_id);
            if (s && !hook_is_paused()) {
                injector_replace_snippet(s);
            }
            break;
        }

        case WM_APP_TRAYMSG: {
            if (lParam == WM_LBUTTONDBLCLK) {
                gui_show_window(true);
            } else if (lParam == WM_RBUTTONUP) {
                tray_show_menu(hwnd);
            }
            break;
        }

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            tray_remove();
            hook_cleanup();
            theme_cleanup();
            table_free_rows();
            if (s_pLogoBitmap) {
                GdipDisposeImage((GpImage *)s_pLogoBitmap);
                s_pLogoBitmap = NULL;
            }
            if (s_gdipToken) {
                GdiplusShutdown(s_gdipToken);
                s_gdipToken = 0;
            }
            if (s_hFont) DeleteObject(s_hFont);
            if (s_hFontBold) DeleteObject(s_hFontBold);
            if (s_hFontTitle) DeleteObject(s_hFontTitle);
            if (s_hFontSmall) DeleteObject(s_hFontSmall);
            if (s_hFontMono) DeleteObject(s_hFontMono);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

bool gui_init(HINSTANCE hInstance, int nCmdShow, SnippetList *snippets, AppSettings *settings) {
    s_hInstance = hInstance;
    s_snippets = snippets;
    s_settings = settings;
    s_table.selected_index = -1;
    s_table.selected_id = -1;
    s_table.hover_index = -1;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Inicializa GDI+ para renderizacao de cantos suaves com antialiasing e logo PNG
    GdiplusStartupInput gdipInput;
    memset(&gdipInput, 0, sizeof(gdipInput));
    gdipInput.GdiplusVersion = 1;
    GdiplusStartup(&s_gdipToken, &gdipInput, NULL);

    load_logo_bitmap(hInstance);

    theme_init(settings ? settings->theme : THEME_DARK);
    const ThemePalette *p = theme_get_current();

    s_hAppIcon = (HICON)LoadImageW(
        hInstance,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        0, 0,
        LR_DEFAULTSIZE | LR_SHARED
    );
    if (!s_hAppIcon) {
        s_hAppIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    // Tipografia moderna Windows 11 (Segoe UI + Consolas para gatilhos)
    s_hFont = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );

    s_hFontBold = CreateFontW(
        -13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );

    s_hFontTitle = CreateFontW(
        -22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );

    s_hFontSmall = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );

    s_hFontMono = CreateFontW(
        -12, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
    );

    // Registra a classe da tabela customizada de snippets
    WNDCLASSW wcTable = {0};
    wcTable.style = CS_DBLCLKS;
    wcTable.lpfnWndProc = SnippetTableWndProc;
    wcTable.hInstance = hInstance;
    wcTable.lpszClassName = L"NimboxSnippetTableW";
    wcTable.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassW(&wcTable)) {
        return false;
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"NimboxMainWindowClassW";
    wc.hbrBackground = theme_get_window_brush();
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = s_hAppIcon;

    if (!RegisterClassW(&wc)) {
        return false;
    }

    int win_width = 880;
    int win_height = 620;

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_w - win_width) / 2;
    int y = (screen_h - win_height) / 2;

    s_hwndMain = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"NimboxMainWindowClassW",
        L"Nimbox — Gerenciador de Snippets",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y, win_width, win_height,
        NULL, NULL, hInstance, NULL
    );

    if (!s_hwndMain) {
        return false;
    }

    apply_window_decorations(s_hwndMain, p->is_dark);

    ShowWindow(s_hwndMain, nCmdShow);
    if (nCmdShow != SW_HIDE) {
        UpdateWindow(s_hwndMain);
    }
    return true;
}

void gui_show_window(bool show) {
    if (s_hwndMain) {
        if (show) {
            ShowWindow(s_hwndMain, IsIconic(s_hwndMain) ? SW_RESTORE : SW_SHOW);
            UpdateWindow(s_hwndMain);
            SetForegroundWindow(s_hwndMain);
        } else {
            ShowWindow(s_hwndMain, SW_HIDE);
        }
    }
}

bool gui_is_visible(void) {
    return s_hwndMain && IsWindowVisible(s_hwndMain);
}

HWND gui_get_main_hwnd(void) {
    return s_hwndMain;
}

bool gui_process_dialog_message(MSG *msg) {
    if (!msg || !s_hwndModal || !IsWindow(s_hwndModal)) {
        return false;
    }
    if (msg->hwnd == s_hwndModal || IsChild(s_hwndModal, msg->hwnd)) {
        return IsDialogMessageW(s_hwndModal, msg) != FALSE;
    }
    return false;
}
