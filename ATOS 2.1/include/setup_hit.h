#ifndef SETUP_HIT_H
#define SETUP_HIT_H
/* Matematyka trafien mysza w oknie SETTINGS.
 *
 * Wyodrebniona do naglowka CELOWO (wzorzec kgfx_planar4.h): ta sama funkcja
 * liczy trafienie w pulpicie i jest sprawdzana testem hostowym, wiec rysowanie
 * i hit-test nie moga sie rozjechac.
 *
 * Pulpit rysuje wiersze WZGLEDEM KLIENTA okna, a klient zaczyna sie 16 pikseli
 * ponizej gornej krawedzi okna (kernel_wm rysuje go od frame.y+16 - tyle ma
 * pasek tytulu).  Hit-test MUSI odjac to samo, inaczej kazdy wiersz trafia
 * o jeden w dol (audyt r2: "16 COLORS wskazuje na 256 COLORS").
 *
 *   window_y  - gorna krawedz OKNA (st_wy)
 *   title_h   - wysokosc paska tytulu (SET_TITLE_H)
 *   top       - pierwszy wiersz tresci wzgledem KLIENTA (SET_TOP)
 *   row_h     - wysokosc wiersza
 *   rows      - liczba wierszy
 * Zwraca numer wiersza albo rows, gdy klikniecie jest poza wierszami.
 */
static inline unsigned setup_row_hit(int my,int window_y,int title_h,int top,
                                     int row_h,unsigned rows)
{
    int sy=my-window_y-title_h-top;
    if(sy>=0&&(unsigned)sy<rows*(unsigned)row_h)
        return (unsigned)(sy/row_h);
    return rows;
}
/* Zakladki: obszar [tab_x, tab_x+tab_w) wzgledem KLIENTA, na wysokosci
   [tab_y, tab_y+tab_h) liczonej od KLIENTA (czyli od window_y+title_h).
   Zwraca numer zakladki albo 0xFF. */
static inline unsigned setup_tab_hit(int mx,int my,int window_x,int window_y,
                                     int title_h,int tab_x,int tab_y,int tab_w,
                                     int tab_h,int tabs)
{
    int sx=mx-window_x;
    int sy=my-window_y-title_h;
    if(sy<tab_y||sy>=tab_y+tab_h)
        return 0xFFu;
    if(sx>=tab_x&&sx<tab_x+tab_w)
        return 0u;
    if(tabs>1&&sx>=tab_x+114&&sx<tab_x+114+tab_w)
        return 1u;
    return 0xFFu;
}
#endif
