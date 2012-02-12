/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	filechooser - shellout GTK filechooser
*/

#include <stdio.h>
#include <stdbool.h>
#include <gtk/gtk.h>

static void destroy(__attribute__((unused)) GtkWidget *widget, __attribute__((unused)) gpointer data)
{
	gtk_main_quit();
}

static void clicked(__attribute__((unused)) GtkWidget *widget, gpointer data)
{
	GFile *f=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(data));
	printf("%s\n", g_file_get_path(f));
	gtk_main_quit();
}

int main(int argc, char *argv[])
{
	gtk_init(&argc, &argv);
	GtkWidget *mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(mainwindow, "destroy", G_CALLBACK(destroy), NULL);
	gtk_window_set_title(GTK_WINDOW(mainwindow), "Spiffy - Load file");
	gtk_window_maximize(GTK_WINDOW(mainwindow));
	GtkWidget *box1 = gtk_vbox_new(false, 0);
	gtk_container_add(GTK_CONTAINER(mainwindow), box1);
	GtkWidget *chooser = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_box_pack_start(GTK_BOX(box1), chooser, true, true, 0);
	GtkWidget *openbutton = gtk_button_new_with_label("Load file");
	g_signal_connect(openbutton, "clicked", G_CALLBACK(clicked), chooser);
	gtk_box_pack_start(GTK_BOX(box1), openbutton, false, false, 0);
	gtk_widget_show_all(mainwindow);
	gtk_main();
	return(0);
}
