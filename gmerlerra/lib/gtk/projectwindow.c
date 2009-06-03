#include <stdlib.h>
#include <gtk/gtk.h>

#include <gmerlin/cfg_registry.h>

#include <config.h>

#include <track.h>
#include <project.h>

#include <gui_gtk/projectwindow.h>
#include <gui_gtk/timeline.h>

#include <gmerlin/gui_gtk/gtkutils.h>
#include <gmerlin/utils.h>
#include <gmerlin/cfg_dialog.h>

typedef struct
  {
  GtkWidget * load;
  GtkWidget * save;
  GtkWidget * settings;
  GtkWidget * close;
  GtkWidget * quit;
  GtkWidget * menu;
  } project_menu_t;

typedef struct
  {
  GtkWidget * add_audio;
  GtkWidget * add_video;
  GtkWidget * move_up;
  GtkWidget * move_down;
  GtkWidget * delete;
  GtkWidget * menu;
  } track_menu_t;

struct bg_nle_project_window_s
  {
  GtkWidget * win;
  GtkWidget * menubar;
  project_menu_t project_menu;
  track_menu_t track_menu;
  
  bg_nle_project_t * project;

  bg_nle_timeline_t * timeline;
  
  };

static void show_settings_dialog(bg_nle_project_window_t * win)
  {
  bg_dialog_t * cfg_dialog;
  cfg_dialog = bg_dialog_create_multi(TR("Project settings"));

  bg_dialog_add(cfg_dialog,
                TR("Audio"),
                win->project->audio_section,
                bg_nle_project_set_audio_parameter,
                NULL,
                win->project,
                bg_nle_project_get_audio_parameters());

  bg_dialog_add(cfg_dialog,
                TR("Video"),
                win->project->video_section,
                bg_nle_project_set_video_parameter,
                NULL,
                win->project,
                bg_nle_project_get_video_parameters());

  bg_dialog_show(cfg_dialog, win->win);
  
  }

static void menu_callback(GtkWidget * w, gpointer data)
  {
  bg_nle_project_window_t * win = data;
  bg_nle_track_t * track;
  
  if(w == win->project_menu.load)
    {
    
    }
  else if(w == win->project_menu.quit)
    {
    gtk_main_quit();
    }
  else if(w == win->project_menu.settings)
    {
    show_settings_dialog(win);
    }
  else if(w == win->track_menu.add_audio)
    {
    track = bg_nle_project_add_audio_track(win->project);
    bg_nle_timeline_add_track(win->timeline, track);
    }
  else if(w == win->track_menu.add_video)
    {
    track = bg_nle_project_add_video_track(win->project);
    bg_nle_timeline_add_track(win->timeline, track);
    }
  
  }

static GtkWidget *
create_menu_item(bg_nle_project_window_t * w, GtkWidget * parent,
                 const char * label, const char * pixmap)
  {
  GtkWidget * ret, *image;
  char * path;
  
  if(pixmap)
    {
    path = bg_search_file_read("icons", pixmap);
    if(path)
      {
      image = gtk_image_new_from_file(path);
      free(path);
      }
    else
      image = gtk_image_new();
    gtk_widget_show(image);
    ret = gtk_image_menu_item_new_with_label(label);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ret), image);
    }
  else
    {
    ret = gtk_menu_item_new_with_label(label);
    }
  
  g_signal_connect(G_OBJECT(ret), "activate", G_CALLBACK(menu_callback),
                   (gpointer)w);
  gtk_widget_show(ret);
  gtk_menu_shell_append(GTK_MENU_SHELL(parent), ret);
  return ret;
  }

static void init_menu_bar(bg_nle_project_window_t * w)
  {
  GtkWidget * item;
  /* Project */
  w->project_menu.menu = gtk_menu_new();
  w->project_menu.load =
    create_menu_item(w, w->project_menu.menu, TR("Open..."), "folder_open_16.png");
  w->project_menu.save =
    create_menu_item(w, w->project_menu.menu, TR("Save..."), "save_16.png");
  w->project_menu.settings =
    create_menu_item(w, w->project_menu.menu, TR("Settings..."), "config_16.png");
  w->project_menu.quit =
    create_menu_item(w, w->project_menu.menu, TR("Quit..."), "quit_16.png");
  
  gtk_widget_show(w->project_menu.menu);

  /* Track */
  w->track_menu.menu = gtk_menu_new();
  w->track_menu.add_audio =
    create_menu_item(w, w->track_menu.menu, TR("Add audio track"), "audio_16.png");
  w->track_menu.add_video =
    create_menu_item(w, w->track_menu.menu, TR("Add video track"), "video_16.png");
  w->track_menu.move_up =
    create_menu_item(w, w->track_menu.menu, TR("Move up"), "up_16.png");
  w->track_menu.move_down =
    create_menu_item(w, w->track_menu.menu, TR("Move down"), "down_16.png");
  w->track_menu.delete =
    create_menu_item(w, w->track_menu.menu, TR("Delete selected"), "trash_16.png");
  
  /* Menubar */
  w->menubar = gtk_menu_bar_new();

  item = gtk_menu_item_new_with_label(TR("Project"));
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), w->project_menu.menu);
  gtk_widget_show(item);
  gtk_menu_shell_append(GTK_MENU_SHELL(w->menubar), item);

  item = gtk_menu_item_new_with_label(TR("Track"));
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), w->track_menu.menu);
  gtk_widget_show(item);
  gtk_menu_shell_append(GTK_MENU_SHELL(w->menubar), item);

  
  gtk_widget_show(w->menubar);
  }

bg_nle_project_window_t *
bg_nle_project_window_create(const char * project_file)
  {
  GtkWidget * box;
  bg_nle_project_window_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->win = bg_gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(ret->win), 1024, 768);

  ret->project = bg_nle_project_create(project_file);

  ret->timeline = bg_nle_timeline_create();
  
  /* menubar */
  init_menu_bar(ret);

  /* Pack everything */
  box = gtk_vbox_new(0, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->menubar, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box),
                     bg_nle_timeline_get_widget(ret->timeline), TRUE, TRUE, 0);
  gtk_widget_show(box);

  gtk_container_add(GTK_CONTAINER(ret->win), box);
  
  return ret;
  }

void bg_nle_project_window_show(bg_nle_project_window_t * w)
  {
  gtk_widget_show(w->win);
  }
