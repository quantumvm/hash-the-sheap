#include <cairo.h>
#include <gtk/gtk.h>
#include "interface.h"

static void draw_heap(cairo_t * c, GtkWidget * widget){
    GtkWidget *window = gtk_widget_get_toplevel(widget);
    
    int width, height;
    gtk_window_get_size(GTK_WINDOW(window), &width, &height);

    cairo_set_line_width(c, 9);  
    cairo_set_source_rgb(c, 0.69, 0.19, 0);

    cairo_translate(c, width/2, height/2);
    cairo_arc(c, 0, 0, 50, 0, 2 * M_PI);
    cairo_stroke_preserve(c);

    cairo_set_source_rgb(c, 0.3, 0.4, 0.6); 
    cairo_fill(c); 
}


static int draw_event_handler(GtkWidget * widget, cairo_t * cairo, void * user_data){
    draw_heap(cairo, widget);
    return 0;
}


void draw_heap_visualization(){
    GtkWidget * window;
    GtkWidget * drawing_area;
    
    //we use NULL and NULL since we are using getopt
    gtk_init(NULL, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    
    drawing_area = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), drawing_area);
    
    g_signal_connect(G_OBJECT(drawing_area), 
                     "draw", 
                     G_CALLBACK(draw_event_handler),
                     NULL);
    
    g_signal_connect(G_OBJECT(window),
                     "destroy", 
                     G_CALLBACK(gtk_main_quit), 
                     NULL);
    
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 400);
    gtk_window_set_title(GTK_WINDOW(window), "Hashthesheap");
    
    gtk_widget_show_all(window);
    gtk_main();

}
