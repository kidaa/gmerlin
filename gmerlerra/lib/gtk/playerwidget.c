#include <gtk/gtk.h>

#include <config.h>
#include <gmerlin/utils.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/player.h>
#include <gmerlin/gui_gtk/gtkutils.h>
#include <gmerlin/gui_gtk/audio.h>
#include <gmerlin/gui_gtk/display.h>


#include <types.h>

#include <gui_gtk/timerange.h>
#include <gui_gtk/timeruler.h>
#include <gui_gtk/playerwidget.h>

struct bg_nle_player_widget_s
  {
  bg_player_t * player;

  GtkWidget * play_button;
  GtkWidget * stop_button;
  GtkWidget * goto_next_button;
  GtkWidget * goto_prev_button;
  GtkWidget * goto_start_button;
  GtkWidget * goto_end_button;
  GtkWidget * frame_forward_button;
  GtkWidget * frame_backward_button;
  GtkWidget * socket;
  
  GtkWidget * box;
  
  bg_plugin_registry_t * plugin_reg;
  
  bg_gtk_vumeter_t * vumeter;
  bg_gtk_time_display_t * display;

  bg_nle_time_ruler_t * ruler;
  bg_nle_time_ruler_t * ruler_priv;

  bg_nle_timerange_widget_t tr;
  };

static void button_callback(GtkWidget * w, gpointer data)
  {

  }

static GtkWidget * create_pixmap_button(bg_nle_player_widget_t * w,
                                        const char * filename,
                                        const char * tooltip)
  {
  GtkWidget * button;
  GtkWidget * image;
  char * path;
  path = bg_search_file_read("icons", filename);
  if(path)
    {
    image = gtk_image_new_from_file(path);
    free(path);
    }
  else
    image = gtk_image_new();

  gtk_widget_show(image);
  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(button), image);

  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(button_callback), w);
  
  gtk_widget_show(button);

  bg_gtk_tooltips_set_tip(button, tooltip, PACKAGE);
  
  return button;
  }

static float display_fg[] = { 0.0, 1.0, 0.0 };
static float display_bg[] = { 0.0, 0.0, 0.0 };

static void load_output_plugins(bg_nle_player_widget_t * p)
  {
  char * display_string;
  bg_plugin_handle_t * handle;
  const bg_plugin_info_t * info;
  GdkDisplay * dpy;

  dpy = gdk_display_get_default();
  
  info = bg_plugin_registry_get_default(p->plugin_reg,
                                        BG_PLUGIN_OUTPUT_VIDEO);

  display_string =
    bg_sprintf("%s:%08lx", gdk_display_get_name(dpy),
               (long unsigned int)gtk_socket_get_id(GTK_SOCKET(p->socket)));
  
  handle = bg_ov_plugin_load(p->plugin_reg, info, display_string);
  
  bg_player_set_ov_plugin(p->player, handle);

  info = bg_plugin_registry_get_default(p->plugin_reg,
                                        BG_PLUGIN_OUTPUT_AUDIO);
  handle = bg_plugin_load(p->plugin_reg, info);
  
  bg_player_set_oa_plugin(p->player, handle);
  
  free(display_string);
  }

static void socket_realize(GtkWidget * w, gpointer data)
  {
  bg_nle_player_widget_t * p = data;
  
  fprintf(stderr, "Socket realize\n");
  load_output_plugins(p);
  }

static void size_allocate_callback(GtkWidget     *widget,
                                   GtkAllocation *allocation,
                                   gpointer       user_data)
  {
  bg_nle_player_widget_t * w = user_data;
  bg_nle_timerange_widget_set_width(&w->tr, allocation->width);
  }

