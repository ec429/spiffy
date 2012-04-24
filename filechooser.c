/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	filechooser - shellout GTK filechooser
	
	Acknowledgements: Guesser (Alistair Cree) rewrote this to use gtk_file_chooser_dialog instead of _widget
*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <gtk/gtk.h>

int main(int argc, char *argv[])
{
	GtkWidget *dialog;
	gtk_init(&argc, &argv);
	
	bool save=false;
	const char *title=NULL;
	for(int arg=1;arg<argc;arg++)
	{
		if(strcmp(argv[arg], "--save")==0)
			save=true;
		else if(strcmp(argv[arg], "--load")==0)
			save=false;
		else if(strncmp(argv[arg], "--title=", 8)==0)
			title=argv[arg]+8;
	}
	
	dialog=gtk_file_chooser_dialog_new(title?title:save?"Spiffy - Save file":"Spiffy - Load file", NULL, save?GTK_FILE_CHOOSER_ACTION_SAVE:GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, save?GTK_STOCK_SAVE:GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	
	if(gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT)
	{
		GFile *f=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
		printf("+%s\n", g_file_get_path(f));
	}
	else
		printf("-\n");
	
	gtk_widget_destroy(dialog);
	return(0);
}
