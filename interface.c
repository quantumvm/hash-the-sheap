#include <cairo.h>
#include <gtk/gtk.h>

#include "interface.h"
#include "hashthesheap.h"


hash_tree_node * my_diff_tree = NULL;
int my_heap_size = 0;
int my_tree_height = 0;

static void draw_heap_diff(hash_tree_node * diff_tree, int tree_height, int heap_size, cairo_t * c, int width, int height){
    if(diff_tree == NULL){
        return;
    }
    
    draw_heap_diff(diff_tree->left,  tree_height-1, heap_size, c, width, height);
    draw_heap_diff(diff_tree->right, tree_height-1, heap_size, c, width, height);

    if(tree_height == 1){
        
        //calculate offset to draw block
        float heap_height_ratio =  ((float) height) / ((float) heap_size);
        int block_offset = (int) (heap_height_ratio * diff_tree->chunk_offset);
        
        //calculate the length of the block
        float percent_of_heap = ((float) diff_tree->chunk_size) / ((float) heap_size);
        int block_length = (int) (percent_of_heap * ((float) height)); 

        cairo_set_source_rgb(c, .1, .4, 0); 
        cairo_rectangle(c, 2, 2+(block_offset), width, block_length);
        cairo_fill(c); 
    }


}

static void draw_heap(cairo_t * c, GtkWidget * widget){
    GtkWidget *window = gtk_widget_get_toplevel(widget);
    
    int width, height;
    gtk_window_get_size(GTK_WINDOW(window), &width, &height);

    //cairo_set_line_width(c, 9);  
    //cairo_set_source_rgb(c, 0, 0, 0);
    

    //draw the base background for our heap graph
    int rect_width = width-4;
    int rect_height = height -4;

    cairo_set_source_rgb(c, 0.3, 0.4, 0.6); 
    cairo_rectangle(c, 2, 2, rect_width, rect_height);
    cairo_fill(c);

    //draw the heap diff images
    draw_heap_diff(my_diff_tree, my_tree_height, my_heap_size, c, rect_width, rect_height);
}


static int draw_event_handler(GtkWidget * widget, cairo_t * cairo, void * hash_tree){
    draw_heap(cairo, widget);
    return 0;
}


void draw_heap_visualization(hash_tree_node * diff_tree, int heap_size, int tree_height){
    GtkWidget * window;
    GtkWidget * drawing_area;
    
    my_diff_tree = diff_tree;
    my_heap_size = heap_size;
    my_tree_height = tree_height;

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