bg_nle_player_widget_t *
bg_nle_player_widget_create(bg_plugin_registry_t * plugin_reg,
                            bg_nle_time_ruler_t * ruler)
  {
  GtkWidget * box;
  bg_nle_player_widget_t * ret;
  bg_nle_time_range_t r;
  r.start = 0;
  r.end = 10 * GAVL_TIME_SCALE;
  
  ret = calloc(1, sizeof(*ret));
  ret->plugin_reg = plugin_reg;

  ret->tr.visible.start = 0;
  ret->tr.visible.end = GAVL_TIME_SCALE * 10;
  
  
  if(!ruler)
    {
    ret->ruler_priv = bg_nle_time_ruler_create(&ret->tr);
    
    bg_nle_time_ruler_update_visible(ret->ruler_priv);
    
    ret->ruler = ret->ruler_priv;
    }
  else
    ret->ruler = ruler;
    
  /* Create buttons */  
  ret->play_button = create_pixmap_button(ret, "gmerlerra/play.png", TRS("Play"));

  ret->stop_button = create_pixmap_button(ret, "gmerlerra/stop.png", TRS("Stop"));

  ret->goto_next_button = create_pixmap_button(ret, "gmerlerra/goto_next.png",
                                               TRS("Goto next label"));
  
  ret->goto_prev_button = create_pixmap_button(ret, "gmerlerra/goto_prev.png",
                                               TRS("Goto previous label"));
  
  ret->goto_start_button = create_pixmap_button(ret, "gmerlerra/goto_start.png",
                                                TRS("Goto start"));
  ret->goto_end_button = create_pixmap_button(ret, "gmerlerra/goto_end.png",
                                                TRS("Goto end"));

  ret->frame_forward_button = create_pixmap_button(ret, "gmerlerra/frame_forward.png",
                                                   TRS("1 frame forward"));
  ret->frame_backward_button = create_pixmap_button(ret, "gmerlerra/frame_backward.png",
                                                    TRS("1 frame backward"));

  /* Create socket */
  
  ret->socket = gtk_socket_new();

  g_signal_connect(G_OBJECT(ret->socket), "realize",
                   G_CALLBACK(socket_realize), ret);
  
  gtk_widget_show(ret->socket);
  
  ret->vumeter = bg_gtk_vumeter_create(2, 1);
  ret->display = bg_gtk_time_display_create(BG_GTK_DISPLAY_SIZE_SMALL, 4,
                                            BG_GTK_DISPLAY_MODE_TIMECODE |
                                            BG_GTK_DISPLAY_MODE_HMSMS);
  bg_gtk_time_display_set_colors(ret->display, display_fg, display_bg );
  //  bg_gtk_time_display_update(ret->display, 0, BG_GTK_DISPLAY_MODE_TIMECODE);
  bg_gtk_time_display_update(ret->display, 0, BG_GTK_DISPLAY_MODE_HMSMS);
  /* Pack */
  
  ret->box = gtk_vbox_new(FALSE, 0);

  box = gtk_hbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(box), ret->socket, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), bg_gtk_vumeter_get_widget(ret->vumeter),
                     FALSE, FALSE, 0);
  
  gtk_widget_show(box);

  gtk_box_pack_start(GTK_BOX(ret->box), box, TRUE, TRUE, 0);

  if(ret->ruler_priv)
    {
    g_signal_connect(ret->box, "size-allocate", G_CALLBACK(size_allocate_callback), ret);
    gtk_box_pack_start(GTK_BOX(ret->box), bg_nle_time_ruler_get_widget(ret->ruler_priv),
                       FALSE, FALSE, 0);
    }
  
  box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->goto_start_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->goto_prev_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->frame_backward_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->stop_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->play_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->frame_forward_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->goto_next_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ret->goto_end_button, FALSE, FALSE, 0);
  
  gtk_box_pack_end(GTK_BOX(box),
                   bg_gtk_time_display_get_widget(ret->display),
                   FALSE, FALSE, 0);

  gtk_widget_show(box);

  gtk_box_pack_start(GTK_BOX(ret->box), box, FALSE, FALSE, 0);

  gtk_widget_show(ret->box);

  ret->player = bg_player_create(ret->plugin_reg);
  
  return ret;
  }

void
bg_nle_player_widget_destroy(bg_nle_player_widget_t * w)
  {

  }

GtkWidget * bg_nle_player_widget_get_widget(bg_nle_player_widget_t * w)
  {
  return w->box;
  }