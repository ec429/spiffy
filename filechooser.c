/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	filechooser - shellout GTK filechooser
	
	Acknowledgements: Guesser (Alistair Cree) rewrote this to use gtk_file_chooser_dialog instead of _widget
*/

#include <stdio.h>
#include <stdbool.h>
#include <gtk/gtk.h>

int main(int argc, char *argv[])
{
	GtkWidget *dialog;
	gtk_init(&argc, &argv);
	
	dialog=gtk_file_chooser_dialog_new("Spiffy - Load file", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	
	if(gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT)
	{
		GFile *f=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
		printf("%s\n", g_file_get_path(f));
	}
	
	gtk_widget_destroy(dialog);
	return(0);
}
